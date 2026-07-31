# 03 — Arquitetura do OS

Como o Noir é organizado por dentro: do boot até um "app" rodando. O objetivo é uma arquitetura **simples, previsível e de baixa memória** (lembre: **sem PSRAM**).

## Visão em camadas

```
┌─────────────────────────────────────────────┐
│  APPS / MÓDULOS (screens/, modules/)          │  Home, Rede, Servidor, Segurança...
├─────────────────────────────────────────────┤
│  SHELL / LAUNCHER (ui/menu, ui/statusbar)     │  Navegação, menus, barra de status
├─────────────────────────────────────────────┤
│  UI TOOLKIT (ui/theme)  +  TEMA NOIR          │  Desenho, cores, fontes, grão
├─────────────────────────────────────────────┤
│  SERVIÇOS (wifi, ntp, storage, config, nvs)   │  Rede, tempo, SD/LittleFS, settings
├─────────────────────────────────────────────┤
│  HAL: M5Cardputer / M5Unified / M5GFX         │  Tela, teclado, áudio, energia
├─────────────────────────────────────────────┤
│  Arduino-ESP32 (pioarduino)  +  ESP-IDF       │  WiFi/BLE/mbedTLS/FreeRTOS
└─────────────────────────────────────────────┘
```

Você **quase nunca** desce abaixo da camada HAL — a M5 abstrai o hardware. Os módulos vivem no topo e usam os serviços do meio.

## Ciclo de vida (boot → loop)

```cpp
setup():
  M5Cardputer.begin(cfg)      // inicializa tela, teclado, energia, áudio
  Theme::init()               // carrega paleta Noir, fontes, sprites de grão
  Config::load()              // lê settings da NVS (fuso, brilho, SSIDs salvos...)
  Splash::play()              // animação/tela "NOIR"
  Router::go(HomeScreen)      // entra na home

loop():
  M5Cardputer.update()        // atualiza estado de teclado/energia (SEMPRE no início)
  Input::poll()               // traduz teclas → eventos (Up/Down/Sel/Esc/Char)
  Router::current()->tick()   // a tela ativa processa e desenha
```

**Regra de ouro:** `M5Cardputer.update()` no começo de todo loop. Sem isso, o teclado não atualiza.

## Sistema de menus (inspirado no Bruce)

O Bruce usa um padrão enxuto que vamos espelhar (ver [05 — reaproveitar Bruce](05-reaproveitar-bruce.md)):

- **`loopOptions(options)`** — recebe uma lista de opções `{ rótulo, callback }`, desenha, captura navegação (↑/↓/Enter/Esc) e chama o callback do item escolhido. É o coração do launcher.
- **Barra de status** desenhada no topo (bateria, WiFi, relógio) por cima de qualquer tela.
- **Modelo de "tela"** (`Screen`): uma classe/estrutura com `enter()`, `tick()`, `exit()`. O `Router` mantém uma pilha simples (entrar num submenu empilha; Esc desempilha).

> No esqueleto entregue, isso aparece como `src/ui/menu.*` (a lista navegável) e `src/screens/home.*` (a primeira tela). Cada módulo futuro vira uma nova `Screen`.

### Entrada de teclado
Padrão M5Cardputer:
```cpp
M5Cardputer.update();
if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
  auto st = M5Cardputer.Keyboard.keysState();
  if (st.enter) { /* selecionar */ }
  for (auto c : st.word) { /* caractere digitado */ }
  // setas: no Cardputer, mapeadas em `;`/`.`/`,`/`/` conforme layout — ver doc do teclado
}
```
O Bruce usa uma **task FreeRTOS** dedicada que seta flags voláteis (`NextPress/PrevPress/SelPress/EscPress`) — útil para não perder input durante operações longas (ex.: scan). Adote isso quando os módulos ficarem pesados.

## Gestão de memória (sem PSRAM!)

- **Sprites pequenos e reaproveitados.** Um `M5Canvas` (sprite) do tamanho da tela inteira (240×135×2 bytes ≈ 64 KB) é viável, mas **um só** — não crie vários grandes.
- **Evite `String` em loop.** Prefira buffers fixos (`char[]`) em caminhos quentes.
- **Imagens por partes.** O visualizador processa JPG/PNG por bandas, não carregando tudo na RAM.
- **JSON com cuidado.** Ao falar com APIs (servidor), use `ArduinoJson` com documentos dimensionados e filtros para não estourar a heap.

## Armazenamento e partições

Flash de **8 MB**, dividida por `partitions/custom_8Mb.csv` (espelha o esquema do Bruce):

| Partição | Uso |
|---|---|
| `nvs` | Configurações (fuso, brilho, credenciais, tokens) — via API `Preferences`/NVS. |
| `otadata` | Metadados de OTA (se habilitar atualização OTA). |
| `app0` (+ `app1`) | O firmware. Dois slots se quiser OTA; um só se quiser app maior. |
| `storage` (LittleFS) | Assets internos (fontes, sprites Noir, HTML do evil portal). |
| **microSD** | Arquivos do usuário, logs, imagens, capturas (.pcap). |

**Onde guardar o quê:**
- **Segredos/config** → NVS (opcionalmente com *flash encryption*).
- **Assets do firmware** → LittleFS (`storage`).
- **Dados do usuário / grandes** → microSD.

## Concorrência
- **Núcleo 0 / Núcleo 1:** dá para dedicar tarefas (ex.: sniffer WiFi, input handler) a um núcleo e manter a UI fluida no outro.
- **FreeRTOS** já está disponível (vem do ESP-IDF por baixo do Arduino). Use `xTaskCreatePinnedToCore` para tarefas de fundo.

## Convenções de código (sugestão)
- `snake_case` para arquivos, `PascalCase` para tipos, `camelCase` para funções/variáveis.
- Um módulo = uma pasta em `src/modules/<nome>/` com sua própria `Screen`.
- Cabeçalho de licença **AGPL-3.0** no topo de cada arquivo `.cpp/.h` (ver [05](05-reaproveitar-bruce.md)).
- Strings de UI centralizadas (facilita manter o tom "Noir" e traduzir).

## Recursos de aprendizado
- 📘 **DeepWiki do Bruce (arquitetura destrinchada):** <https://deepwiki.com/pr3y/Bruce>
- 📘 **M5Unified (camada HAL):** <https://github.com/m5stack/M5Unified>
- 📘 **FreeRTOS no ESP-IDF:** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html>
- 📘 **Partições no ESP32:** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/partition-tables.html>
- 📘 **LittleFS Arduino:** <https://github.com/lorol/LITTLEFS>

## Próximo passo
➡️ Dê alma ao projeto: **[04 — Design system Noir](04-design-noir.md)**.
