// ============================================================================
//  Noir OS  -  Categoria "Seguranca" (ferramentas offline)
//
//  Todos os apps aqui funcionam SEM rede (o TOTP so precisa da hora, obtida
//  antes por NTP). Nenhum transmite nada: sao todos danger=false.
//
//  Apps:
//    1) Gerador de senhas  -> entropia com esp_random(), opcao de QR.
//    2) Base64             -> encode/decode com mbedtls/base64.
//    3) QR Code            -> desenho nativo da M5GFX (gfx().qrcode()).
//    4) TOTP (RFC 6238)    -> Base32 + HMAC-SHA1 (mbedtls), 6 digitos.
//    5) Cofre criptografado-> PBKDF2 + AES-256-GCM (mbedtls), blob na NVS.
//    6) Cofre: Backup      -> exporta/importa as chaves do cofre para o SD.
//
//  Toda a criptografia usa o mbedtls que ja' vem no core do ESP32 (nao ha
//  dependencia externa). O codigo e' propositalmente comentado para servir
//  de material de estudo; onde ha risco de seguranca, esta' anotado.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
// IMPORTANTE: SPI/SD ANTES de qualquer header que puxe o M5GFX (ui/theme.h ->
// M5Cardputer.h). O M5GFX so' habilita o suporte a SD (SDFS) se <SD.h> ja' tiver
// sido incluido antes. Usado pelo app "Cofre: Backup". (Veja arquivos.cpp.)
#include <SPI.h>
#include <SD.h>

#include "apps/seguranca/seguranca.h"

#include "ui/theme.h"       // ui::gfx(), clearNoir(), present(), cores noir::*
#include "ui/widgets.h"     // listView, confirm, textInput, messageBox, ...
#include "ui/input.h"       // ui::readKey / waitKey
#include "ui/statusbar.h"   // ui::statusBar
#include "core/config.h"    // NVS (salt, blob do cofre)
#include "core/time_service.h"

#include <M5Cardputer.h>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>          // strtoull() para o contador de nonce persistido
#include <time.h>

#include "esp_random.h"        // esp_random() -> uint32_t do gerador de HW (TRNG)
#include "mbedtls/base64.h"    // Base64
#include "mbedtls/md.h"        // HMAC-SHA1 (TOTP) e HMAC-SHA256 (PBKDF2)
#include "mbedtls/gcm.h"       // AES-256-GCM (cofre)

namespace apps {
namespace seguranca {

// ============================================================================
//  Tudo abaixo (helpers + apps) e' file-local: namespace anonimo.
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
//  Helpers de baixo nivel: aleatoriedade, Base64, Base32.
// ----------------------------------------------------------------------------

// Preenche 'n' bytes com aleatoriedade de hardware. esp_random() usa o TRNG do
// ESP32 (bom para chaves/salt/IV desde que o RF/WiFi esteja ativo em algum
// momento; aqui usamos como fonte de entropia razoavel para uso pessoal).
void fillRandom(uint8_t* buf, size_t n) {
    size_t i = 0;
    while (i < n) {
        uint32_t r = esp_random();
        size_t k = (n - i < 4) ? (n - i) : 4;
        memcpy(buf + i, &r, k);
        i += k;
    }
}

// Codifica bytes -> String Base64.
String b64encode(const uint8_t* data, size_t n) {
    if (n == 0) return String("");
    size_t cap = 4 * ((n + 2) / 3) + 1;   // tamanho maximo + terminador
    std::vector<uint8_t> out(cap);
    size_t olen = 0;
    if (mbedtls_base64_encode(out.data(), cap, &olen, data, n) != 0) return String("");
    return String((const char*)out.data());
}

// Decodifica String Base64 -> bytes. Retorna false se invalida.
bool b64decode(const String& s, std::vector<uint8_t>& out) {
    out.clear();
    if (s.length() == 0) return true;
    out.resize(s.length());                       // saida decodificada <= entrada
    size_t olen = 0;
    int rc = mbedtls_base64_decode(out.data(), out.size(), &olen,
                                   (const uint8_t*)s.c_str(), s.length());
    if (rc != 0) return false;
    out.resize(olen);
    return true;
}

// Decodifica Base32 (RFC 4648) -> bytes. Aceita minusculas, espacos e '-';
// para no primeiro '='. Usado pelos segredos TOTP. Retorna false se algo
// invalido aparecer.
bool base32Decode(const String& in, std::vector<uint8_t>& out) {
    out.clear();
    uint32_t buffer = 0;
    int bits = 0;
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == ' ' || c == '-') continue;   // separadores comuns
        if (c == '=') break;                    // padding: fim dos dados
        int v;
        if (c >= 'A' && c <= 'Z')      v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a';
        else if (c >= '2' && c <= '7') v = c - '2' + 26;
        else return false;                      // caractere fora do alfabeto
        buffer = (buffer << 5) | (uint32_t)v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)((buffer >> bits) & 0xff));
        }
    }
    return !out.empty();
}

// ----------------------------------------------------------------------------
//  PBKDF2-HMAC-SHA256 (deriva a chave de 32 bytes a partir da senha-mestra).
//
//  Como so precisamos de 32 bytes (= tamanho do SHA-256), basta o "bloco 1"
//  do PBKDF2. Reutilizamos um unico contexto HMAC entre as iteracoes para
//  nao pagar setup/alloc a cada volta (importante num MCU).
// ----------------------------------------------------------------------------
// Custo por desbloqueio. Trade-off no MCU: mais iteracoes = brute force offline
// mais caro (bom), porem cada unlock demora mais (ruim). 100000 e' um piso
// razoavel para o ESP32-S3 (~1-2 s de derivacao) sem PSRAM.
// ATENCAO: mudar este valor INVALIDA cofres ja' existentes, pois a chave
// derivada muda e a GCM deixa de decifrar -> use "Resetar cofre" para recriar.
constexpr int PBKDF2_ITERS = 100000;

