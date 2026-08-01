// ============================================================================
//  Noir OS  -  Modulo REDE / Reconhecimento (RX passivo)
//
//  Seis ferramentas de baixo risco que apenas OBSERVAM a rede (nao injetam
//  frames 802.11 nem destroem nada), por isso todas entram no menu com
//  danger=false:
//
//    1) Scan WiFi     - lista redes 2.4GHz (SSID, RSSI, canal, cadeado, BSSID).
//    2) Scan BLE      - varre dispositivos Bluetooth LE proximos (~5s).
//    3) DNS lookup    - resolve um host para IP.
//    4) Ping          - ICMP echo, calcula min/avg/max e perda %.
//    5) Port scanner  - testa portas TCP comuns num host (pede confirmacao).
//    6) Speed test    - baixa um recurso conhecido e estima Mbps.
//
//  Cada app e' uma funcao void() arquivo-local (namespace anonimo) que roda
//  um loop bloqueante com os widgets do nucleo e RETORNA quando o usuario sai.
//  Esse e' o mesmo padrao do app de referencia src/core/wifi_config.cpp.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#include "apps/rede/rede_recon.h"

#include "core/wifi_service.h"
#include "core/config.h"
#include "core/net.h"
#include "ui/widgets.h"
#include "ui/input.h"

#include <Arduino.h>
#include <WiFi.h>          // WiFi.hostByName, WiFiClient (port scanner/DNS)
#include <algorithm>
#include <vector>

// Bibliotecas externas (declaradas no manifesto de retorno; a integracao as
// adiciona em platformio.ini):
#include <NimBLEDevice.h>  // h2zero/NimBLE-Arduino  (scan BLE)
#include <ESP32Ping.h>     // marian-craciunescu/ESP32Ping (ping ICMP)

namespace {

// ---------------------------------------------------------------------------
//  Helpers internos
// ---------------------------------------------------------------------------

// Garante WiFi conectado antes de um app que precisa de rede. Mostra um aviso
// e retorna false se nao houver conexao (o app deve abortar nesse caso).
bool exigirWifi() {
    if (noir::wifi::ensure()) return true;
    ui::messageBox("Rede", "Sem WiFi.\nConfigure em Config > WiFi\ne tente de novo.");
    return false;
}

// Converte um RSSI (dBm) numa "forca" textual curta para caber na lista.
// -30 = colado no AP ... -90 = quase sumindo.
const char* forcaSinal(int rssi) {
    if (rssi >= -55) return "otimo";
    if (rssi >= -67) return "bom";
    if (rssi >= -78) return "fraco";
    return "ruim";
}

// ---------------------------------------------------------------------------
//  1) Scan WiFi
//      Reaproveita noir::wifi::scan(). Lista as redes ordenadas por sinal com
//      um cadeado (*) para as protegidas; ao selecionar, mostra o detalhe
//      completo (BSSID, canal, criptografia).
// ---------------------------------------------------------------------------
void appScanWifi() {
    for (;;) {
        ui::progress("Scan WiFi", "Escaneando redes 2.4GHz...", 40);

        std::vector<noir::wifi::Network> nets;
        int n = noir::wifi::scan(nets);
        if (n <= 0) {
            if (!ui::confirm("Scan WiFi", "Nenhuma rede encontrada.\nEscanear novamente?"))
                return;
            continue;
        }

        // Ordena por sinal (mais forte primeiro) - igual ao app de referencia.
        std::sort(nets.begin(), nets.end(),
                  [](const noir::wifi::Network& a, const noir::wifi::Network& b) {
                      return a.rssi > b.rssi;
                  });

        // Monta os rotulos da lista: "SSID  -63dBm c6 *"
        std::vector<String> itens;
        itens.reserve(nets.size());
        for (const auto& net : nets) {
            String ssid = net.ssid.length() ? net.ssid : String("<oculto>");
            String lock = net.open ? "" : " *";        // * = protegida
            itens.push_back(ssid + "  " + String((int)net.rssi) + "dBm c" +
                            String((int)net.channel) + lock);
        }

        int sel = ui::listView("Scan WiFi", itens);
        if (sel < 0) return;                            // ` voltar = sai do app

        // Tela de detalhe da rede escolhida.
        const auto& net = nets[sel];
        String det;
        det += "SSID:  " + (net.ssid.length() ? net.ssid : String("<oculto>")) + "\n";
        det += "BSSID: " + net.bssid + "\n";
        det += "Canal: " + String((int)net.channel) + "\n";
        det += "RSSI:  " + String((int)net.rssi) + " dBm (" + forcaSinal(net.rssi) + ")\n";
        det += String("Cripto: ") + (net.open ? "ABERTA" : "protegida");
        ui::messageBox("Detalhe da rede", det);
        // Volta para a lista (loop) ate' o usuario dar ` na lista.
    }
}

// ---------------------------------------------------------------------------
//  2) Scan BLE  (NimBLE-Arduino)
//      NimBLE e' a stack Bluetooth LE leve. Fazemos um scan ATIVO de ~5s
//      (pede o "scan response", que costuma trazer o nome do dispositivo) e
//      listamos nome/MAC/RSSI. Ao final liberamos o controlador BT para nao
//      brigar por memoria com o WiFi.
// ---------------------------------------------------------------------------
void appScanBle() {
    ui::progress("Scan BLE", "Iniciando radio BLE...", 15);

    // Inicializa a stack. O nome ("") e' o nome que ESTE dispositivo anunciaria;
    // como so' escutamos, deixamos vazio.
    NimBLEDevice::init("");

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);   // ativo = tambem pede scan response (mais nomes)
    scan->setInterval(45);       // parametros em unidades de 0.625ms
    scan->setWindow(15);         // window <= interval

