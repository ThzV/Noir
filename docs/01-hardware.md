# 01 — Hardware do Cardputer v1.1

Entender o hardware evita 80% das dores de cabeça. Aqui está o que a placa **tem**, o que **não tem**, e os limites que moldam a arquitetura.

## Visão rápida

O Cardputer v1.1 é construído em cima do módulo **M5StampS3**, que usa o SoC **ESP32-S3FN8**.

| Item | Especificação | Impacto no projeto |
|---|---|---|
| **MCU** | ESP32-S3FN8, dual-core Xtensa LX7 @ 240 MHz | 2 núcleos → dá para usar tarefas FreeRTOS (ex.: input handler). |
| **Flash** | **8 MB** | Partição custom de 8 MB (ver [arquitetura](03-arquitetura.md)). |
| **PSRAM** | **NENHUMA** ⚠️ | Só ~512 KB de SRAM interna. Cuidado com buffers grandes de imagem/sprite. |
| **RAM interna** | ~512 KB SRAM | Orçamento de memória apertado — evitar cópias grandes. |
| **Rádio** | WiFi 2.4 GHz b/g/n + BLE 5 (LE) | Base de todos os módulos de rede. |
| **Tela** | 1.14", **240×135**, controlador **ST7789V2** | Área útil pequena e larga — UI horizontal. |
| **Teclado** | Matriz **56 teclas** (4×14), lida por GPIO | Lida via `M5Cardputer.Keyboard` (NÃO é TCA8418 na v1.1). |
| **RTC** | **NENHUM** ⚠️ | Relógio não sobrevive a reboot → usar **NTP** ou Unit RTC externo. |
| **Microfone** | SPM1423 (PDM/I2S) | Áudio-in (ex.: futuros módulos de som). |
| **Alto-falante** | NS4168, 8 Ω / 1 W (I2S) | Beeps/feedback sonoro. |
| **IR** | Transmissor IR embarcado (~4 m) | Possível módulo "controle remoto" no futuro. |
| **Armazenamento** | Slot microSD | Explorador de arquivos, assets, logs. |
| **Expansão** | 1× Grove HY2.0-4P (**I2C**) | Sensores, Unit RTC, etc. |
| **Energia** | Bateria interna 120 mAh + 1400 mAh na base | Ler nível via `M5.Power`. |
| **USB** | USB-C nativo do ESP32-S3 (VID `303A`) | Flash/serial/JTAG sem chip conversor — **sem driver extra no Windows**. |

## ⚠️ Duas correções importantes (leia antes de codar)

Muita informação solta na internet erra estes dois pontos:

### 1. Não tem PSRAM
Alguns textos citam o Cardputer como "ESP32-S3-WROOM-1U-N8R8" (o `R8` implicaria 8 MB de PSRAM). **O Cardputer v1.1 padrão não tem PSRAM** — usa o `ESP32-S3FN8` (8 MB flash, sem PSRAM). Só a variante separada *M5StampS3 BAT* tem PSRAM.
- **Consequência prática:** não configure `board_build.arduino.memory_type` para usar PSRAM; buffers grandes (ex.: framebuffer completo de imagem) precisam caber na SRAM. Renderize por partes / use sprites pequenos.
- **Confirmação cruzada:** o próprio Bruce não define PSRAM no env do Cardputer e usa uma tabela de partição `custom_8Mb.csv`.

### 2. Não tem chip RTC
O Cardputer v1.1 **não tem** um chip RTC dedicado com bateria. Ele só tem os timers RTC internos do ESP32-S3, que **zeram ao perder energia**.
- **Consequência prática:** o módulo de relógio depende de **sincronização NTP** ao conectar no WiFi. Sem rede, o horário só conta a partir do boot.
- **Alternativa opcional:** um **Unit RTC** (chip HYM8563/BM8563) plugado no Grove (I2C) dá horário persistente offline.

## Pinout essencial (v1.1)

> Você **quase nunca** precisa desses números diretamente — a lib `M5Cardputer`/`M5Unified` já configura tela, teclado, SD, áudio. Use esta tabela só para depurar ou plugar hardware no Grove.

| Função | Pinos (GPIO) | Observação |
|---|---|---|
| TFT (SPI) | SCLK 36, MOSI 35, DC 34, RST 33, CS 37, BL 38 | ST7789V2, gerenciado pela M5GFX. |
| microSD (SPI) | SCLK 40, MISO 39, MOSI 14, CS 12 | Compartilha barramento; `SD.begin(12)`. |
| Grove (I2C) | SDA 2, SCL 1 | Porta HY2.0-4P para periféricos externos. |
| IR TX | 44 | Transmissor infravermelho. |
| Speaker (I2S) | BCLK 41, LRCK 43, DATA 42 | NS4168. |
| Mic (PDM) | CLK 43, DATA 46 | SPM1423. |
| Teclado | matriz multiplexada | Ler via `M5Cardputer.Keyboard`, não por GPIO cru. |

*(Sempre confirme pinos na fonte oficial antes de soldar/plugar — ver links abaixo. Versões de placa podem mudar.)*

## Limites que moldam o design

- **Tela pequena e larga (240×135):** menus em lista vertical curta, fontes compactas, uma "coisa" principal por tela.
- **Sem PSRAM:** nada de carregar imagens enormes na RAM. Visualizador de imagem processa por linhas; sprites Noir são pequenos e reaproveitados.
- **Sem RTC:** relógio = NTP obrigatório; guarde o fuso e o último sync na NVS.
- **8 MB flash:** dá para um app grande + partição de dados (LittleFS) para assets. OTA é possível mas consome espaço (decidir no [roadmap](../ROADMAP.md)).
- **Bateria pequena (120 mAh interna):** telas escuras (Noir ajuda!), desligar rádio quando não usar, dimmer de brilho.

## Recursos de aprendizado

- 📄 **Doc oficial Cardputer v1.1:** <https://docs.m5stack.com/en/core/Cardputer%20V1.1>
- 📄 **M5StampS3 (o módulo):** <https://docs.m5stack.com/en/core/StampS3>
- 📄 **Datasheet ESP32-S3:** <https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf>
- 📄 **Referência do teclado (comunidade):** <https://github.com/RetroBreeze/cardputer-keyboard-reference>
- 📄 **Unit RTC (relógio externo opcional):** <https://docs.m5stack.com/en/unit/UNIT%20RTC>

## Próximo passo
➡️ Prepare a máquina: **[02 — Ambiente de desenvolvimento](02-ambiente-dev.md)**.