void pbkdf2_sha256(const uint8_t* pw, size_t pwlen,
                   const uint8_t* salt, size_t saltlen,
                   int iters, uint8_t out[32]) {
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1 /* usar como HMAC */);

    uint8_t U[32], T[32];
    // U1 = HMAC(pw, salt || INT_BE32(1))
    const uint8_t idx[4] = {0, 0, 0, 1};
    mbedtls_md_hmac_starts(&ctx, pw, pwlen);
    mbedtls_md_hmac_update(&ctx, salt, saltlen);
    mbedtls_md_hmac_update(&ctx, idx, 4);
    mbedtls_md_hmac_finish(&ctx, U);
    memcpy(T, U, 32);

    // T = U1 ^ U2 ^ ... ^ Uc, com Ui = HMAC(pw, U(i-1))
    for (int i = 1; i < iters; i++) {
        mbedtls_md_hmac_starts(&ctx, pw, pwlen);   // re-chaveia (finish reseta)
        mbedtls_md_hmac_update(&ctx, U, 32);
        mbedtls_md_hmac_finish(&ctx, U);
        for (int j = 0; j < 32; j++) T[j] ^= U[j];
    }
    memcpy(out, T, 32);

    mbedtls_md_free(&ctx);
    memset(U, 0, sizeof(U));   // higiene: apaga intermediarios da pilha
    memset(T, 0, sizeof(T));
}

// ----------------------------------------------------------------------------
//  Desenho: bloco de texto em fonte fixa (mono), com quebra por largura.
//  Usa Font0 (unica fixed-width da M5GFX) ampliada 2x para ficar legivel.
// ----------------------------------------------------------------------------
void drawMono(const String& text, int x, int y, int charsPerLine, int lineH,
              uint16_t color = noir::WHITE) {
    M5Canvas& d = ui::gfx();
    d.setTextSize(2);
    d.setFont(&fonts::Font0);
    d.setTextDatum(top_left);
    d.setTextColor(color, noir::BLACK);
    int line = 0;
    for (int i = 0; i < (int)text.length(); i += charsPerLine) {
        int end = i + charsPerLine;
        if (end > (int)text.length()) end = text.length();
        String seg = text.substring(i, end);
        d.drawString(seg.c_str(), x, y + line * lineH);
        line++;
    }
    d.setTextSize(1);   // volta ao padrao para nao afetar telas seguintes
}

// Rodape de dica (fonte pequena, cinza) usado em varias telas proprias.
void drawHint(const char* txt) {
    M5Canvas& d = ui::gfx();
    d.setTextSize(1);
    d.setFont(&fonts::Font0);
    d.setTextDatum(bottom_center);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString(txt, noir::SCREEN_W / 2, noir::SCREEN_H - 2);
}

// ----------------------------------------------------------------------------
//  QR Code (compartilhado pelo app de QR e pela opcao do gerador de senhas).
//  A M5GFX desenha o QR nativamente: escolhe a menor versao que cabe no texto
//  e ja' pinta o fundo branco. margin=true garante a "quiet zone".
// ----------------------------------------------------------------------------
void showQr(const char* title, const String& text) {
    M5Canvas& d = ui::gfx();
    if (text.length() == 0) { ui::messageBox(title, "Nada para gerar."); return; }
    if (text.length() > 512) { ui::messageBox(title, "Texto longo demais\npara um QR legivel."); return; }

    ui::clearNoir();
    ui::statusBar(title);

    // Quadrado centralizado na area util (abaixo da statusbar).
    const int avail = noir::SCREEN_H - noir::STATUSBAR_H;
    int w = avail - 16;                    // deixa uma folga vertical
    int x = (noir::SCREEN_W - w) / 2;
    int y = noir::STATUSBAR_H + (avail - w) / 2;

    // qrcode(texto, x, y, tamanho, versao_inicial, margin). Comeca na versao 1
    // e a M5GFX sobe a versao ate' o texto caber; margin desenha a quiet zone.
    d.qrcode(text.c_str(), x, y, w, 1, true);

    ui::present();
    for (;;) {
        ui::KeyEvent e = ui::waitKey();
        if (e.key == ui::Key::Back || e.key == ui::Key::Enter) return;
    }
}

// ============================================================================
//  APP 1  -  Gerador de senhas
// ============================================================================

// Gera uma senha usando esp_random() com "rejection sampling" para evitar o
// vies de modulo (%). Sem vies, cada caractere do alfabeto e' equiprovavel.
String gerarSenha(const String& alfabeto, int tam) {
    String out;
    out.reserve(tam);
    const int m = alfabeto.length();
    if (m == 0) return out;
    // Maior multiplo de 'm' <= 256; bytes acima disso sao descartados.
    const int limite = (256 / m) * m;
    while ((int)out.length() < tam) {
        uint8_t b;
        fillRandom(&b, 1);
        if (b >= limite) continue;       // descarta p/ manter uniformidade
        out += alfabeto[b % m];
    }
    return out;
}