    ui::progress("Scan BLE", "Escutando anuncios (~5s)...", 55);

    // getResults(duracaoMs, continuar). Bloqueia ~5s e devolve o resultado.
    NimBLEScanResults resultados = scan->getResults(5000, false);
    int total = resultados.getCount();

    // Copiamos os dados para Strings nossas ANTES de liberar o radio. Usamos
    // dois vetores paralelos: 'linhas' e' o que aparece na lista e 'detalhes'
    // guarda o texto completo (MAC) mostrado ao selecionar um dispositivo.
    std::vector<String> linhas;      // o que aparece na lista
    std::vector<String> detalhes;    // texto do messageBox de cada um
    linhas.reserve(total);
    detalhes.reserve(total);
    for (int i = 0; i < total; i++) {
        const NimBLEAdvertisedDevice* dev = resultados.getDevice(i);
        if (!dev) continue;
        String nome = dev->getName().empty() ? String("<sem nome>")
                                             : String(dev->getName().c_str());
        String mac  = String(dev->getAddress().toString().c_str());
        int    rssi = dev->getRSSI();

        linhas.push_back(nome + "  " + String(rssi) + "dBm");
        String d;
        d += "Nome: " + nome + "\n";
        d += "MAC:  " + mac + "\n";
        d += "RSSI: " + String(rssi) + " dBm (" + forcaSinal(rssi) + ")";
        detalhes.push_back(d);
    }

    // Libera memoria do scan e desliga o controlador BT (coexistencia c/ WiFi).
    scan->clearResults();
    NimBLEDevice::deinit(true);

    if (linhas.empty()) {
        ui::messageBox("Scan BLE", "Nenhum dispositivo BLE\nencontrado por perto.");
        return;
    }

    // Loop lista -> detalhe -> lista, ate' o usuario voltar (`).
    int start = 0;
    for (;;) {
        int sel = ui::listView("Scan BLE", linhas, start);
        if (sel < 0) return;
        start = sel;                       // reabre a lista no mesmo item
        ui::messageBox("Dispositivo BLE", detalhes[sel]);
    }
}

// ---------------------------------------------------------------------------
//  3) DNS lookup
//      Pergunta um host e usa WiFi.hostByName() para resolver o IP.
// ---------------------------------------------------------------------------
void appDnsLookup() {
    if (!exigirWifi()) return;

    for (;;) {
        bool ok = true;
        String host = ui::textInput("DNS lookup", "example.com", false, &ok);
        if (!ok) return;
        host.trim();
        if (host.length() == 0) { ui::redStripe("Host vazio"); continue; }

        ui::progress("DNS lookup", "Resolvendo " + host + "...", 50);

        IPAddress ip;
        // hostByName retorna 1 (sucesso) e preenche 'ip'.
        int r = WiFi.hostByName(host.c_str(), ip);
        if (r == 1) {
            ui::messageBox("DNS lookup",
                           "Host:\n" + host + "\n\nIP:\n" + ip.toString());
        } else {
            ui::messageBox("DNS lookup",
                           "Falha ao resolver:\n" + host +
                           "\n\n(dominio inexistente\nou sem DNS)");
        }
        // Volta ao textInput para tentar outro host; ` sai.
    }
}

