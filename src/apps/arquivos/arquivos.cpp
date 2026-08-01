// ============================================================================
//  Noir OS  -  Modulo "Arquivos" (cartao SD)
//
//  Quatro apps que trabalham sobre o cartao microSD do Cardputer:
//    1) Explorador  - navega pastas, abre/apaga/renomeia, cria pasta.
//    2) Editor      - edita um arquivo de texto pequeno (ate' 8 KB).
//    3) Notas       - abre direto /noir/notas.txt e faz autosave ao sair.
//    4) Imagens     - lista .jpg/.png/.bmp da raiz e navega com as setas.
//
//  IMPORTANTE (hardware): o Cardputer v1.1 NAO tem PSRAM. Por isso limitamos o
//  buffer de edicao a 8 KB (EDIT_LIMIT) e lemos arquivos byte a byte para uma
//  String. Nada de buffers gigantes.
//
//  SD do Cardputer: barramento SPI dedicado.
//    SCLK=40  MISO=39  MOSI=14  CS=12
//  Inicializamos com SPI.begin(40,39,14,12) seguido de SD.begin(12, SPI).
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
// IMPORTANTE: SPI/SD ANTES de qualquer header que puxe o M5GFX (ui/theme.h ->
// M5Cardputer.h). O M5GFX so' habilita o suporte a desenhar direto do SDFS
// (drawJpgFile/drawPngFile/drawBmpFile) se <SD.h> ja' tiver sido incluido.
#include <SPI.h>
#include <SD.h>

#include "apps/arquivos/arquivos.h"
#include "ui/theme.h"        // ui::gfx(), clearNoir(), present(), cores noir::*
#include "ui/statusbar.h"    // ui::statusBar()
#include "ui/widgets.h"      // messageBox, confirm, textInput, listView, banner...
#include "ui/input.h"        // ui::readKey(), waitKey(), Key, KeyEvent

#include <vector>
#include <algorithm>

namespace {

// ---------------------------------------------------------------------------
//  Constantes e helpers de caminho
// ---------------------------------------------------------------------------

// Sem PSRAM: teto do arquivo carregado na memoria para edicao/visualizacao.
constexpr size_t EDIT_LIMIT = 8 * 1024;  // 8 KB

// Largura em colunas ao usar a Font0 (6 px de largura) na tela de 240 px.
constexpr int TEXT_COLS = 40;

// Uma entrada listada de um diretorio.
struct Ent {
    String name;   // apenas o nome (sem o caminho da pasta)
    bool   dir;    // true se for diretorio
    size_t size;   // tamanho em bytes (arquivos)
};

// Extrai apenas o nome final de um caminho ("/noir/a.txt" -> "a.txt").
// Necessario porque no core ESP32 File::name() costuma devolver o caminho todo.
String baseName(const String& p) {
    int s = p.lastIndexOf('/');
    return (s < 0) ? p : p.substring(s + 1);
}

// Junta pasta + nome respeitando a raiz ("/" + "a" => "/a").
String joinPath(const String& dir, const String& name) {
    if (dir.length() == 0 || dir == "/") return "/" + name;
    return dir + "/" + name;
}

// Extensao em minusculas, sem o ponto ("Foto.JPG" -> "jpg").
String extOf(const String& n) {
    int d = n.lastIndexOf('.');
    if (d < 0) return "";
    String e = n.substring(d + 1);
    e.toLowerCase();
    return e;
}

// Tamanho legivel para humanos.
String humanSize(size_t b) {
    if (b < 1024)               return String((uint32_t)b) + "B";
    if (b < 1024UL * 1024UL)    return String((uint32_t)(b / 1024)) + "K";
    return String((uint32_t)(b / (1024UL * 1024UL))) + "M";
}

bool isTextExt(const String& ex) {
    return ex == "txt" || ex == "md"  || ex == "log" || ex == "ini" ||
           ex == "csv" || ex == "json"|| ex == "cfg" || ex == "conf";
}

bool isImageExt(const String& ex) {
    return ex == "jpg" || ex == "jpeg" || ex == "png" || ex == "bmp";
}

// ---------------------------------------------------------------------------
//  Cartao SD: inicializacao preguicosa
// ---------------------------------------------------------------------------

// Garante que o SD esteja pronto. O SPI so' precisa ser configurado uma vez;
// SD.begin() e' idempotente e re-monta o cartao (permite trocar o cartao).
// Retorna false se nao houver cartao -> o app avisa com messageBox.
bool ensureSD() {
    static bool spiStarted = false;
    if (!spiStarted) {
        SPI.begin(40, 39, 14, 12);   // SCLK, MISO, MOSI, CS
        spiStarted = true;
    }
    if (!SD.begin(12, SPI)) return false;
    return SD.cardType() != CARD_NONE;
}

// Aviso padrao quando nao ha' cartao.
void noCardMsg(const char* title) {
    ui::messageBox(title, "Sem cartao SD.\nInsira um cartao e\ntente novamente.");
}

// Le os itens de um diretorio para 'out' (limitado a 200 para nao estourar RAM).
void listDir(const String& path, std::vector<Ent>& out) {
    out.clear();
    File dir = SD.open(path);
    if (!dir) return;
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        Ent it;
        it.name = baseName(String(e.name()));
        it.dir  = e.isDirectory();
        it.size = e.size();
        e.close();
        out.push_back(it);
        if (out.size() >= 200) break;
    }
    dir.close();