// Tela que mostra a senha (mono) e oferece: ENTER=nova, Q=QR, `=sair.
// Retorna true se o usuario pediu "gerar nova".
bool telaSenha(const String& senha) {
    M5Canvas& d = ui::gfx();
    ui::clearNoir();
    ui::statusBar("Senha gerada");
    drawMono(senha, 10, noir::STATUSBAR_H + 12, 18, 20);
    // Rodape com o tamanho e as acoes.
    d.setTextSize(1);
    d.setFont(&fonts::Font0);
    d.setTextDatum(bottom_left);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString((String(senha.length()) + " chars").c_str(), 6, noir::SCREEN_H - 2);
    drawHint("ENTER nova   Q qr   ` sair");
    ui::present();

    for (;;) {
        ui::KeyEvent e = ui::waitKey();
        if (e.key == ui::Key::Enter) return true;                 // gerar nova
        if (e.key == ui::Key::Back)  return false;                // sair
        if (e.key == ui::Key::Char && (e.ch == 'q' || e.ch == 'Q')) {
            showQr("QR da senha", senha);
            // Redesenha a tela da senha apos fechar o QR.
            ui::clearNoir();
            ui::statusBar("Senha gerada");
            drawMono(senha, 10, noir::STATUSBAR_H + 12, 18, 20);
            drawHint("ENTER nova   Q qr   ` sair");
            ui::present();
        }
    }
}

void appGerador() {
    // Estado das opcoes (persistido em RAM enquanto o app roda).
    int  tam = 16;
    bool usarMai = true, usarMin = true, usarNum = true, usarSim = false;

    for (;;) {
        // Monta o menu refletindo o estado atual (checkbox [x]/[ ]).
        auto chk = [](bool b) { return String(b ? "[x]" : "[ ]"); };
        std::vector<String> itens = {
            String("Tamanho: ") + tam,
            String("Maiusculas ABC ") + chk(usarMai),
            String("Minusculas abc ") + chk(usarMin),
            String("Numeros 123 ")    + chk(usarNum),
            String("Simbolos !@# ")   + chk(usarSim),
            String("== Gerar senha =="),
        };
        int r = ui::listView("Gerador de senha", itens);
        if (r < 0) return;

        switch (r) {
            case 0: {   // ajusta o tamanho com < e >
                for (;;) {
                    ui::progress("Tamanho", "< diminui   > aumenta   ENTER ok", tam);
                    ui::KeyEvent e = ui::waitKey();
                    if (e.key == ui::Key::Left)  { if (tam > 4)  tam--; }
                    else if (e.key == ui::Key::Right) { if (tam < 64) tam++; }
                    else if (e.key == ui::Key::Enter || e.key == ui::Key::Back) break;
                }
                break;
            }
            case 1: usarMai = !usarMai; break;
            case 2: usarMin = !usarMin; break;
            case 3: usarNum = !usarNum; break;
            case 4: usarSim = !usarSim; break;
            case 5: {   // gera
                String alf;
                if (usarMai) alf += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                if (usarMin) alf += "abcdefghijklmnopqrstuvwxyz";
                if (usarNum) alf += "0123456789";
                if (usarSim) alf += "!@#$%&*-_=+?";
                if (alf.length() == 0) {
                    ui::redStripe("Escolha ao menos 1 tipo");
                    break;
                }
                // Loop de "gerar nova" enquanto o usuario pedir ENTER.
                bool novamente = true;
                while (novamente) {
                    String senha = gerarSenha(alf, tam);
                    novamente = telaSenha(senha);
                }
                break;
            }
        }
    }
}

// ============================================================================
//  APP 2  -  Base64 (encode / decode)
// ============================================================================
void appBase64() {
    int modo = ui::listView("Base64", {
        "Codificar (texto -> b64)",
        "Decodificar (b64 -> texto)",
    });
    if (modo < 0) return;

    bool ok = true;
    String in = ui::textInput(modo == 0 ? "Texto p/ codificar"
                                        : "Base64 p/ decodificar",
                              "", false, &ok);
    if (!ok) return;

    if (modo == 0) {
        String out = b64encode((const uint8_t*)in.c_str(), in.length());
        ui::messageBox("Resultado Base64", out);
    } else {
        std::vector<uint8_t> bytes;
        if (!b64decode(in, bytes)) {
            ui::redStripe("Base64 invalido");
            return;
        }
        // Reconstroi como texto (bytes crus viram chars).
        String out;
        out.reserve(bytes.size());
        for (uint8_t b : bytes) out += (char)b;
        ui::messageBox("Texto decodificado", out);
    }
}

// ============================================================================
//  APP 3  -  QR Code de um texto qualquer
// ============================================================================
void appQr() {
    bool ok = true;
    String txt = ui::textInput("Texto do QR", "", false, &ok);
    if (!ok || txt.length() == 0) return;
    showQr("QR Code", txt);
}

// ============================================================================
//  COFRE CRIPTOGRAFADO  -  estado e primitivas (compartilhado por TOTP + Cofre)
//
//  Modelo de dados: uma lista de entradas {kind, title, secret}, serializada
//  em texto e cifrada com AES-256-GCM. A chave vem da senha-mestra via PBKDF2.
//    kind = 'P' -> segredo/senha generica (app Cofre)
//    kind = 'T' -> semente TOTP em Base32 (app TOTP)
//
//  NVS (chaves <= 15 chars):
//    "sec_salt"  -> Base64 do salt (16 bytes) do PBKDF2 (gravado so' APOS o
//                   primeiro vaultSave dar certo, para nunca deixar um cofre
//                   "meio criado" e inacessivel).
//    "sec_vault" -> Base64 de (IV[12] || TAG[16] || CIPHERTEXT)
//    "sec_ctr"   -> contador monotonico (decimal) que gera os 8 primeiros bytes
//                   do nonce GCM, garantindo nonce unico por construcao.
//
//  Seguranca:
//    - GCM e' cifra AUTENTICADA: a TAG confirma integridade E chave correta.
//      Se a senha-mestra estiver errada, mbedtls_gcm_auth_decrypt falha -> nao
//      precisamos guardar hash-verificador separado nem revelamos nada.
//    - A chave derivada vive so' na RAM (g_key) e e' apagada (memset) ao
//      bloquear. Riscos residuais: a RAM pode ser lida por um atacante fisico
//      com JTAG; o texto plano existe brevemente durante cifra/decifra; e nao
//      ha protecao contra keylogger de hardware. Para um "cofre pessoal" num
//      dispositivo de estudo isto e' um compromisso razoavel, mas nao e'
//      grau militar.
// ============================================================================
struct VaultEntry {
    char   kind;      // 'P' ou 'T'
    String title;
    String secret;
};

