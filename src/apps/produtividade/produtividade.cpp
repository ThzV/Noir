// ============================================================================
//  Noir OS  -  Categoria "Produtividade" (offline)
//
//  Implementacao dos 4 apps da categoria. Tudo em um unico arquivo de traducao
//  (translation unit) porque o array PRODUTIVIDADE_APPS precisa enxergar as
//  funcoes run(), que sao intencionalmente arquivo-locais (namespace anonimo).
//
//  Padrao de app do Noir: uma funcao void() bloqueante que roda seu proprio
//  laco desenhando no canvas compartilhado (ui::gfx) e retorna quando o usuario
//  sai. Reaproveitamos os widgets (listView, textInput, messageBox, ...) sempre
//  que da, e so desenhamos "na mao" quando a tela e' especial (cronometro,
//  pomodoro e calendario tem telas proprias).
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#include "apps/produtividade/produtividade.h"

#include "ui/theme.h"       // ui::gfx(), clearNoir(), present(), panel(), cores noir::*
#include "ui/statusbar.h"   // ui::statusBar()
#include "ui/widgets.h"     // listView, textInput, messageBox, banner, progress...
#include "ui/input.h"       // readKey(), waitKey(), Key
#include "core/config.h"    // NVS (config do pomodoro)
#include "core/time_service.h" // hora/data para o calendario

#include <M5Cardputer.h>    // M5.Speaker.tone() (beep do pomodoro)
#include <vector>
#include <cstdio>
#include <cstdlib>   // atof()
#include <cmath>
#include <ctime>