    // Ordena: pastas primeiro, depois por nome (case-insensitive).
    std::sort(out.begin(), out.end(), [](const Ent& a, const Ent& b) {
        if (a.dir != b.dir) return a.dir > b.dir;  // dir(true) antes de file
        String x = a.name; x.toLowerCase();
        String y = b.name; y.toLowerCase();
        return x < y;
    });
}

// ---------------------------------------------------------------------------
//  Nucleo de texto: quebra em linhas, editor e visualizador
// ---------------------------------------------------------------------------

// Quebra o buffer em linhas de tela: respeita '\n' e faz wrap a cada 'cols'.
// (Ignora '\r' para tolerar arquivos com terminador CRLF do Windows.)
void wrapLines(const String& buf, int cols, std::vector<String>& out) {
    out.clear();
    String cur;
    for (size_t i = 0; i < buf.length(); ++i) {
        char c = buf[i];
        if (c == '\r') continue;
        if (c == '\n') { out.push_back(cur); cur = ""; continue; }
        cur += c;
        if ((int)cur.length() >= cols) { out.push_back(cur); cur = ""; }
    }
    out.push_back(cur);  // ultima linha (pode ser vazia = cursor em linha nova)
}

// Feedback rapido nao-destrutivo (o vermelho e' reservado a perigo/erro).
void toast(const String& msg) {
    ui::banner("", msg, "", false);
    delay(700);
}

// Editor de texto simples (edicao "append/backspace/enter" no fim do buffer).
//  - Lemos o teclado cru (M5Cardputer.Keyboard) igual ao ui::textInput, mas
//    aqui aceitamos varias linhas.
//  - A tela mostra sempre o FIM do texto (auto-scroll para o cursor).
//  - Teclas: caracteres inserem; DEL apaga; ENTER quebra linha; ` (crase) sai.
//  Edita 'buf' no lugar; quem chama decide se grava.
void editText(const char* title, String& buf) {
    M5Canvas& d = ui::gfx();
    const int top    = noir::STATUSBAR_H + 2;
    const int rowH   = 8;                       // altura da Font0
    const int footer = 9;
    const int rows   = (noir::SCREEN_H - top - footer) / rowH;

    auto render = [&]() {
        std::vector<String> lines;
        wrapLines(buf, TEXT_COLS, lines);
        ui::clearNoir();
        ui::statusBar(title);
        d.setFont(&fonts::Font0);
        d.setTextDatum(top_left);
        d.setTextColor(noir::BONE, noir::BLACK);
        int first = (int)lines.size() > rows ? (int)lines.size() - rows : 0;
        for (int i = 0; i < rows && (first + i) < (int)lines.size(); ++i) {
            String ln = lines[first + i];
            if (first + i == (int)lines.size() - 1) ln += "_";  // cursor no fim
            d.drawString(ln.c_str(), 2, top + i * rowH);
        }
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString("` sai   DEL apaga   ENTER linha",
                     noir::SCREEN_W / 2, noir::SCREEN_H - 1);
        ui::present();
    };
    render();

    for (;;) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto ks = M5Cardputer.Keyboard.keysState();
            if (ks.del) {
                if (buf.length() > 0) buf.remove(buf.length() - 1);
                render();
                continue;
            }
            bool changed = false, quit = false;
            if (ks.enter) { buf += '\n'; changed = true; }
            for (char c : ks.word) {
                if (c == '`') { quit = true; break; }     // crase = sair
                if (c == '\n' || c == '\r') continue;      // ja' tratado por ks.enter
                buf += c;
                changed = true;
            }
            if (quit) return;
            if (changed) render();
        }
        delay(8);
    }
}