uint8_t                 g_key[32];       // chave AES-256 derivada (so em RAM)
bool                    g_unlocked = false;
std::vector<VaultEntry> g_entries;

// Higiene de memoria: zera o buffer interno de uma Arduino String que guardou
// dado sensivel (senha-mestra/segredo) e a esvazia. A Arduino String NAO limpa
// a heap ao ser destruida, entao sobrescrevemos manualmente antes de sair de
// escopo. Limitacao: copias temporarias feitas pela lib (realloc/substring/+=)
// podem deixar residuos que nao controlamos; isto REDUZ, mas nao elimina, a
// janela de exposicao do texto plano na RAM.
void wipe(String& s) {
    if (s.length() > 0) memset((void*)s.c_str(), 0, s.length());
    s = "";
}

// Remove separadores que quebrariam a serializacao (\t e \n viram espaco).
String sanitize(const String& s) {
    String o = s;
    o.replace('\t', ' ');
    o.replace('\n', ' ');
    o.replace('\r', ' ');
    return o;
}

// Entradas -> texto plano. Cada linha: "K\tTITULO\tSEGREDO".
// A 1a linha "V1" e' um cabecalho de versao que garante que o texto cifrado
// NUNCA seja vazio: cifrar plaintext de tamanho ZERO com AES-GCM e' um caso-
// limite que pode falhar (e ai o cofre nao persistia e voltava a pedir para
// criar). O parse() ignora esta linha (ela nao tem os 2 tabs esperados).
String serialize() {
    String s = "V1\n";
    for (const auto& e : g_entries) {
        s += e.kind; s += '\t'; s += e.title; s += '\t'; s += e.secret; s += '\n';
    }
    return s;
}

// Texto plano -> entradas.
void parse(const String& s) {
    g_entries.clear();
    int start = 0;
    while (start < (int)s.length()) {
        int nl = s.indexOf('\n', start);
        if (nl < 0) nl = s.length();
        String line = s.substring(start, nl);
        start = nl + 1;
        if (line.length() < 3) continue;
        int t1 = line.indexOf('\t');
        int t2 = line.indexOf('\t', t1 + 1);
        if (t1 < 0 || t2 < 0) continue;
        VaultEntry e;
        e.kind   = line[0];
        e.title  = line.substring(t1 + 1, t2);
        e.secret = line.substring(t2 + 1);
        g_entries.push_back(e);
    }
}

// Cifra 'plain' com g_key. Saida = IV || TAG || CIPHERTEXT.
bool gcmEncrypt(const String& plain, std::vector<uint8_t>& blob) {
    // Nonce GCM de 96 bits UNICO POR CONSTRUCAO. Reusar o par (chave, nonce) no
    // GCM e' catastrofico: vaza o keystream e permite forjar a TAG. Como este app
    // e' OFFLINE, o TRNG (esp_random) pode nao ter entropia de RF garantida e um
    // IV puramente aleatorio poderia REPETIR com a chave fixa de sessao. Por isso
    // os 8 primeiros bytes vem de um contador MONOTONICO persistido na NVS
    // ("sec_ctr"), incrementado e gravado a cada cifragem: mesmo com RNG fraco o
    // nonce nunca se repete. Os 4 bytes finais sao aleatorios apenas como margem.
    uint64_t ctr = (uint64_t)strtoull(
        noir::config::getStr("sec_ctr", "0").c_str(), nullptr, 10) + 1;
    char ctrStr[24];
    snprintf(ctrStr, sizeof(ctrStr), "%llu", (unsigned long long)ctr);
    noir::config::setStr("sec_ctr", String(ctrStr));   // persiste ANTES de usar

    uint8_t iv[12];
    for (int i = 7; i >= 0; i--) { iv[i] = (uint8_t)(ctr & 0xff); ctr >>= 8; }
    uint32_t rnd = esp_random();
    memcpy(iv + 8, &rnd, 4);

    size_t n = plain.length();
    std::vector<uint8_t> ct(n);
    uint8_t tag[16];

    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, g_key, 256) != 0) {
        mbedtls_gcm_free(&g); return false;
    }
    int rc = mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, n,
                                       iv, 12, nullptr, 0,
                                       (const uint8_t*)plain.c_str(), ct.data(),
                                       16, tag);
    mbedtls_gcm_free(&g);
    if (rc != 0) return false;

    blob.clear();
    blob.insert(blob.end(), iv, iv + 12);
    blob.insert(blob.end(), tag, tag + 16);
    blob.insert(blob.end(), ct.begin(), ct.end());
    return true;
}

