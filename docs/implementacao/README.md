# Implementação — como o código funciona

Enquanto os guias em [`../modulos/`](../modulos/) explicam **como construir** cada parte (do zero, com recursos de estudo), esta pasta documenta **como o código que já existe no repositório funciona** — é o companheiro de leitura do `src/`.

## Arquitetura em 1 minuto

O Noir é organizado em **núcleo (core)** + **módulos (apps)**, colados por um **registro**.

```
main.cpp ── setup(): init HW + servicos + splash + WiFi/NTP
         └─ loop(): noir::runLauncher()
                       │
                       ├─ dashboard (apps/home/runDashboard)   ← tela inicial
                       └─ menu de categorias (app_registry)
                              └─ app.run()  ← cada ferramenta roda aqui (bloqueante)
```

### O núcleo (`src/core`, `src/ui`)
A "biblioteca padrão" do Noir. Todo módulo constrói em cima disto:

| Peça | Arquivo | O que oferece |
|---|---|---|
| Modelo de app | `core/app.h` | `AppEntry{name,hint,run,danger}`; flag global de TX (`setTxActive`) |
| Config/NVS | `core/config.*` | ler/gravar ajustes e segredos (chaves ≤15 chars) |
| WiFi | `core/wifi_service.*` | `scan`, `connectSaved`, `ensure`, status |
| Tempo (NTP) | `core/time_service.*` | relógio/data (sem RTC → NTP) |
| Rede HTTP | `core/net.*` | `get`/`post` (HTTP/HTTPS) + `basicAuth` → base do Servidor |
| Tema Noir | `ui/theme.*` + `include/noir_theme.h` | canvas, cores, grão/vinheta, `panel` |
| Barra de status | `ui/statusbar.*` | bateria, título, WiFi, relógio, alerta **TX** |
| Teclado | `ui/input.*` | `readKey`/`waitKey` → eventos `Up/Down/Enter/Back/...` |
| Widgets | `ui/widgets.*` | `messageBox`, `confirm`, `textInput`, `listView`, `progress`, `banner`, `redStripe` |
| Launcher | `core/launcher.*` | dashboard + menu + confirmação de apps de perigo |
| Registro | `core/app_registry.*` | monta as categorias a partir dos módulos |

**App de referência:** [`src/core/wifi_config.cpp`](../../src/core/wifi_config.cpp) mostra o padrão canônico (scan → `listView` → `textInput` → salvar → conectar).

### Os módulos (`src/apps/<módulo>`)
Cada módulo é uma pasta com apps. Um **app** é uma função `void()` que roda um loop bloqueante usando os widgets e retorna quando o usuário sai. O módulo exporta um array `AppEntry[]`; o `app_registry` o pendura numa categoria do menu. Apps de **perigo** (TX ativo/destrutivo) vêm por último, com `danger=true`, e o launcher exige confirmação antes de rodá-los.

## Os guias por módulo

| # | Módulo | Doc |
|---|---|---|
| 1 | Home (dashboard: relógio, clima, status) | [1-home.md](1-home.md) |
| 2 | Rede — reconhecimento (RX passivo) | [2-rede-recon.md](2-rede-recon.md) |
| 3 | Rede — ofensivo (TX ativo) ⚠️ | [3-rede-ofensivo.md](3-rede-ofensivo.md) |
| 4 | Servidor (homelab remoto) | [4-servidor.md](4-servidor.md) |
| 5 | Segurança (senhas, cofre, TOTP, QR) | [5-seguranca.md](5-seguranca.md) |
| 6 | Arquivos (SD: explorador, editor, imagens) | [6-arquivos.md](6-arquivos.md) |
| 7 | Produtividade (cronômetro, pomodoro, ...) | [7-produtividade.md](7-produtividade.md) |

## Como adicionar um novo app (receita)

1. Crie `src/apps/<seu_modulo>/<seu_modulo>.{h,cpp}`.
2. No `.cpp`, escreva funções `static void meuApp()` (namespace anônimo) usando os widgets.
3. Exporte um array:
   ```cpp
   namespace apps { namespace seu_modulo {
     const noir::AppEntry SEU_APPS[] = {
       {"Meu App", "dica", meuApp, false},
       // apps de perigo por ultimo, com danger=true
     };
     const int SEU_APPS_COUNT = sizeof(SEU_APPS)/sizeof(SEU_APPS[0]);
   }}
   ```
4. Declare os `extern` no header e inclua-o em `core/app_registry.cpp`, adicionando uma `CategoryRT`.
5. Compile: `pio run -e m5stack-cardputer`.

> Veja [../03-arquitetura.md](../03-arquitetura.md) para a visão de arquitetura conceitual e [../05-reaproveitar-bruce.md](../05-reaproveitar-bruce.md) para reaproveitar o Bruce nos módulos de rede.