// ---------------------------------------------------------------------------
//  4) Ping (ESP32Ping)
//      A lib ESP32Ping expoe Ping.ping(host, count) e as medias globais. Para
//      conseguir a PERDA % de forma confiavel, fazemos N pings de 1 pacote num
//      laco e contamos nos mesmos os sucessos e os tempos (min/avg/max).
// ---------------------------------------------------------------------------
void appPing() {
    if (!exigirWifi()) return;

    const int N = 10;   // quantidade de pacotes

    for (;;) {
        bool ok = true;
        String host = ui::textInput("Ping", "1.1.1.1", false, &ok);
        if (!ok) return;
        host.trim();
        if (host.length() == 0) { ui::redStripe("Host vazio"); continue; }

        int   recebidos = 0;
        float somaMs = 0, minMs = 1e9f, maxMs = 0;

        for (int i = 0; i < N; i++) {
            ui::progress(("Ping " + host).c_str(), "Pacote " + String(i + 1) + "/" + String(N),
                         (i + 1) * 100 / N);

            // Um pacote por chamada -> assim sabemos exatamente quais chegaram.
            bool sucesso = Ping.ping(host.c_str(), 1);
            if (sucesso) {
                recebidos++;
                float t = Ping.averageTime();   // com count=1, e' o RTT desse ping
                somaMs += t;
                if (t < minMs) minMs = t;
                if (t > maxMs) maxMs = t;
            }
        }

        int perda = (N - recebidos) * 100 / N;
        String res;
        res += "Host: " + host + "\n";
        res += "Enviados: " + String(N) + "  Recebidos: " + String(recebidos) + "\n";
        res += "Perda: " + String(perda) + "%\n";
        if (recebidos > 0) {
            res += "min:  " + String(minMs, 1) + " ms\n";
            res += "avg:  " + String(somaMs / recebidos, 1) + " ms\n";
            res += "max:  " + String(maxMs, 1) + " ms";
        } else {
            res += "\nNenhuma resposta (host\ninacessivel ou bloqueando ICMP).";
        }
        ui::messageBox("Resultado do ping", res);
        // Volta ao textInput; ` sai.
    }
}

// ---------------------------------------------------------------------------
//  5) Port scanner
//      Testa uma lista de portas TCP comuns via WiFiClient::connect() com
//      timeout curto. E' sequencial e usa ui::progress para nao dar a sensacao
//      de travamento. Pede CONFIRMACAO antes, pois varrer portas de terceiros
//      pode ser sensivel/ilegal (mesmo sendo tecnicamente RX/TCP).
// ---------------------------------------------------------------------------
void appPortScanner() {
    if (!exigirWifi()) return;

    // Portas comuns (as pedidas no escopo do modulo).
    static const uint16_t PORTAS[] = {
        21, 22, 23, 80, 443, 3306, 8080, 8096, 9000, 32400
    };
    const int NP = sizeof(PORTAS) / sizeof(PORTAS[0]);

    for (;;) {
        bool ok = true;
        String host = ui::textInput("Port scan", "192.168.1.1", false, &ok);
        if (!ok) return;
        host.trim();
        if (host.length() == 0) { ui::redStripe("Host vazio"); continue; }

        // Aviso de escopo + confirmacao (danger=true so' pinta o botao SIM de
        // vermelho; o app em si continua sendo danger=false no menu).
        if (!ui::confirm("Port scan",
                         "Varrer portas de:\n" + host +
                         "\n\nSo' em redes proprias\nou autorizadas. Seguir?",
                         true)) {
            continue;
        }

        // Resolve o host uma vez (mais rapido do que resolver a cada porta).
        IPAddress ip;
        if (WiFi.hostByName(host.c_str(), ip) != 1) {
            ui::messageBox("Port scan", "Nao resolvi o host:\n" + host);
            continue;
        }

        std::vector<String> abertas;
        for (int i = 0; i < NP; i++) {
            uint16_t porta = PORTAS[i];
            ui::progress(("Port scan " + host).c_str(),
                         "Testando porta " + String(porta) + "...",
                         (i + 1) * 100 / NP);

            WiFiClient cliente;
            // connect(ip, porta, timeoutMs): timeout curto p/ nao arrastar.
            if (cliente.connect(ip, porta, 400)) {
                abertas.push_back("Porta " + String(porta) + "  ABERTA");
                cliente.stop();
            }
        }

        if (abertas.empty()) {
            ui::messageBox("Port scan",
                           "Host: " + host + "\n\nNenhuma das " + String(NP) +
                           " portas\ncomuns respondeu.");
        } else {
            // Cabecalho + lista das abertas num listView (so' leitura).
            abertas.insert(abertas.begin(), String("== ") + host + " ==");
            ui::listView("Portas abertas", abertas);
        }
        // Volta ao textInput; ` sai.
    }
}