// Decifra IV || TAG || CIPHERTEXT com g_key. Falha se a TAG nao bater (senha
// errada ou dado adulterado).
bool gcmDecrypt(const std::vector<uint8_t>& blob, String& plain) {
    if (blob.size() < 28) return false;   // precisa de pelo menos IV+TAG
    const uint8_t* iv  = blob.data();
    const uint8_t* tag = blob.data() + 12;
    const uint8_t* ct  = blob.data() + 28;
    size_t n = blob.size() - 28;
    std::vector<uint8_t> pt(n);

    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, g_key, 256) != 0) {
        mbedtls_gcm_free(&g); return false;
    }
    int rc = mbedtls_gcm_auth_decrypt(&g, n, iv, 12, nullptr, 0,
                                      tag, 16, ct, pt.data());
    mbedtls_gcm_free(&g);
    if (rc != 0) return false;            // MBEDTLS_ERR_GCM_AUTH_FAILED etc.

    plain = "";
    plain.reserve(n);
    for (size_t i = 0; i < n; i++) plain += (char)pt[i];
    return true;
}

// Persiste g_entries cifradas na NVS.
bool vaultSave() {
    std::vector<uint8_t> blob;
    if (!gcmEncrypt(serialize(), blob)) return false;
    noir::config::setStr("sec_vault", b64encode(blob.data(), blob.size()));
    return true;
}

// Le e decifra o blob da NVS para g_entries. false = senha errada/corrompido.
bool vaultLoad() {
    String b64 = noir::config::getStr("sec_vault", "");
    std::vector<uint8_t> blob;
    if (!b64decode(b64, blob)) return false;
    String plain;
    if (!gcmDecrypt(blob, plain)) return false;
    parse(plain);
    return true;
}

// Zera a chave da RAM e trava o cofre.
void vaultLock() {
    memset(g_key, 0, sizeof(g_key));
    for (auto& e : g_entries) wipe(e.secret);   // zera os segredos antes de liberar
    g_entries.clear();
    g_unlocked = false;
}

// Recuperacao de emergencia: apaga TODO o cofre (salt + blob + contador de
// nonce). Usada quando a senha-mestra foi perdida, o blob corrompeu ou um cofre
// antigo ficou incompativel (ex.: mudanca de iteracoes do PBKDF2). NAO recupera
// dados; apenas permite recriar um cofre limpo.
void vaultReset() {
    noir::config::remove("sec_salt");
    noir::config::remove("sec_vault");
    noir::config::remove("sec_ctr");
    vaultLock();
}

// Garante o cofre destravado. Na primeira vez, cria a senha-mestra.
// Retorna false se o usuario cancelar ou errar a senha.
bool vaultEnsureUnlocked() {
    if (g_unlocked) return true;
    noir::config::begin();

    String saltB64 = noir::config::getStr("sec_salt", "");

    // -------- Primeiro uso: nao existe cofre ainda --------
    if (saltB64.length() == 0) {
        if (!ui::confirm("Cofre", "Nenhum cofre existe.\nCriar um novo?")) return false;

        bool ok = true;
        String p1 = ui::textInput("Nova senha-mestra", "", true, &ok);
        if (!ok) return false;
        if (p1.length() < 8) {
            wipe(p1);
            ui::messageBox("Cofre", "Senha-mestra curta demais.\nUse ao menos 8 caracteres\ne tente de novo.");
            return false;
        }
        String p2 = ui::textInput("Repita a senha", "", true, &ok);
        if (!ok) { wipe(p1); return false; }
        if (p1 != p2) {
            wipe(p1); wipe(p2);
            ui::messageBox("Cofre", "As duas senhas nao\nbatem. Tente de novo.");
            return false;
        }

        uint8_t salt[16];
        fillRandom(salt, 16);
        ui::progress("Cofre", "Derivando chave...", 50);
        pbkdf2_sha256((const uint8_t*)p1.c_str(), p1.length(), salt, 16,
                      PBKDF2_ITERS, g_key);
        wipe(p1); wipe(p2);   // a senha-mestra ja' virou chave: some com o texto

        g_entries.clear();
        g_unlocked = true;
        // O salt so' e' persistido APOS o vaultSave dar certo. Se gravassemos o
        // salt antes e o save falhasse, o cofre existiria (sec_salt presente) mas
        // sem blob decifravel -> inacessivel para sempre. Na falha, removemos
        // qualquer residuo para o proximo uso recomecar limpo.
        if (!vaultSave()) {
            noir::config::remove("sec_vault");
            noir::config::remove("sec_ctr");
            vaultLock();
            ui::messageBox("Cofre", "Falha ao gravar o cofre\n(cripto/NVS). Nada foi\nsalvo. Tente de novo.");
            return false;
        }
        noir::config::setStr("sec_salt", b64encode(salt, 16));

        // Verifica que o salt REALMENTE persistiu. Se nao persistir, o proximo
        // acesso acharia que nao ha cofre e pediria para criar de novo -- entao
        // avisamos aqui em vez de deixar o cofre "sumir".
        if (noir::config::getStr("sec_salt", "").length() == 0) {
            noir::config::remove("sec_vault");
            noir::config::remove("sec_ctr");
            vaultLock();
            ui::messageBox("Cofre", "Nao consegui persistir o\ncofre na NVS. Verifique\no armazenamento.");
            return false;
        }
        return true;
    }

    // -------- Cofre existe: desbloquear (ou resetar se a senha foi perdida) --------
    // A opcao de reset vem ANTES de pedir a senha: e' a unica recuperacao possivel
    // para quem esqueceu a senha-mestra (num cofre GCM nao existe "esqueci minha
    // senha"). Tambem resgata cofres corrompidos ou incompativeis (ex.: iteracoes
    // do PBKDF2 alteradas).
    int esc = ui::listView("Cofre", {"Desbloquear", "Resetar cofre"}, 0, 1 /* reset = perigo */);
    if (esc < 0) return false;
    if (esc == 1) {
        if (ui::confirm("Resetar cofre",
                        "Isto APAGA todo o\ncofre (sem volta).\nContinuar?", true)) {
            vaultReset();
            ui::messageBox("Cofre", "Cofre apagado.\nAbra de novo para\ncriar um novo.");
        }
        return false;
    }

    std::vector<uint8_t> salt;
    if (!b64decode(saltB64, salt) || salt.size() != 16) {
        ui::redStripe("Cofre corrompido");
        return false;
    }
    bool ok = true;
    String pw = ui::textInput("Senha-mestra", "", true, &ok);
    if (!ok) return false;

    ui::progress("Cofre", "Derivando chave...", 50);
    pbkdf2_sha256((const uint8_t*)pw.c_str(), pw.length(), salt.data(), 16,
                  PBKDF2_ITERS, g_key);
    wipe(pw);   // o texto da senha some assim que vira chave

    // A prova da senha correta e' a propria GCM: se decifrar, a chave bate.
    if (!vaultLoad()) {
        memset(g_key, 0, sizeof(g_key));   // higiene: nao deixa chave errada na RAM
        ui::redStripe("Senha incorreta");
        return false;
    }
    g_unlocked = true;
    return true;
}

