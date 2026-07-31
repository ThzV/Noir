# Recursos de aprendizado

Todos os links de estudo do projeto reunidos por tema. Marcados por nível: 🟢 iniciante · 🟡 intermediário · 🔴 avançado.

## Fundamentos ESP32-S3 / Arduino
- 🟢 **Random Nerd Tutorials (ESP32):** <https://randomnerdtutorials.com> — a melhor fonte "mão na massa" para começar.
- 🟢 **TechTutorialsX (ESP32):** <https://techtutorialsx.com>
- 🟡 **Arduino-ESP32 (docs oficiais):** <https://docs.espressif.com/projects/arduino-esp32/en/latest/>
- 🔴 **ESP-IDF Programming Guide (S3):** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/>
- 🔴 **FreeRTOS no ESP32:** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html>

## PlatformIO
- 🟢 **Instalação no VS Code:** <https://docs.platformio.org/en/latest/integration/ide/vscode.html>
- 🟡 **`platformio.ini` (referência):** <https://docs.platformio.org/en/latest/projectconf/index.html>
- 🟡 **Plataforma pioarduino:** <https://github.com/pioarduino/platform-espressif32>

## Cardputer & bibliotecas M5
- 🟢 **Cardputer v1.1 (doc oficial):** <https://docs.m5stack.com/en/core/Cardputer%20V1.1>
- 🟢 **Quickstart Arduino Cardputer:** <https://docs.m5stack.com/en/arduino/m5cardputer/program>
- 🟢 **Teclado do Cardputer:** <https://docs.m5stack.com/en/arduino/m5cardputer/keyboard>
- 🟡 **M5Cardputer (repo):** <https://github.com/m5stack/M5Cardputer>
- 🟡 **M5Unified (repo):** <https://github.com/m5stack/M5Unified>
- 🟡 **M5GFX (repo):** <https://github.com/m5stack/M5GFX>
- 🟡 **M5GFX API:** <https://docs.m5stack.com/en/arduino/m5gfx/m5gfx>
- 🟡 **M5GFX na prática (lang-ship):** <https://m5stack.lang-ship.com/howto/m5gfx/basic/>
- 🟢 **Referência de teclado (comunidade):** <https://github.com/RetroBreeze/cardputer-keyboard-reference>

## Firmwares de referência (estudar arquitetura)
- 🔴 **Bruce:** <https://github.com/pr3y/Bruce> · DeepWiki: <https://deepwiki.com/pr3y/Bruce>
- 🟡 **Launcher (bmorcelli):** <https://github.com/bmorcelli/Launcher>
- 🟡 **M5Stick-NEMO (n0xa):** <https://github.com/n0xa/m5stick-nemo> — menu M5Unified limpo, bom ponto de partida.
- 🟡 **m5-launcher (konsumer):** <https://github.com/konsumer/m5-launcher>

## Rede (scanners e ferramentas)
- 🟡 **WiFi (Arduino-ESP32):** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html>
- 🔴 **esp_wifi (promiscuous/injeção):** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html>
- 🟡 **NimBLE-Arduino (BLE):** <https://github.com/h2zero/NimBLE-Arduino>
- 🟡 **ESP32Ping:** <https://github.com/marian-craciunescu/ESP32Ping>
- 🟡 **AsyncTCP / ESPAsyncWebServer (evil portal, APIs):** <https://github.com/ESP32Async/ESPAsyncWebServer>

## Servidor / homelab (clientes REST)
- 🟡 **HTTPClient (Arduino-ESP32):** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/http.html>
- 🟡 **ArduinoJson:** <https://arduinojson.org>
- 🟡 **Portainer API:** <https://docs.portainer.io/api/docs>
- 🟡 **Docker Engine API:** <https://docs.docker.com/engine/api/>
- 🟡 **AdGuard Home API (OpenAPI):** <https://github.com/AdguardTeam/AdGuardHome/tree/master/openapi>
- 🟡 **Uptime Kuma (API/monitor):** <https://github.com/louislam/uptime-kuma/wiki>

## Segurança (cofre, TOTP, QR)
- 🟡 **mbedTLS (AES/SHA/Base64) — já no core ESP32.** Guia: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/protocols/mbedtls.html>
- 🟢 **Preferences/NVS (guardar segredos):** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html>
- 🟡 **TOTP-Arduino (lucadentella):** <https://www.arduinolibraries.info/libraries/totp-library>
- 🟡 **RFC 6238 (TOTP):** <https://datatracker.ietf.org/doc/html/rfc6238>
- 🟡 **ricmoo/QRCode:** <https://github.com/ricmoo/QRCode>
- 🔴 **SecureGen (cofre/TOTP em ESP32 — referência):** <https://github.com/makepkg/SecureGen>

## Relógio / clima
- 🟢 **NTP com `configTime()`:** <https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/>
- 🟡 **Cardputer NTP (exemplo real):** <https://github.com/PaulskPt/M5Stack_Cardputer_NTP_syncd_clock>
- 🟡 **OpenWeatherMap + ESP32:** <https://randomnerdtutorials.com/esp32-http-get-open-weather-map-thingspeak-arduino/>

## Design / gráficos
- 🎨 **Conversor TTF→GFX font:** <https://rop.nl/truetype2gfx/>
- 🎨 **Floyd–Steinberg dithering:** <https://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering>

## Legalidade / ética
- 📕 Ver **[legal-etica.md](legal-etica.md)** para as fontes jurídicas.

---
*Sugestão de estudo: siga a ordem dos docs (00→05) e, ao chegar em cada módulo, abra os links 🟢 primeiro. Aprender fazendo > ler tudo antes.*
