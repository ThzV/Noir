# 05 — Reaproveitando o Bruce

O [Bruce](https://github.com/pr3y/Bruce) já implementa quase toda a parte ofensiva de rede que o Noir quer — e o **Cardputer é o alvo padrão dele**. Este guia explica **o que a licença exige**, **como o Bruce é organizado** e **como puxar módulos** para o Noir.

## ⚖️ 1. A licença muda tudo: Bruce é AGPL-3.0

O Bruce é licenciado sob **GNU AGPL-3.0** — *copyleft* forte, com **cláusula de rede**. Na prática:

- Se você **reusar código do Bruce** e **distribuir** (publicar no GitHub, dar o `.bin`, ou até expor por rede), você é **obrigado a**:
  1. Licenciar **o projeto inteiro combinado** como **AGPL-3.0**; e
  2. Disponibilizar o **código-fonte completo correspondente**.
- Você **não pode** relicenciar as partes derivadas do Bruce como MIT/Apache/proprietário.
- A cláusula "Affero" estende a obrigação até para uso **via rede** (relevante porque o Noir tem evil portal/serviços de rede).

**Decisão deste projeto:** adotamos **AGPL-3.0 no repositório inteiro** (o `LICENSE` foi trocado de Apache-2.0 para AGPL-3.0). Isso é o caminho mais simples e honesto para reusar o Bruce.

### Compatibilidade de licenças
| Origem | Licença | Combina com AGPL-3.0? |
|---|---|---|
| Bruce | AGPL-3.0 | ✅ (é a própria) |
| M5Cardputer / M5Unified / M5GFX | MIT | ✅ (MIT entra em AGPL) |
| ricmoo/QRCode | MIT | ✅ |
| mbedTLS | Apache-2.0 | ✅ (Apache entra em AGPL) |
| ArduinoJson | MIT | ✅ |

➡️ Tudo que precisamos é combinável **desde que o resultado final seja AGPL-3.0**.

### Higiene de licença (faça sempre)
- Cabeçalho **AGPL-3.0** no topo de cada arquivo fonte.
- **Preserve os créditos** e avisos de copyright do Bruce nos trechos reusados.
- Um `NOTICE`/seção no README citando o Bruce e sua licença.
- Não misture segredos/keys no repo público.

## 2. Como o Bruce é construído

| Aspecto | Detalhe |
|---|---|
| Build | **PlatformIO + Arduino** (usa componentes ESP-IDF via `idf_component.yml`). |
| Plataforma | Fork **pioarduino** do `espressif32` (habilita Arduino-core 3.x / ESP32-S3 novo). |
| Alvo padrão | `default_envs = m5stack-cardputer`. |
| Config do board | `boards/m5stack-cardputer/m5stack-cardputer.ini` (pinos, `custom_8Mb.csv`, `lib_deps`). |

> É exatamente por isso que o Noir também é **PlatformIO + pioarduino** — para o encaixe ser direto. Nosso `platformio.ini` e `partitions/custom_8Mb.csv` são baseados nesse env do Bruce.

## 3. Estrutura do repositório do Bruce

```
Bruce/
├── src/
│   ├── main.cpp                 # init + loop principal
│   ├── core/                    # framework: display, config, settings, sd_functions
│   │   └── menu_items/          # entradas do menu de topo
│   └── modules/                 # AS FEATURES — uma pasta por capacidade
│       ├── wifi/                # scanner, sniffer, evil portal, beacon spam, deauth, AP, DNS/port/host scan
│       ├── ble/                 # scanner BLE + spam (iOS/Android/Win/Samsung)
│       ├── rf/  rfid/  ir/      # SubGHz (CC1101), NFC (PN532), IR (TV-B-Gone)
│       ├── NRF24/ lora/ fm/ gps/ ethernet/
│       ├── pwnagotchi/ reverseShell/ badusb_ble/
│       ├── bjs_interpreter/     # interpretador JavaScript embarcado
│       └── others/
├── boards/                      # um .ini por placa
├── embedded_resources/web_interface/   # HTML do evil portal etc.
└── ...
```

## 4. Mapa: o que você quer × onde está no Bruce

| Recurso desejado (módulo Rede) | No Bruce? | Local |
|---|---|---|
| Scanner WiFi | ✅ | `src/modules/wifi/` |
| Scanner BLE | ✅ | `src/modules/ble/` |
| Sniffer de pacotes (PCAP) | ✅ | `src/modules/wifi/` |
| Evil portal (captive) | ✅ | `src/modules/wifi/` + `embedded_resources/web_interface/` |
| Beacon spam | ✅ | `src/modules/wifi/` |
| Hotspot / AP | ✅ | `src/modules/wifi/` |
| Clone de WiFi / deauth | ✅ | `src/modules/wifi/` |
| DNS lookup / ping / port scan | ✅ | `src/modules/wifi/` (ferramentas de host/porta) |

> Bônus que o Bruce traz (para o futuro): SubGHz/CC1101, NFC/RFID, IR (TV-B-Gone), NRF24, LoRa, BadUSB/HID, Pwnagotchi, reverse shell, interpretador JS.

## 5. O padrão de menu/input do Bruce (para integrar no Noir)

Peças-chave em `src/core/` que valem copiar/adaptar:

- **`loopOptions()`** — loop reutilizável que desenha uma lista de opções e captura navegação. É onde você "pendura" cada módulo.
- **`drawOptions()`** — renderização da lista.
- **`MainMenu`** — estrutura central de navegação; itens registrados em `core/menu_items/`.
- **`drawStatusBar()`** — bateria/WiFi no topo (nossa barra Noir se inspira nela).
- **`displayRedStripe()`** — faixa de aviso/erro (vira nossa faixa `NOIR_BLOOD`).
- **Input por flags via task FreeRTOS** — uma task seta `NextPress/PrevPress/SelPress/EscPress`; os loops leem essas flags. Bom para não perder tecla durante scans longos.

**Estratégia de integração recomendada:**
1. Construa o **shell do Noir** (home, menu, tema) com as libs M5 — já é o que o esqueleto faz.
2. Para cada ferramenta ofensiva, **traga o módulo `wifi`/`ble` do Bruce** para `src/modules/<nome>/`, ajuste as chamadas de UI para o **tema Noir** e registre uma `Screen` no menu Rede.
3. Reaproveite o **HTML do evil portal** de `embedded_resources/` (colocando em LittleFS).
4. Mantenha os arquivos derivados com **cabeçalho AGPL** e créditos.

> ⚠️ Cuidado de hardware: o `.ini` do Cardputer no Bruce lista `Adafruit TCA8418`. Esse controlador de teclado é do **Cardputer ADV (v2)** — na **v1.1** o teclado é matriz GPIO lida pela `M5Cardputer`. Não copie essa dependência para a v1.1.

## 6. Duas formas de reusar

| Abordagem | Como | Quando |
|---|---|---|
| **Portar arquivos** | Copiar `modules/wifi/*` para o Noir e adaptar a UI. | Máximo controle e visual Noir. Mais trabalho de integração. |
| **Estudar e reescrever** | Ler o Bruce como referência e reimplementar chamando as APIs Arduino/ESP-IDF diretamente (`esp_wifi_80211_tx`, `WiFi.scanNetworks`, `NimBLE`...). | Aprender a fundo; código 100% seu (ainda AGPL se inspirado de perto). |

Recomendação: **estudar + portar seletivamente**. Comece pelo scanner WiFi (simples, RX passivo) para pegar o padrão, depois avance.

## Recursos de aprendizado
- 📗 **Repo do Bruce:** <https://github.com/pr3y/Bruce>
- 📗 **DeepWiki do Bruce (arquitetura explicada):** <https://deepwiki.com/pr3y/Bruce>
- 📗 **Texto da AGPL-3.0:** <https://www.gnu.org/licenses/agpl-3.0.html>
- 📗 **Compatibilidade de licenças GNU:** <https://www.gnu.org/licenses/license-list.html>
- 📗 **esp_wifi (injeção/promiscuous mode):** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html>
- 📗 **NimBLE-Arduino (BLE):** <https://github.com/h2zero/NimBLE-Arduino>

## Próximo passo
➡️ Antes de qualquer TX: **[Legalidade e ética](legal-etica.md)**. Depois, mergulhe nos **[módulos](modulos/)**.