namespace {  // ======================= NAMESPACE ANONIMO ========================
// Tudo aqui dentro tem ligacao interna (so existe neste arquivo). E' onde ficam
// as funcoes run() dos apps e seus utilitarios.

using ui::gfx;
using ui::clearNoir;
using ui::Key;
using ui::KeyEvent;

// ----------------------------------------------------------------------------
//  Utilitarios de desenho comuns
// ----------------------------------------------------------------------------

// Rodape com dicas de tecla (fonte pequena, cor discreta). Chame antes do
// present() da sua tela para nao redesenhar o fundo.
void drawHints(const char* hints) {
    M5Canvas& d = gfx();
    d.setFont(&fonts::Font0);
    d.setTextDatum(bottom_center);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString(hints, noir::SCREEN_W / 2, noir::SCREEN_H - 2);
}

// Desenha um relogio "MM:SS" grande (fonte 7-segmentos, Font7) e centesimos
// ".cc" menores logo ao lado. Como as larguras variam, medimos com textWidth()
// e centralizamos o conjunto -> nunca corta nas bordas de 240px.
//   cy = centro vertical do bloco.
void drawBigClock(const String& mmss, const String& cc, int cy,
                  uint16_t mainColor = noir::WHITE) {
    M5Canvas& d = gfx();
    d.setTextDatum(middle_left);

    d.setFont(&fonts::Font7);
    int wMain = d.textWidth(mmss.c_str());

    String tail = "." + cc;
    d.setFont(&fonts::Font4);
    int wTail = cc.length() ? d.textWidth(tail.c_str()) : 0;

    int x0 = (noir::SCREEN_W - (wMain + wTail)) / 2;
    if (x0 < 2) x0 = 2;

    d.setFont(&fonts::Font7);
    d.setTextColor(mainColor, noir::BLACK);
    d.drawString(mmss.c_str(), x0, cy);

    if (cc.length()) {
        d.setFont(&fonts::Font4);
        d.setTextColor(noir::STEEL, noir::BLACK);
        // desce um pouco a base dos centesimos p/ alinhar com a "linha de base"
        d.drawString(tail.c_str(), x0 + wMain + 2, cy + 8);
    }
}

// Formata milissegundos em componentes MM, SS, cc (centesimos).
void splitTime(uint32_t ms, char* mmss, size_t mmssN, char* cc, size_t ccN) {
    uint32_t totalCs = ms / 10;          // centesimos totais
    uint32_t c  = totalCs % 100;
    uint32_t s  = (totalCs / 100) % 60;
    uint32_t m  = (totalCs / 100) / 60;  // pode passar de 99 em sessoes longas
    if (m > 99) m = 99;                  // trava visual (2 digitos)
    std::snprintf(mmss, mmssN, "%02u:%02u", (unsigned)m, (unsigned)s);
    std::snprintf(cc,   ccN,   "%02u",      (unsigned)c);
}

// ============================================================================
//  APP 1  -  CRONOMETRO
//  Base: millis(). Guardamos o tempo "acumulado" (accum) e, enquanto rodando,
//  somamos (millis() - startMark). Assim start/stop nao perde fracoes de tempo.
//  Voltas (laps) guardam o tempo decorrido no instante da marcacao.
// ============================================================================
void appCronometro() {
    M5Canvas& d = gfx();

    bool     running   = false;
    uint32_t accum     = 0;        // tempo congelado (ms) quando pausado
    uint32_t startMark = 0;        // millis() no ultimo "start"
    std::vector<uint32_t> laps;    // tempo decorrido total em cada volta
    int      scroll    = 0;        // 1a volta visivel na lista

    auto elapsed = [&]() -> uint32_t {
        return running ? accum + (millis() - startMark) : accum;
    };

    // Layout da lista de voltas (parte de baixo da tela).
    const int listTop = 74;
    const int rowH    = 14;
    const int visible = (noir::SCREEN_H - listTop - 10) / rowH;  // linhas cabendo

    auto draw = [&]() {
        clearNoir();
        ui::statusBar("Cronometro");

        // --- Tempo grande MM:SS.cc ---
        char mmss[8], cc[4];
        splitTime(elapsed(), mmss, sizeof(mmss), cc, sizeof(cc));
        // Cor: branco quando rodando, "osso" quando parado (sutil).
        drawBigClock(mmss, cc, 44, running ? noir::WHITE : noir::BONE);

        // --- Cabecalho da lista de voltas ---
        d.setFont(&fonts::Font0);
        d.setTextDatum(top_left);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString(laps.empty() ? "sem voltas" : "voltas:", 8, listTop - 12);

        // --- Voltas visiveis (mais recentes rolam para dentro) ---
        d.setFont(&fonts::Font2);
        for (int i = 0; i < visible && (scroll + i) < (int)laps.size(); i++) {
            int idx = scroll + i;
            int y   = listTop + i * rowH;
            uint32_t prev = (idx == 0) ? 0 : laps[idx - 1];
            uint32_t delta = laps[idx] - prev;      // duracao desta volta
            char dmm[8], dcc[4], tmm[8], tcc[4];
            splitTime(delta,     dmm, sizeof(dmm), dcc, sizeof(dcc));
            splitTime(laps[idx], tmm, sizeof(tmm), tcc, sizeof(tcc));
            char line[40];
            std::snprintf(line, sizeof(line), "%2d  +%s.%s   %s.%s",
                          idx + 1, dmm, dcc, tmm, tcc);
            d.setTextDatum(top_left);
            d.setTextColor(noir::BONE, noir::BLACK);
            d.drawString(line, 8, y);
        }

        // Barra de rolagem, quando ha mais voltas do que cabem.
        if ((int)laps.size() > visible && visible > 0) {
            int trackH = visible * rowH;
            int barH = visible * trackH / (int)laps.size(); if (barH < 6) barH = 6;
            int barY = listTop + scroll * trackH / (int)laps.size();
            d.fillRect(noir::SCREEN_W - 3, listTop, 2, trackH, noir::INK);
            d.fillRect(noir::SCREEN_W - 3, barY, 2, barH, noir::STEEL);
        }

        drawHints(running ? "ENTER pausa  SPACE volta  ` sai"
                          : "ENTER inicia  DEL zera  ` sai");
        ui::present();
    };

    for (;;) {
        // Enquanto roda, redesenhamos continuamente (readKey nao bloqueia).
        draw();

        KeyEvent e = ui::readKey();
        switch (e.key) {
            case Key::Enter:
                if (running) { accum = elapsed(); running = false; }   // pausa
                else         { startMark = millis(); running = true; } // (re)inicia
                break;

            case Key::Space:                 // marca volta (so faz sentido rodando)
                if (running) {
                    laps.push_back(elapsed());
                    // auto-rola para mostrar a volta recem-criada
                    if ((int)laps.size() > visible)
                        scroll = (int)laps.size() - visible;
                }
                break;

            case Key::Del:                   // zera (apenas quando parado)
                if (!running) { accum = 0; laps.clear(); scroll = 0; }
                break;

            case Key::Up:                    // rola a lista de voltas
                if (scroll > 0) scroll--;
                break;
            case Key::Down:
                if (scroll + visible < (int)laps.size()) scroll++;
                break;

            case Key::Back:
                return;
            default:
                break;
        }

        // Cadencia ~50fps quando rodando; economiza CPU quando parado.
        delay(running ? 20 : 40);
    }
}

// ============================================================================
//  APP 2  -  CONVERSOR DE UNIDADES
//  Estrategia: cada unidade "normal" tem um FATOR ate' a unidade-base da sua
//  categoria (valor_base = valor * fator). Converter = valor_base / fator_dest.
//  Temperatura foge da regra (offsets), entao tratamos com formulas.
// ============================================================================

struct Unidade { const char* nome; double fator; };  // fator -> unidade-base
struct Categoria { const char* nome; const Unidade* uns; int n; bool temperatura; };

// --- Tabelas de fatores (base entre parenteses) ---
const Unidade U_COMP[] = {  // base: metro
    {"mm", 0.001}, {"cm", 0.01}, {"m", 1.0}, {"km", 1000.0},
    {"pol", 0.0254}, {"pe", 0.3048}, {"milha", 1609.344},
};
const Unidade U_MASSA[] = { // base: grama
    {"mg", 0.001}, {"g", 1.0}, {"kg", 1000.0}, {"ton", 1000000.0},
    {"lb", 453.59237}, {"oz", 28.349523},
};
const Unidade U_TEMP[] = {  // formulas especiais (fator ignorado)
    {"C", 0}, {"F", 0}, {"K", 0},
};
const Unidade U_VOL[] = {   // base: litro
    {"ml", 0.001}, {"L", 1.0}, {"m3", 1000.0},
    {"gal", 3.785411}, {"xicara", 0.236588},
};
const Unidade U_DADOS[] = { // base: byte (binario: 1KB = 1024B)
    {"bit", 0.125}, {"B", 1.0}, {"KB", 1024.0}, {"MB", 1048576.0},
    {"GB", 1073741824.0}, {"TB", 1099511627776.0},
};
const Unidade U_VEL[] = {   // base: m/s
    {"m/s", 1.0}, {"km/h", 0.2777778}, {"mph", 0.44704},
    {"no", 0.5144444}, {"pe/s", 0.3048},
};

const Categoria CATS[] = {
    {"Comprimento", U_COMP,  (int)(sizeof(U_COMP)/sizeof(Unidade)),  false},
    {"Massa",       U_MASSA, (int)(sizeof(U_MASSA)/sizeof(Unidade)), false},
    {"Temperatura", U_TEMP,  (int)(sizeof(U_TEMP)/sizeof(Unidade)),  true},
    {"Volume",      U_VOL,   (int)(sizeof(U_VOL)/sizeof(Unidade)),   false},
    {"Dados",       U_DADOS, (int)(sizeof(U_DADOS)/sizeof(Unidade)), false},
    {"Velocidade",  U_VEL,   (int)(sizeof(U_VEL)/sizeof(Unidade)),   false},
};
const int CATS_N = (int)(sizeof(CATS)/sizeof(Categoria));

// Converte temperatura de/para Celsius como pivo.
double tempParaC(double v, const char* u) {
    if (u[0] == 'F') return (v - 32.0) * 5.0 / 9.0;
    if (u[0] == 'K') return v - 273.15;
    return v;                       // ja' e' Celsius
}
double tempDeC(double c, const char* u) {
    if (u[0] == 'F') return c * 9.0 / 5.0 + 32.0;
    if (u[0] == 'K') return c + 273.15;
    return c;
}

// Le um numero com o textInput. Aceita virgula ou ponto como separador decimal.
// Retorna false se cancelado ou vazio.
bool lerNumero(const char* titulo, double& out) {
    bool ok = true;
    String s = ui::textInput(titulo, "", false, &ok);
    if (!ok) return false;
    s.trim();
    if (s.length() == 0) return false;
    s.replace(",", ".");            // brasileiro escreve "1,5"
    out = atof(s.c_str());
    return true;
}

// Formata um resultado double de forma legivel (sem casas inuteis).
String fmtNum(double v) {
    char b[32];
    double a = std::fabs(v);
    if (a != 0 && (a < 0.001 || a >= 1e7)) std::snprintf(b, sizeof(b), "%.4g", v);
    else                                   std::snprintf(b, sizeof(b), "%.4f", v);
    String s(b);
    // remove zeros a' direita (e o ponto se sobrar) para "12.5000" -> "12.5"
    if (s.indexOf('.') >= 0 && s.indexOf('e') < 0 && s.indexOf('E') < 0) {
        while (s.endsWith("0")) s.remove(s.length() - 1);
        if (s.endsWith(".")) s.remove(s.length() - 1);
    }
    return s;
}

void appConversor() {
    // 1) Categoria
    std::vector<String> catItems;
    for (int i = 0; i < CATS_N; i++) catItems.push_back(CATS[i].nome);
    int ci = ui::listView("Conversor", catItems);
    if (ci < 0) return;
    const Categoria& cat = CATS[ci];

    // 2) Unidade de origem
    std::vector<String> unItems;
    for (int i = 0; i < cat.n; i++) unItems.push_back(cat.uns[i].nome);
    int fromI = ui::listView("De qual unidade", unItems);
    if (fromI < 0) return;

    // 3) Unidade de destino
    int toI = ui::listView("Para qual unidade", unItems, fromI);
    if (toI < 0) return;

    // 4) Valor
    double val;
    String t = String("Valor (") + cat.uns[fromI].nome + ")";
    if (!lerNumero(t.c_str(), val)) return;

    // 5) Converte
    double res;
    if (cat.temperatura) {
        double c = tempParaC(val, cat.uns[fromI].nome);
        res = tempDeC(c, cat.uns[toI].nome);
    } else {
        double base = val * cat.uns[fromI].fator;
        res = base / cat.uns[toI].fator;
    }

    // 6) Resultado (banner grande + linha de origem)
    String big = fmtNum(res) + " " + cat.uns[toI].nome;
    String sub = fmtNum(val) + " " + cat.uns[fromI].nome + "  =";
    ui::banner("Resultado", big, sub);
}

// ============================================================================
//  APP 3  -  POMODORO
//  Ciclos de foco/pausa. Duracoes (min) salvas na NVS. Beep na transicao.
//  Fases: FOCO -> PAUSA_CURTA (repete) -> a cada N focos, PAUSA_LONGA.
// ============================================================================

// Chaves NVS (<= 15 chars!). Guardamos minutos e o intervalo do descanso longo.
constexpr const char* K_WORK  = "pm_work";   // minutos de foco
constexpr const char* K_SHORT = "pm_short";  // minutos de pausa curta
constexpr const char* K_LONG  = "pm_long";   // minutos de pausa longa
constexpr const char* K_EVERY = "pm_every";  // focos por pausa longa

enum class Fase { Foco, PausaCurta, PausaLonga };

// Beep curto e nao-bloqueante via alto-falante do Cardputer.
void beep(float freq, uint32_t ms) {
    M5.Speaker.setVolume(180);
    M5.Speaker.tone(freq, ms);
}

// Tela de configuracao: ajusta os 4 parametros com as setas.
void pomodoroConfig() {
    struct Item { const char* nome; const char* key; int def, mn, mx, passo; const char* un; };
    Item its[] = {
        {"Foco",         K_WORK,  25, 1, 90, 1, "min"},
        {"Pausa curta",  K_SHORT,  5, 1, 30, 1, "min"},
        {"Pausa longa",  K_LONG,  15, 1, 60, 1, "min"},
        {"Focos p/ longa", K_EVERY, 4, 2,  8, 1, "x"},
    };
    const int N = 4;
    int vals[N];
    for (int i = 0; i < N; i++) vals[i] = noir::config::getInt(its[i].key, its[i].def);

    int sel = 0;
    M5Canvas& d = gfx();
    for (;;) {
        clearNoir();
        ui::statusBar("Pomodoro - Config");
        d.setFont(&fonts::Font2);
        for (int i = 0; i < N; i++) {
            int y = noir::STATUSBAR_H + 8 + i * 22;
            bool on = (i == sel);
            if (on) {
                d.fillRect(6, y - 2, noir::SCREEN_W - 12, 20, noir::WHITE);
                d.setTextColor(noir::BLACK, noir::WHITE);
            } else {
                d.setTextColor(noir::BONE, noir::BLACK);
            }
            d.setTextDatum(middle_left);
            d.drawString(its[i].nome, 12, y + 8);
            char v[16];
            std::snprintf(v, sizeof(v), "< %d %s >", vals[i], its[i].un);
            d.setTextDatum(middle_right);
            d.drawString(v, noir::SCREEN_W - 12, y + 8);
        }
        drawHints("; . escolhe  , / ajusta  ENTER salva  ` sai");
        ui::present();

        KeyEvent e = ui::waitKey();
        if (e.key == Key::Up)    sel = (sel - 1 + N) % N;
        else if (e.key == Key::Down)  sel = (sel + 1) % N;
        else if (e.key == Key::Left)  { vals[sel] -= its[sel].passo; if (vals[sel] < its[sel].mn) vals[sel] = its[sel].mn; }
        else if (e.key == Key::Right) { vals[sel] += its[sel].passo; if (vals[sel] > its[sel].mx) vals[sel] = its[sel].mx; }
        else if (e.key == Key::Enter) {
            for (int i = 0; i < N; i++) noir::config::setInt(its[i].key, vals[i]);
            ui::redStripe("Config salva", 800);
            return;
        }
        else if (e.key == Key::Back) return;
    }
}

// Laco de contagem de uma sessao pomodoro completa.
void pomodoroRun() {
    const int workMin  = noir::config::getInt(K_WORK,  25);
    const int shortMin = noir::config::getInt(K_SHORT,  5);
    const int longMin  = noir::config::getInt(K_LONG,  15);
    const int every    = noir::config::getInt(K_EVERY,  4);

    auto duracaoDe = [&](Fase f) -> uint32_t {
        switch (f) {
            case Fase::Foco:       return (uint32_t)workMin  * 60;
            case Fase::PausaCurta: return (uint32_t)shortMin * 60;
            default:               return (uint32_t)longMin  * 60;
        }
    };
    auto nomeDe = [&](Fase f) -> const char* {
        switch (f) {
            case Fase::Foco:       return "FOCO";
            case Fase::PausaCurta: return "PAUSA";
            default:               return "PAUSA LONGA";
        }
    };

    Fase     fase      = Fase::Foco;
    uint32_t totalS    = duracaoDe(fase);
    uint32_t restanteS = totalS;
    bool     running   = true;
    int      pomos     = 0;                 // focos concluidos
    uint32_t lastTick  = millis();

    M5Canvas& d = gfx();

    // Avanca para a proxima fase (com beep e "flash" da tela).
    auto proximaFase = [&]() {
        if (fase == Fase::Foco) {
            pomos++;
            fase = (pomos % every == 0) ? Fase::PausaLonga : Fase::PausaCurta;
            beep(880, 140); delay(150); beep(1175, 220);   // "subida" ao descansar
        } else {
            fase = Fase::Foco;
            beep(1175, 140); delay(150); beep(784, 220);    // "descida" ao focar
        }
        totalS    = duracaoDe(fase);
        restanteS = totalS;
        // Flash rapido para chamar atencao sem depender so do som.
        d.fillSprite(noir::WHITE); ui::present(); delay(60);
    };

    for (;;) {
        // Tick de 1 segundo (independente do fps de desenho).
        uint32_t nowMs = millis();
        if (running && nowMs - lastTick >= 1000) {
            lastTick += 1000;
            if (restanteS > 0) restanteS--;
            if (restanteS == 0) proximaFase();
        }

        // --- Desenho ---
        clearNoir();
        ui::statusBar("Pomodoro");

        // Nome da fase
        d.setFont(&fonts::Font4);
        d.setTextDatum(middle_center);
        d.setTextColor(fase == Fase::Foco ? noir::WHITE : noir::STEEL, noir::BLACK);
        d.drawString(nomeDe(fase), noir::SCREEN_W / 2, 32);

        // Contagem MM:SS grande (sem centesimos aqui)
        char mmss[8];
        std::snprintf(mmss, sizeof(mmss), "%02u:%02u",
                      (unsigned)(restanteS / 60), (unsigned)(restanteS % 60));
        drawBigClock(String(mmss), "", 66,
                     fase == Fase::Foco ? noir::WHITE : noir::BONE);

        // Barra de progresso da fase atual
        int pct = totalS ? (int)((totalS - restanteS) * 100 / totalS) : 0;
        const int bx = 20, by = 96, bw = noir::SCREEN_W - 40, bh = 10;
        d.drawRect(bx, by, bw, bh, noir::BONE);
        d.fillRect(bx + 2, by + 2, (bw - 4) * pct / 100, bh - 4, noir::WHITE);

        // Contador de pomodoros
        d.setFont(&fonts::Font0);
        d.setTextDatum(top_left);
        d.setTextColor(noir::STEEL, noir::BLACK);
        char cnt[24];
        std::snprintf(cnt, sizeof(cnt), "pomodoros: %d", pomos);
        d.drawString(cnt, 8, 114);

        drawHints(running ? "ENTER pausa  SPACE pula  ` sai"
                          : "ENTER retoma  SPACE pula  ` sai");
        ui::present();

        // --- Teclado (nao bloqueante) ---
        KeyEvent e = ui::readKey();
        if (e.key == Key::Enter) { running = !running; lastTick = millis(); }
        else if (e.key == Key::Space) proximaFase();
        else if (e.key == Key::Back)  { M5.Speaker.stop(); return; }

        delay(30);
    }
}

void appPomodoro() {
    for (;;) {
        int workMin  = noir::config::getInt(K_WORK,  25);
        int shortMin = noir::config::getInt(K_SHORT,  5);
        int longMin  = noir::config::getInt(K_LONG,  15);
        char resumo[40];
        std::snprintf(resumo, sizeof(resumo), "Iniciar (%d/%d/%d)",
                      workMin, shortMin, longMin);
        std::vector<String> menu = { resumo, "Configurar", "Voltar" };
        int r = ui::listView("Pomodoro", menu);
        if (r == 0)      pomodoroRun();
        else if (r == 1) pomodoroConfig();
        else             return;   // "Voltar" ou Back
    }
}

// ============================================================================
//  APP 4  -  CALENDARIO
//  Grade do mes em 7 colunas. O dia da semana do dia 1 vem de mktime()/struct
//  tm. Precisa de NTP so' para saber que dia e' "hoje" (destaque); a navegacao
//  do mes funciona sem rede.
// ============================================================================

const char* MESES[] = {
    "Janeiro","Fevereiro","Marco","Abril","Maio","Junho",
    "Julho","Agosto","Setembro","Outubro","Novembro","Dezembro"
};

bool bissexto(int ano) {
    return (ano % 4 == 0 && ano % 100 != 0) || (ano % 400 == 0);
}
int diasNoMes(int ano, int mes /*0-11*/) {
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (mes == 1 && bissexto(ano)) return 29;
    return dm[mes];
}
// Dia da semana (0=Dom..6=Sab) do dia 1 do mes, via mktime (normaliza tm_wday).
int diaSemanaDia1(int ano, int mes /*0-11*/) {
    struct tm t = {};
    t.tm_year = ano - 1900;
    t.tm_mon  = mes;
    t.tm_mday = 1;
    t.tm_hour = 12;          // meio-dia evita ambiguidades de horario de verao
    t.tm_isdst = -1;
    mktime(&t);              // preenche tm_wday
    return t.tm_wday;
}

void appCalendario() {
    // Ponto de partida: hoje (se houver NTP). Sem hora, avisamos e saimos.
    struct tm agora;
    bool temHora = noir::timeservice::now(agora);
    if (!temHora) {
        ui::messageBox("Calendario",
            "Sem hora ainda.\nConecte o WiFi e sincronize\n(Config > WiFi) para ver\no mes atual.");
        return;
    }

    int ano  = agora.tm_year + 1900;
    int mes  = agora.tm_mon;                 // 0-11
    int hojeD = agora.tm_mday;
    int hojeM = agora.tm_mon;
    int hojeA = agora.tm_year + 1900;

    M5Canvas& d = gfx();
    const char* DOW[7] = {"D","S","T","Q","Q","S","S"};  // Dom..Sab

    for (;;) {
        clearNoir();
        char titulo[24];
        std::snprintf(titulo, sizeof(titulo), "%s %d", MESES[mes], ano);
        ui::statusBar(titulo);

        // Geometria da grade
        const int cols = 7;
        const int cellW = 32;
        const int gridX = (noir::SCREEN_W - cols * cellW) / 2;   // centraliza (~8)
        const int headY = noir::STATUSBAR_H + 4;
        const int gridY = headY + 14;
        const int cellH = 16;

        // Cabecalho dos dias da semana
        d.setFont(&fonts::Font2);
        d.setTextDatum(middle_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        for (int c = 0; c < cols; c++)
            d.drawString(DOW[c], gridX + c * cellW + cellW / 2, headY + 6);

        // Dias
        int primeiro = diaSemanaDia1(ano, mes);   // coluna do dia 1
        int nd = diasNoMes(ano, mes);
        for (int dia = 1; dia <= nd; dia++) {
            int celula = primeiro + dia - 1;
            int col = celula % 7;
            int row = celula / 7;
            int cx = gridX + col * cellW + cellW / 2;
            int cy = gridY + row * cellH + cellH / 2;

            bool hoje = (dia == hojeD && mes == hojeM && ano == hojeA);
            if (hoje) {   // destaque: quadrado branco preenchido
                d.fillRect(cx - cellW / 2 + 1, cy - cellH / 2, cellW - 2, cellH - 1, noir::WHITE);
                d.setTextColor(noir::BLACK, noir::WHITE);
            } else {
                d.setTextColor(noir::BONE, noir::BLACK);
            }
            char nb[4];
            std::snprintf(nb, sizeof(nb), "%d", dia);
            d.setTextDatum(middle_center);
            d.drawString(nb, cx, cy);
        }

        drawHints(", / mes   ; . ano   ` sai");
        ui::present();

        KeyEvent e = ui::waitKey();
        if (e.key == Key::Left)  { if (--mes < 0)  { mes = 11; ano--; } }
        else if (e.key == Key::Right) { if (++mes > 11) { mes = 0; ano++; } }
        else if (e.key == Key::Up)    ano++;
        else if (e.key == Key::Down)  ano--;
        else if (e.key == Key::Back)  return;
    }
}

} // ======================= FIM DO NAMESPACE ANONIMO ===========================

// ----------------------------------------------------------------------------
//  Exportacao: array de apps da categoria (visto pelo app_registry).
//  Todos danger=false (nenhum transmite nem apaga nada).
// ----------------------------------------------------------------------------
namespace apps {
namespace produtividade {

const noir::AppEntry PRODUTIVIDADE_APPS[] = {
    {"Cronometro", "tempo",  appCronometro, false},
    {"Conversor",  "unids",  appConversor,  false},
    {"Pomodoro",   "foco",   appPomodoro,   false},
    {"Calendario", "mes",    appCalendario, false},
};
const int PRODUTIVIDADE_APPS_COUNT =
    (int)(sizeof(PRODUTIVIDADE_APPS) / sizeof(PRODUTIVIDADE_APPS[0]));

} // namespace produtividade
} // namespace apps