// ============================================================================
//  APP 4  -  TOTP (RFC 6238)
// ============================================================================

// Calcula o codigo TOTP (HMAC-SHA1 truncado -> 'digits' digitos).
uint32_t totpCode(const uint8_t* key, size_t klen, uint64_t counter, int digits) {
    // Contador de 8 bytes big-endian (numero de janelas desde a epoch).
    uint8_t msg[8];
    for (int i = 7; i >= 0; i--) { msg[i] = (uint8_t)(counter & 0xff); counter >>= 8; }

    uint8_t hash[20];   // SHA-1 = 20 bytes
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    mbedtls_md_hmac(info, key, klen, msg, sizeof(msg), hash);

    // "Dynamic truncation" da RFC 4226.
    int offset = hash[19] & 0x0f;
    uint32_t bin = ((uint32_t)(hash[offset]     & 0x7f) << 24) |
                   ((uint32_t)(hash[offset + 1] & 0xff) << 16) |
                   ((uint32_t)(hash[offset + 2] & 0xff) << 8)  |
                   ((uint32_t)(hash[offset + 3] & 0xff));
    uint32_t mod = 1;
    for (int i = 0; i < digits; i++) mod *= 10;
    return bin % mod;
}

// Tela ao vivo de um codigo TOTP: 6 digitos grandes + barra de tempo (30s).
void telaTotp(const String& title, const std::vector<uint8_t>& key) {
    M5Canvas& d = ui::gfx();
    for (;;) {
        // TOTP usa a epoch UTC (independe do fuso). time(nullptr) ja' e' UTC
        // depois do NTP; por isso exigimos hora sincronizada antes.
        uint64_t epoch   = (uint64_t)time(nullptr);
        uint64_t counter = epoch / 30;
        int      rem     = 30 - (int)(epoch % 30);
        uint32_t code    = totpCode(key.data(), key.size(), counter, 6);

        ui::clearNoir();
        ui::statusBar(title.c_str());

        char buf[8];
        snprintf(buf, sizeof(buf), "%06lu", (unsigned long)code);
        d.setFont(&fonts::Font7);            // display 7-segmentos: cara de token
        d.setTextDatum(middle_center);
        d.setTextColor(noir::WHITE, noir::BLACK);
        d.drawString(buf, noir::SCREEN_W / 2, 58);

        // Barra do tempo restante; fica vermelha nos ultimos 5s.
        const int bx = 20, by = 100, bw = 200, bh = 8;
        int fill = bw * rem / 30;
        d.drawRect(bx, by, bw, bh, noir::ASH);
        d.fillRect(bx, by, fill, bh, rem <= 5 ? noir::BLOOD : noir::BONE);

        drawHint("` sair");
        ui::present();

        // Espera ~1s, mas responde ao "voltar" quase de imediato.
        for (int i = 0; i < 20; i++) {
            ui::KeyEvent e = ui::readKey();
            if (e.key == ui::Key::Back) return;
            delay(50);
        }
    }
}