// Visualizador de texto somente-leitura, com rolagem (setas). Usado para
// arquivos grandes (truncados) onde editar seria arriscado.
void viewText(const char* title, const String& buf) {
    M5Canvas& d = ui::gfx();
    std::vector<String> lines;
    wrapLines(buf, TEXT_COLS, lines);
    const int top    = noir::STATUSBAR_H + 2;
    const int rowH   = 8;
    const int footer = 9;
    const int rows   = (noir::SCREEN_H - top - footer) / rowH;
    const int maxOff = (int)lines.size() > rows ? (int)lines.size() - rows : 0;
    int off = 0;

    auto render = [&]() {
        ui::clearNoir();
        ui::statusBar(title);
        d.setFont(&fonts::Font0);
        d.setTextDatum(top_left);
        d.setTextColor(noir::BONE, noir::BLACK);
        for (int i = 0; i < rows && (off + i) < (int)lines.size(); ++i)
            d.drawString(lines[off + i].c_str(), 2, top + i * rowH);
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString("; sobe  . desce  ` sai", noir::SCREEN_W / 2, noir::SCREEN_H - 1);
        ui::present();
    };
    render();

    for (;;) {
        ui::KeyEvent e = ui::waitKey();
        if (e.key == ui::Key::Back) return;
        else if (e.key == ui::Key::Up   && off > 0)      { off--; render(); }
        else if (e.key == ui::Key::Down  && off < maxOff){ off++; render(); }
    }
}

// Le um arquivo (ate' EDIT_LIMIT bytes) para 'buf'. Retorna true se o arquivo
// foi lido por inteiro; false se foi truncado (maior que o limite).
bool loadFile(const String& path, String& buf) {
    buf = "";
    File f = SD.open(path, FILE_READ);
    if (!f) return true;               // arquivo inexistente = vazio (novo)
    size_t sz    = f.size();
    bool   trunc = sz > EDIT_LIMIT;
    size_t lim   = trunc ? EDIT_LIMIT : sz;
    buf.reserve(lim + 1);
    while (buf.length() < lim && f.available()) buf += (char)f.read();
    f.close();
    return !trunc;
}

// Grava 'buf' em 'path' (FILE_WRITE trunca o arquivo). Retorna sucesso.
bool saveFile(const String& path, const String& buf) {
    File w = SD.open(path, FILE_WRITE);
    if (!w) return false;
    w.print(buf);
    w.close();
    return true;
}

// Abre um arquivo de texto: se couber, edita e pergunta se grava; se for maior
// que o limite, mostra somente-leitura.
void openTextFile(const String& path) {
    String buf;
    bool whole = loadFile(path, buf);
    if (!whole) {
        File f = SD.open(path, FILE_READ);
        size_t sz = f ? f.size() : 0;
        if (f) f.close();
        ui::messageBox("Grande demais",
                       "Arquivo tem " + humanSize(sz) + ".\n"
                       "Mostrando " + humanSize(EDIT_LIMIT) + "\n(somente leitura).");
        viewText(baseName(path).c_str(), buf);
        return;
    }
    editText(baseName(path).c_str(), buf);
    if (ui::confirm("Salvar", "Gravar alteracoes em\n" + baseName(path) + "?")) {
        if (saveFile(path, buf)) toast("Salvo");
        else                     ui::redStripe("Falha ao salvar");
    }
}

// ---------------------------------------------------------------------------
//  Imagens
// ---------------------------------------------------------------------------

// Desenha o arquivo de imagem no canvas, abaixo da barra de status. O M5GFX
// decodifica direto do SD (streaming) - nao carregamos o arquivo em RAM.
bool drawImageFile(const String& path, const String& ex) {
    M5Canvas& d = ui::gfx();
    ui::clearNoir();
    const int y = noir::STATUSBAR_H;
    if (ex == "png")  return d.drawPngFile(SD, path.c_str(), 0, y);
    if (ex == "bmp")  return d.drawBmpFile(SD, path.c_str(), 0, y);
    return d.drawJpgFile(SD, path.c_str(), 0, y);   // jpg / jpeg
}

// ===========================================================================
//  APP 1  -  Explorador de arquivos
// ===========================================================================

