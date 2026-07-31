# Módulo 🌐 Rede

O maior módulo. Divide-se em **Reconhecimento (RX passivo — baixo risco)** e **Ferramentas (TX ativo — peso legal)**. 

> ⚠️ **Leia [legal-etica.md](../legal-etica.md) antes.** No menu, mantenha as duas seções separadas visualmente. O acento vermelho Noir acende sempre que há TX ativo.

---

## Parte A — Reconhecimento (RX passivo)

### A1. Scanner WiFi
- **Objetivo:** listar redes 2.4 GHz (SSID, BSSID, canal, RSSI, criptografia).
- **API:** `WiFi.scanNetworks()` → itera com `WiFi.SSID(i)`, `WiFi.RSSI(i)`, `WiFi.channel(i)`, `WiFi.encryptionType(i)`.
- **Bruce:** `src/modules/wifi/` — scanner completo com UI.
- **Noir:** lista com barras de sinal; cadeado para redes protegidas.
- **Dificuldade:** 🟢 Baixa. Ótimo primeiro módulo.

### A2. Scanner BLE
- **Objetivo:** listar dispositivos BLE próximos (nome, MAC, RSSI, serviços).
- **API:** **NimBLE-Arduino** (`NimBLEScan`), mais leve que o BLE do core.
- **Bruce:** `src/modules/ble/`.
- **Dificuldade:** 🟡 Média (conceitos de GATT/advertising).

### A3. DNS lookup
- **Objetivo:** resolver um domínio → IP (e reverso).
- **API:** `WiFi.hostByName(host, IPAddress&)`; para reverso, uma query manual.
- **Noir:** campo de texto (teclado) + resultado mono.
- **Dificuldade:** 🟢 Baixa.

### A4. Ping
- **Objetivo:** ICMP echo para um host (latência, perda).
- **API:** lib **ESP32Ping** (`Ping.ping(host, count)`).
- **Noir:** mostra min/avg/max e um gráfico de barras das respostas.
- **Dificuldade:** 🟢 Baixa.

### A5. Port scanner
- **Objetivo:** testar portas TCP abertas num host (ex.: 22, 80, 443, 8080...).
- **API:** `WiFiClient::connect(host, port)` com timeout curto; itere uma lista/intervalo.
- **Noir:** progresso + lista de portas abertas em verde-osso; use vermelho só se marcar como "scan agressivo".
- **Cuidado:** port scan em redes de terceiros pode ser ilegal — trate como TX-ish. Peça confirmação.
- **Dificuldade:** 🟡 Média (timeouts, não travar a UI — use task/async).

### A6. Speed test
- **Objetivo:** medir download/upload aproximado.
- **Como:** baixar um arquivo de tamanho conhecido de um servidor (ex.: seu próprio) e cronometrar; upload com POST. Serviços públicos de speedtest exigem protocolos específicos — comece com um arquivo estático seu.
- **API:** `HTTPClient` medindo bytes/tempo.
- **Dificuldade:** 🟡 Média.

---

## Parte B — Ferramentas ofensivas (TX ativo) 🔴

> **Todas exigem confirmação com aviso de escopo.** O acento vermelho Noir fica aceso enquanto transmitem. Só use em redes próprias/autorizadas.

### B1. Sniffer de pacotes
- **Objetivo:** capturar tráfego 802.11 (modo promíscuo) e salvar **.pcap** no SD.
- **API:** `esp_wifi_set_promiscuous(true)` + callback `esp_wifi_set_promiscuous_rx_cb`; gravar em formato PCAP.
- **Bruce:** `src/modules/wifi/` (sniffer/PCAP) — reaproveite o formato de arquivo e o callback.
- **Nota:** captura é RX, mas fixar canal e o uso do conteúdo têm implicações — trate como sensível.
- **Dificuldade:** 🔴 Alta.

### B2. Evil portal (captive portal)
- **Objetivo:** subir um AP + página captive que imita um login para **testes autorizados**.
- **API:** `WiFi.softAP()` + **DNS server** (redireciona tudo) + **ESPAsyncWebServer** servindo HTML.
- **Bruce:** `src/modules/wifi/` + HTML em `embedded_resources/web_interface/` → coloque em LittleFS.
- **Dificuldade:** 🔴 Alta (DNS hijack + servidor + captura).

### B3. Beacon spam
- **Objetivo:** transmitir muitos beacons de SSIDs falsos.
- **API:** injeção de frames com `esp_wifi_80211_tx()`.
- **Bruce:** `src/modules/wifi/`.
- **Dificuldade:** 🔴 Alta. **Interferência — cuidado legal máximo.**

### B4. Hotspot / AP
- **Objetivo:** criar um ponto de acesso (com ou sem portal).
- **API:** `WiFi.softAP(ssid, pass)`; opcional NAT/roteamento.
- **Dificuldade:** 🟡 Média.

### B5. Clone de WiFi / deauth
- **Objetivo:** clonar um SSID/AP (com evil portal) e/ou enviar frames de **deauth**.
- **API:** deauth via `esp_wifi_80211_tx()` com frames 802.11 de desautenticação.
- **Bruce:** `src/modules/wifi/` (deauth + clone via evil portal).
- **Dificuldade:** 🔴 Alta. **Deauth é interferência ativa — o caso de uso mais sensível legalmente.**

---

## Arquitetura do módulo (sugestão)
- Cada ferramenta é uma `Screen` em `src/modules/rede/<nome>/`.
- Operações longas (scan, sniff, spam) rodam em **task FreeRTOS** para não travar a UI; a tela lê estado/flags.
- Um "estado global de TX" acende o indicador vermelho na barra de status enquanto qualquer ferramenta transmite.
- Confirmação obrigatória (`modal`) antes de B1–B5, exibindo o aviso de escopo.

## Ordem recomendada de construção
1. Scanner WiFi (🟢) → pega o padrão de lista + task.
2. Ping / DNS (🟢) → entrada de texto + rede básica.
3. Scanner BLE (🟡).
4. Port scanner / speed test (🟡).
5. Sniffer (🔴) → primeiro contato com modo promíscuo.
6. AP/Hotspot (🟡) → base para evil portal.
7. Evil portal, beacon spam, deauth (🔴) → o topo da montanha.

## Recursos
- **WiFi Arduino:** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html>
- **esp_wifi (promiscuous/tx):** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html>
- **NimBLE-Arduino:** <https://github.com/h2zero/NimBLE-Arduino>
- **ESP32Ping:** <https://github.com/marian-craciunescu/ESP32Ping>
- **ESPAsyncWebServer:** <https://github.com/ESP32Async/ESPAsyncWebServer>
- **Formato PCAP:** <https://wiki.wireshark.org/Development/LibpcapFileFormat>
- **Bruce (referência de todos):** <https://github.com/pr3y/Bruce> · <https://deepwiki.com/pr3y/Bruce>
