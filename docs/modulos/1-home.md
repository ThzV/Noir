# Módulo 🏠 Home

A primeira tela do Noir: relógio, clima, bateria e WiFi. É onde a identidade Noir aparece com mais força.

## Objetivo
- **Relógio** grande, sincronizado por **NTP** (o Cardputer não tem RTC persistente).
- **Clima** do dia (temperatura + condição) via API.
- **Bateria** (%) e **status de WiFi** na barra de status.
- Ponto de entrada para o menu principal (as 6 categorias).

## APIs / bibliotecas
| Função | O que usar |
|---|---|
| Hora | `configTzTime()` / `getLocalTime()` (SNTP embutido no core) |
| WiFi | `WiFi.begin()`, `WiFi.status()`, `WiFi.RSSI()` |
| Clima | `HTTPClient` + `WiFiClientSecure` + `ArduinoJson` (API OpenWeatherMap) |
| Bateria | `M5.Power.getBatteryLevel()` (via M5Unified) |
| Persistência | `Preferences` (NVS) — guardar fuso, SSID, cidade, chave da API |

## Reaproveitamento do Bruce
- **Barra de status:** `drawStatusBar()` do Bruce (`src/core/display.cpp`) mostra bateria e WiFi — bom modelo para a nossa versão Noir.
- **Config de WiFi:** o Bruce tem fluxo de conectar/salvar redes em `src/core/` — reutilize a lógica de armazenar credenciais na NVS.

## Passos de implementação
1. **Conectar ao WiFi** (tela de config: escolher SSID do scan + digitar senha; salvar na NVS).
2. **Sincronizar NTP:** ao conectar, chamar `configTzTime("<TZ>", "pool.ntp.org")`. Para o Brasil: `TZ="<-03>3"` (ou use um mapeamento de fuso). Guardar timestamp do último sync.
3. **Relógio:** a cada segundo, `getLocalTime(&tm)` e desenhar HH:MM (grande, fonte art-déco) + data menor.
4. **Clima:** a cada N minutos, GET na OpenWeatherMap (`/data/2.5/weather?q=<cidade>&units=metric&appid=<key>`), parsear com ArduinoJson, exibir temp + ícone da condição (mapeado para um sprite Noir 1-bit).
5. **Bateria/WiFi:** ler a cada loop e desenhar na barra de status; vermelho se bateria <15%.
6. **Entrada de menu:** Enter na home abre o menu das 6 categorias (`loopOptions`).

## Detalhes que importam (Noir)
- Relógio no centro, enorme; data e clima em cinza (`NOIR_STEEL`) abaixo.
- Segundos podem "piscar" o separador `:` (feito noir de relógio de estação).
- Sem rede: mostrar o horário desde o boot com um aviso discreto "⚠ sem sync".
- Grão + vinheta por cima de tudo.

## Dificuldade
🟢 **Baixa-média.** WiFi + NTP + HTTP GET são padrões bem documentados. O desafio real é o polimento visual.

## Recursos
- **NTP no ESP32:** <https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/>
- **Exemplo NTP no Cardputer:** <https://github.com/PaulskPt/M5Stack_Cardputer_NTP_syncd_clock>
- **OpenWeatherMap + ESP32:** <https://randomnerdtutorials.com/esp32-http-get-open-weather-map-thingspeak-arduino/>
- **M5.Power (bateria):** <https://github.com/m5stack/M5Unified>
- **Fusos (TZ database):** <https://github.com/nayarsystems/posix_tz_db>