// Menu de acoes para um arquivo selecionado (Apagar por ultimo = danger).
void fileActions(const String& cur, const Ent& en, const String& full) {
    std::vector<String> acts = {"Abrir", "Renomear", "Apagar"};
    int r = ui::listView(en.name.c_str(), acts, 0, /*dangerFrom=*/2);
    if (r < 0) return;

    if (r == 0) {                          // Abrir conforme a extensao
        String ex = extOf(en.name);
        if (isTextExt(ex))       openTextFile(full);
        else if (isImageExt(ex)) { if (!drawImageFile(full, ex)) ui::redStripe("Imagem invalida");
                                   else { ui::statusBar(en.name.c_str()); ui::present(); ui::waitKey(); } }
        else ui::messageBox("Arquivo", en.name + "\nTipo: " + (ex.length() ? ex : String("?")) +
                                       "\nTamanho: " + humanSize(en.size) + "\n\nSem visualizador.");
    } else if (r == 1) {                   // Renomear
        bool ok = true;
        String nm = ui::textInput("Novo nome", en.name, false, &ok);
        if (ok && nm.length()) {
            if (!SD.rename(full, joinPath(cur, nm))) ui::redStripe("Falha ao renomear");
        }
    } else if (r == 2) {                   // Apagar (perigo -> confirma)
        if (ui::confirm("Apagar", "Apagar definitivamente\n" + en.name + " ?", true)) {
            if (!SD.remove(full)) ui::redStripe("Falha ao apagar");
        }
    }
}

void appExplorer() {
    if (!ensureSD()) { noCardMsg("Explorador"); return; }

    String cur = "/";
    for (;;) {
        std::vector<Ent> ents;
        listDir(cur, ents);

        // Monta os itens do listView. Itens fixos no topo: nova pasta e "voltar".
        std::vector<String> items;
        items.push_back("[+ Nova pasta]");
        bool hasUp = (cur != "/");
        if (hasUp) items.push_back("[.. voltar]");
        const int base = hasUp ? 2 : 1;    // indice do 1o item real
        for (const auto& e : ents) {
            if (e.dir) items.push_back("/ " + e.name);
            else       items.push_back("  " + e.name + "   " + humanSize(e.size));
        }

        String title = "SD:" + cur;
        int sel = ui::listView(title.c_str(), items);
        if (sel < 0) return;               // crase = sair do app

        if (sel == 0) {                    // criar pasta
            bool ok = true;
            String nm = ui::textInput("Nova pasta", "", false, &ok);
            if (ok && nm.length() && !SD.mkdir(joinPath(cur, nm)))
                ui::redStripe("Falha ao criar");
            continue;
        }
        if (hasUp && sel == 1) {           // subir um nivel
            int s = cur.lastIndexOf('/');
            cur = (s <= 0) ? "/" : cur.substring(0, s);
            continue;
        }

        const Ent& en = ents[sel - base];
        String full = joinPath(cur, en.name);
        if (en.dir) { cur = full; continue; }  // entrar na pasta
        fileActions(cur, en, full);            // arquivo -> menu de acoes
    }
}

// ===========================================================================
//  APP 2  -  Editor de texto
// ===========================================================================

// Navegador minimalista para escolher um arquivo de texto a editar.
void editorBrowse() {
    String cur = "/";
    for (;;) {
        std::vector<Ent> ents;
        listDir(cur, ents);
        std::vector<String> items;
        bool hasUp = (cur != "/");
        if (hasUp) items.push_back("[.. voltar]");
        const int base = hasUp ? 1 : 0;
        for (const auto& e : ents) {
            if (e.dir) items.push_back("/ " + e.name);
            else       items.push_back("  " + e.name);
        }
        String title = "Abrir:" + cur;
        int sel = ui::listView(title.c_str(), items);
        if (sel < 0) return;
        if (hasUp && sel == 0) {
            int s = cur.lastIndexOf('/');
            cur = (s <= 0) ? "/" : cur.substring(0, s);
            continue;
        }
        const Ent& en = ents[sel - base];
        String full = joinPath(cur, en.name);
        if (en.dir) { cur = full; continue; }
        if (isTextExt(extOf(en.name))) { openTextFile(full); return; }
        ui::messageBox("Editor", en.name + "\nnao e' um arquivo\nde texto conhecido.");
    }
}