void appTotp() {
    // Precisa de hora valida (obtida por NTP quando havia WiFi).
    if (!noir::timeservice::have()) {
        ui::messageBox("TOTP",
            "Preciso da hora certa.\nConecte o WiFi e\nsincronize o relogio\n(Config > Fuso/NTP).");
        return;
    }
    // Segredos ficam no cofre: exige desbloqueio.
    if (!vaultEnsureUnlocked()) return;

    for (;;) {
        // Lista so' as entradas do tipo 'T'.
        std::vector<String> itens;
        std::vector<int>     map;   // item -> indice em g_entries
        for (int i = 0; i < (int)g_entries.size(); i++) {
            if (g_entries[i].kind == 'T') { itens.push_back(g_entries[i].title); map.push_back(i); }
        }
        int addIdx = itens.size();  itens.push_back("+ Adicionar conta");
        int lockIdx = itens.size(); itens.push_back("Bloquear cofre");

        int r = ui::listView("TOTP", itens);
        if (r < 0) { vaultLock(); return; }   // voltar bloqueia por seguranca

        if (r == lockIdx) { vaultLock(); ui::redStripe("Cofre bloqueado"); return; }

        if (r == addIdx) {
            bool ok = true;
            String nome = ui::textInput("Nome da conta", "", false, &ok);
            if (!ok || nome.length() == 0) continue;
            String seg = ui::textInput("Segredo Base32", "", true, &ok);
            if (!ok || seg.length() == 0) continue;
            std::vector<uint8_t> chk;
            if (!base32Decode(seg, chk)) { wipe(seg); ui::redStripe("Base32 invalido"); continue; }
            if (!chk.empty()) memset(chk.data(), 0, chk.size());   // segredo decodificado fora da pilha
            g_entries.push_back({'T', sanitize(nome), sanitize(seg)});
            wipe(seg);   // zera o texto do segredo (a copia sanitizada ja' esta' no cofre)
            if (!vaultSave()) ui::redStripe("Falha ao salvar");
            continue;
        }

        // Selecionou uma conta: acao ver/apagar.
        int idx = map[r];
        int act = ui::listView(g_entries[idx].title.c_str(),
                               {"Ver codigo", "Apagar conta"}, 0, 1 /* apagar = perigo */);
        if (act < 0) continue;
        if (act == 0) {
            std::vector<uint8_t> key;
            if (!base32Decode(g_entries[idx].secret, key)) { ui::redStripe("Segredo invalido"); continue; }
            telaTotp(g_entries[idx].title, key);
            if (!key.empty()) memset(key.data(), 0, key.size());   // higiene: apaga a chave HMAC
        } else if (act == 1) {
            if (ui::confirm("Apagar", "Remover esta conta\ndo cofre?", true)) {
                g_entries.erase(g_entries.begin() + idx);
                if (!vaultSave()) ui::redStripe("Falha ao salvar");
            }
        }
    }
}

// ============================================================================
//  APP 5  -  Cofre criptografado (senhas/segredos genericos, kind 'P')
// ============================================================================
void appCofre() {
    if (!vaultEnsureUnlocked()) return;

    for (;;) {
        std::vector<String> itens;
        std::vector<int>     map;
        for (int i = 0; i < (int)g_entries.size(); i++) {
            if (g_entries[i].kind == 'P') { itens.push_back(g_entries[i].title); map.push_back(i); }
        }
        int addIdx  = itens.size(); itens.push_back("+ Adicionar entrada");
        int lockIdx = itens.size(); itens.push_back("Bloquear cofre");

        int r = ui::listView("Cofre", itens);
        if (r < 0) { vaultLock(); return; }   // sair bloqueia (apaga a chave)

        if (r == lockIdx) { vaultLock(); ui::redStripe("Cofre bloqueado"); return; }

        if (r == addIdx) {
            bool ok = true;
            String titulo = ui::textInput("Titulo (ex: Email)", "", false, &ok);
            if (!ok || titulo.length() == 0) continue;
            String segredo = ui::textInput("Segredo/senha", "", true, &ok);
            if (!ok) continue;
            g_entries.push_back({'P', sanitize(titulo), sanitize(segredo)});
            wipe(segredo);   // zera o texto digitado (copia sanitizada ja' esta' no cofre)
            if (!vaultSave()) ui::redStripe("Falha ao salvar");
            continue;
        }

        // Entrada existente: ver ou apagar.
        int idx = map[r];
        int act = ui::listView(g_entries[idx].title.c_str(),
                               {"Ver segredo", "Apagar"}, 0, 1);
        if (act < 0) continue;
        if (act == 0) {
            ui::messageBox(g_entries[idx].title.c_str(), g_entries[idx].secret);
        } else if (act == 1) {
            if (ui::confirm("Apagar", "Remover esta entrada\ndo cofre?", true)) {
                g_entries.erase(g_entries.begin() + idx);
                if (!vaultSave()) ui::redStripe("Falha ao salvar");
            }
        }
    }
}

// ============================================================================
//  APP 6  -  Cofre: Backup / Restauracao (cartao SD)
//
//  Copia as 3 chaves do cofre entre a NVS e um arquivo texto no SD
//  (/noir/cofre.bak). Nada aqui e decifrado: o "vault" ja e ciphertext
//  AES-GCM, e o "salt"/"ctr" sao publicos por natureza. Logo o arquivo de
//  backup e seguro de guardar (so serve com a senha-mestra original). Este app
//  nao transmite RF: danger=false. As escritas destrutivas (restaurar) pedem
//  ui::confirm(..., danger=true).
//
//  As 3 chaves NVS (ver "COFRE CRIPTOGRAFADO"):
//    sec_salt  -> Base64 do salt (16 bytes) do PBKDF2
//    sec_vault -> Base64 de (IV || TAG || CIPHERTEXT)
//    sec_ctr   -> contador monotonico decimal do nonce GCM
// ============================================================================

// Caminho fixo do arquivo de backup.
const char* BACKUP_DIR  = "/noir";
const char* BACKUP_PATH = "/noir/cofre.bak";

// Garante o SD montado. Igual ao padrao de arquivos.cpp: o SPI so precisa ser
// configurado uma vez; SD.end() antes de begin() forca re-montagem (permite
// trocar o cartao a quente). Retorna false se nao houver cartao.
bool backupEnsureSD() {
    static bool spiStarted = false;
    if (!spiStarted) {
        SPI.begin(40, 39, 14, 12);   // SCLK, MISO, MOSI, CS
        spiStarted = true;
    }
    SD.end();
    if (!SD.begin(12, SPI)) return false;
    return SD.cardType() != CARD_NONE;
}