// ---------------------------------------------------------------------------
//  6) Speed test
//      Baixa um recurso conhecido com noir::net::get() medindo bytes/tempo e
//      estima os Mbps de download. A URL fica em config ("spd_url").
//
//      ATENCAO (sem PSRAM): net::get() carrega o corpo INTEIRO numa String na
//      RAM. Por isso o padrao baixa ~100KB (endpoint da Cloudflare que devolve
//      exatamente o numero de bytes pedido). NAO aponte para arquivos grandes.
// ---------------------------------------------------------------------------
void appSpeedTest() {
    if (!exigirWifi()) return;

    // URL configuravel. Default: Cloudflare devolve 'bytes' bytes de lixo.
    const char* DEFAULT_URL = "https://speed.cloudflare.com/__down?bytes=100000";
    String url = noir::config::getStr("spd_url", DEFAULT_URL);

    // Deixa o usuario editar a URL (e persistir) antes de rodar.
    bool ok = true;
    String nova = ui::textInput("Speed test URL", url, false, &ok);
    if (!ok) return;
    nova.trim();
    if (nova.length() == 0) nova = DEFAULT_URL;
    if (nova != url) noir::config::setStr("spd_url", nova);   // persiste a escolha
    url = nova;

    ui::progress("Speed test", "Baixando amostra...", 30);

    uint32_t t0 = millis();
    noir::net::Resp r = noir::net::get(url, {}, /*insecure=*/true, /*timeout=*/15000);
    uint32_t dtMs = millis() - t0;
    if (dtMs == 0) dtMs = 1;   // evita divisao por zero em links absurdamente rapidos

    if (!r.ok()) {
        ui::messageBox("Speed test",
                       "Falha no download.\nHTTP: " + String(r.code) +
                       "\n\nURL:\n" + url);
        return;
    }

    size_t bytes = r.body.length();
    // Mbps = (bytes * 8 bits) / (segundos) / 1e6.
    float mbps = (float)bytes * 8.0f / ((float)dtMs / 1000.0f) / 1e6f;
    float kb   = (float)bytes / 1024.0f;

    String sub = String(kb, 0) + " KB em " + String(dtMs) + " ms";
    ui::banner("Speed test", String(mbps, 2) + " Mbps", sub);
}

} // namespace (apps arquivo-local)

// ---------------------------------------------------------------------------
//  Contrato de exportacao: array consumido pelo app_registry na integracao.
//  Todos danger=false (reconhecimento RX passivo, sem TX ofensivo).
// ---------------------------------------------------------------------------
namespace apps {
namespace rede {

const noir::AppEntry RECON_APPS[] = {
    {"Scan WiFi",    "redes", appScanWifi,    false},
    {"Scan BLE",     "bt",    appScanBle,     false},
    {"DNS lookup",   "dns",   appDnsLookup,   false},
    {"Ping",         "icmp",  appPing,        false},
    {"Port scanner", "tcp",   appPortScanner, false},
    {"Speed test",   "mbps",  appSpeedTest,   false},
};
const int RECON_APPS_COUNT = sizeof(RECON_APPS) / sizeof(RECON_APPS[0]);

} // namespace rede
} // namespace apps