void appEditor() {
    if (!ensureSD()) { noCardMsg("Editor"); return; }

    int m = ui::listView("Editor de texto",
                         {"Novo arquivo...", "Abrir existente..."});
    if (m < 0) return;

    if (m == 0) {                          // criar arquivo novo em /noir
        if (!SD.exists("/noir")) SD.mkdir("/noir");
        bool ok = true;
        String nm = ui::textInput("Nome (em /noir)", "novo.txt", false, &ok);
        if (!ok || !nm.length()) return;
        String path = joinPath("/noir", nm);
        String buf;
        editText(baseName(path).c_str(), buf);
        if (saveFile(path, buf)) toast("Salvo");
        else                     ui::redStripe("Falha ao salvar");
    } else {
        editorBrowse();
    }
}

// ===========================================================================
//  APP 3  -  Notas rapidas (arquivo fixo, autosave)
// ===========================================================================

void appNotes() {
    if (!ensureSD()) { noCardMsg("Notas"); return; }

    if (!SD.exists("/noir")) SD.mkdir("/noir");   // garante a pasta
    const char* path = "/noir/notas.txt";

    String buf;
    loadFile(path, buf);                          // carrega o que ja' existe
    editText("Notas rapidas", buf);               // edita
    // Autosave: gravamos sempre ao sair, sem perguntar.
    if (saveFile(path, buf)) toast("Notas salvas");
    else                     ui::redStripe("Falha ao salvar");
}

// ===========================================================================
//  APP 4  -  Visualizador de imagens
// ===========================================================================

void appImages() {
    if (!ensureSD()) { noCardMsg("Imagens"); return; }

    // Coleta as imagens da raiz.
    const String dir = "/";
    std::vector<String> imgs;
    {
        std::vector<Ent> ents;
        listDir(dir, ents);
        for (const auto& e : ents) {
            if (e.dir) continue;
            if (isImageExt(extOf(e.name))) imgs.push_back(joinPath(dir, e.name));
        }
    }
    if (imgs.empty()) {
        ui::messageBox("Imagens", "Nenhuma imagem na raiz.\n(.jpg .png .bmp)");
        return;
    }

    int idx = 0;
    auto show = [&]() {
        String p  = imgs[idx];
        String ex = extOf(p);
        bool ok = drawImageFile(p, ex);
        M5Canvas& d = ui::gfx();
        if (!ok) {
            ui::clearNoir();
            d.setFont(&fonts::Font2);
            d.setTextDatum(middle_center);
            d.setTextColor(noir::BLOOD, noir::BLACK);
            d.drawString("Falha ao decodificar", noir::SCREEN_W / 2, noir::SCREEN_H / 2);
        }
        // Barra e rodape por cima da imagem.
        String t = "Img " + String(idx + 1) + "/" + String((int)imgs.size());
        ui::statusBar(t.c_str());
        d.setFont(&fonts::Font0);
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString(baseName(imgs[idx]).c_str(), noir::SCREEN_W / 2, noir::SCREEN_H - 1);
        ui::present();
    };
    show();

    for (;;) {
        ui::KeyEvent e = ui::waitKey();
        if (e.key == ui::Key::Back) return;
        else if (e.key == ui::Key::Left  || e.key == ui::Key::Up)
            { idx = (idx - 1 + (int)imgs.size()) % (int)imgs.size(); show(); }
        else if (e.key == ui::Key::Right || e.key == ui::Key::Down)
            { idx = (idx + 1) % (int)imgs.size(); show(); }
    }
}

} // namespace (anonimo)

// ---------------------------------------------------------------------------
//  Exportacao do modulo (contrato do app_registry)
// ---------------------------------------------------------------------------
namespace apps {
namespace arquivos {

// Nenhum app aqui e' de "perigo TX", mas o Explorador e o Editor podem apagar/
// sobrescrever arquivos. Essas acoes destrutivas ja' sao protegidas por
// ui::confirm(...) interno (Apagar em vermelho), entao os apps ficam com
// danger=false na barra. (Regra: TX ativo/destrutivo por ultimo; aqui nao ha' TX.)
const noir::AppEntry ARQUIVOS_APPS[] = {
    {"Explorador",   "sd",    appExplorer, false},
    {"Editor texto", "txt",   appEditor,   false},
    {"Notas",        "nota",  appNotes,    false},
    {"Imagens",      "foto",  appImages,   false},
};
const int ARQUIVOS_APPS_COUNT = sizeof(ARQUIVOS_APPS) / sizeof(ARQUIVOS_APPS[0]);

} // namespace arquivos
} // namespace apps