// Grava as 3 chaves do cofre em /noir/cofre.bak (texto simples).
void backupToSD() {
    if (!backupEnsureSD()) {
        ui::messageBox("Backup", "Sem cartao SD.\nInsira um cartao e\ntente novamente.");
        return;
    }
    noir::config::begin();

    // Sem cofre criado ainda? Nao ha o que salvar.
    String salt  = noir::config::getStr("sec_salt", "");
    String vault = noir::config::getStr("sec_vault", "");
    String ctr   = noir::config::getStr("sec_ctr", "0");
    if (salt.length() == 0 || vault.length() == 0) {
        ui::messageBox("Backup", "Nenhum cofre para\nfazer backup. Crie o\ncofre primeiro.");
        return;
    }

    // Garante a pasta /noir.
    if (!SD.exists(BACKUP_DIR) && !SD.mkdir(BACKUP_DIR)) {
        ui::redStripe("Falha ao criar /noir");
        return;
    }

    // Monta o conteudo: uma chave por linha. O vault ja e ciphertext, entao o
    // arquivo e seguro de guardar (so serve com a senha-mestra original).
    String out;
    out += "salt=";  out += salt;  out += '\n';
    out += "vault="; out += vault; out += '\n';
    out += "ctr=";   out += ctr;   out += '\n';

    File w = SD.open(BACKUP_PATH, FILE_WRITE);   // FILE_WRITE trunca
    if (!w) { ui::redStripe("Falha ao abrir arquivo"); return; }
    size_t n = w.write((const uint8_t*)out.c_str(), out.length());
    w.close();
    if (n != out.length()) { ui::redStripe("Falha ao gravar (SD cheio?)"); return; }

    ui::messageBox("Backup", String("Cofre salvo em\n") + BACKUP_PATH +
                             "\n\nGuarde bem: so abre\ncom a senha-mestra.");
}

// Le /noir/cofre.bak, faz parse das 3 chaves e regrava na NVS (sobrescreve o
// cofre atual). Pede confirmacao (perigo) antes.
void restoreFromSD() {
    if (!backupEnsureSD()) {
        ui::messageBox("Restaurar", "Sem cartao SD.\nInsira um cartao e\ntente novamente.");
        return;
    }
    if (!SD.exists(BACKUP_PATH)) {
        ui::messageBox("Restaurar", String("Arquivo nao existe:\n") + BACKUP_PATH +
                                    "\nFaca um backup antes.");
        return;
    }

    // Le o arquivo inteiro (o backup e pequeno: 3 linhas curtas).
    File f = SD.open(BACKUP_PATH, FILE_READ);
    if (!f) { ui::redStripe("Falha ao ler arquivo"); return; }
    String buf;
    buf.reserve(f.size() + 1);
    while (f.available()) buf += (char)f.read();
    f.close();

    // Parse linha a linha: "chave=valor". Tolera CRLF do Windows.
    String salt, vault, ctr;
    int start = 0;
    while (start < (int)buf.length()) {
        int nl = buf.indexOf('\n', start);
        if (nl < 0) nl = buf.length();
        String line = buf.substring(start, nl);
        start = nl + 1;
        line.replace("\r", "");
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        String k = line.substring(0, eq);
        String v = line.substring(eq + 1);
        if      (k == "salt")  salt  = v;
        else if (k == "vault") vault = v;
        else if (k == "ctr")   ctr   = v;
    }

    if (salt.length() == 0 || vault.length() == 0) {
        ui::messageBox("Restaurar", "Backup invalido ou\nincompleto (faltam\nsalt/vault).");
        return;
    }
    if (ctr.length() == 0) ctr = "0";   // contador ausente = comeca do zero

    // Sobrescreve o cofre atual: acao destrutiva -> confirma em vermelho.
    if (!ui::confirm("Restaurar cofre",
                     "Isto SOBRESCREVE o\ncofre atual deste\naparelho. Continuar?", true)) {
        return;
    }

    noir::config::begin();
    noir::config::setStr("sec_salt",  salt);
    noir::config::setStr("sec_vault", vault);
    noir::config::setStr("sec_ctr",   ctr);

    // Se havia um cofre destravado em RAM, ele agora nao corresponde ao blob
    // restaurado: travamos para forcar um novo desbloqueio (com a senha do
    // backup) na proxima abertura do Cofre.
    vaultLock();

    ui::messageBox("Restaurar", "Cofre restaurado.\nAbra o Cofre e\ndesbloqueie com a\nsenha-mestra do backup.");
}

// App: menu de duas opcoes (backup / restaurar).
void appBackup() {
    for (;;) {
        int r = ui::listView("Cofre: Backup",
                             {"Backup p/ SD", "Restaurar do SD"}, 0, 1 /* restaurar = perigo */);
        if (r < 0) return;
        if (r == 0)      backupToSD();
        else if (r == 1) restoreFromSD();
    }
}

} // namespace anonimo

// ============================================================================
//  Exportacao da categoria. Nenhum app aqui transmite/destrói: danger=false.
// ============================================================================
const noir::AppEntry SEGURANCA_APPS[] = {
    { "Gerador de senha", "rng",   appGerador, false },
    { "Base64",           "codec", appBase64,  false },
    { "QR Code",          "qr",    appQr,      false },
    { "TOTP 2FA",         "otp",   appTotp,    false },
    { "Cofre",            "aes",   appCofre,   false },
    { "Cofre: Backup",    "bak",   appBackup,  false },
};
const int SEGURANCA_APPS_COUNT =
    (int)(sizeof(SEGURANCA_APPS) / sizeof(SEGURANCA_APPS[0]));

} // namespace seguranca
} // namespace apps
