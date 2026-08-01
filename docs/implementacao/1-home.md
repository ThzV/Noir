<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Modulo HOME - dashboard e clima

Este documento explica como o modulo `src/apps/home/` funciona: a tela inicial
(dashboard) e o app de configuracao do clima. E material de aprendizado, entao
o foco e o "porque" de cada decisao, nao so o "o que".

## Visao geral

O modulo entrega duas coisas, ambas declaradas em `home.h`:

| Simbolo | O que e |
| --- | --- |
| `apps::home::runDashboard()` | A tela inicial. Loop bloqueante que desenha relogio/data/clima/status e retorna no ENTER. |
| `apps::home::HOME_CFG_APPS[]` | Array com o app "Clima" (entra na categoria Config do menu). |

A integracao (fora do nosso modulo) liga esses simbolos ao sistema:
`app_registry.cpp` faz `dashboardApp()` devolver `runDashboard` e adiciona
`HOME_CFG_APPS` na categoria Config. Nos NAO editamos esse arquivo - so
exportamos o contrato de `home.h`.

## O ciclo do launcher

O `runLauncher()` (nucleo) roda assim, em loop:

1. Chama o dashboard (`runDashboard`), que **bloqueia** ate' o usuario apertar
   ENTER.
2. Quando `runDashboard` retorna, o launcher abre o menu principal.
3. Ao sair do menu, volta ao passo 1.

Ou seja: um "app" no Noir e' apenas uma funcao que roda ate' o usuario sair.
O dashboard segue o mesmo modelo - a unica diferenca e' que ele e' a primeira
tela e sai com ENTER (em vez da tecla de voltar).

## Anatomia do dashboard

### Desenho no canvas

Tudo e' desenhado em um unico sprite de tela cheia (`ui::gfx()`), o que evita
flicker. O padrao e' sempre:

```cpp
ui::clearNoir();          // fundo preto + grao de filme
ui::statusBar("NOIR");    // barra superior (bateria, titulo, relogio, WiFi)
// ... desenha o conteudo em ui::gfx() ...
ui::present();            // envia o sprite para o display
```

Elementos, de cima para baixo:

- **Relogio grande**: `fonts::Font7` (display de 7 segmentos), texto vindo de
  `noir::timeservice::hhmm()`. `middle_center` centraliza no eixo.
- **Data**: `noir::timeservice::dateStr()` quando ha sincronismo NTP; caso
  contrario o texto `"sem sync NTP"` (checado com `timeservice::have()`).
- **Clima**: uma linha com temperatura + descricao (detalhes abaixo).
- **Bateria + WiFi**: `M5.Power.getBatteryLevel()` e, quando conectado,
  `noir::wifi::rssi()` em dBm. Isso complementa o que a statusbar ja mostra
  (a statusbar so tem a %; aqui adicionamos o RSSI). Abaixo de 15% a linha
  pinta em `noir::BLOOD`, seguindo o padrao de alerta da statusbar.
- **Dica**: `ENTER menu   / atualiza clima`.

### Atualizacao por tempo (sem travar o input)

O loop e' **nao bloqueante** para o teclado: usa `ui::readKey()` (que retorna
`Key::None` quando nada foi pressionado) em vez de `ui::waitKey()`. Assim
conseguimos:

- Redesenhar o relogio ~1x por segundo comparando `millis() - lastClock`.
- Responder ao ENTER imediatamente.

```cpp
for (;;) {
    ui::KeyEvent e = ui::readKey();
    if (e.key == ui::Key::Enter) return;         // sai -> menu
    if (e.key == ui::Key::Right) { /* forca clima */ }
    if (millis() - lastClock >= 1000) { draw(); lastClock = millis(); }
    delay(10);   // cede CPU
}
```

O `delay(10)` evita um busy-loop a 100% de CPU (que so gastaria bateria).

## Clima (OpenWeatherMap)

### Estado em memoria

Guardamos o ultimo resultado numa struct `WeatherState` **arquivo-local**
(dentro de um namespace anonimo). Isso permite desenhar o clima em todo frame
sem refazer a requisicao HTTP. Buscamos da rede so:

- na primeira vez (`everFetched == false`);
- a cada ~10 minutos (`WEATHER_INTERVAL_MS`) quando o ultimo resultado foi `Ok`;
- a cada ~60 segundos (`WEATHER_ERROR_INTERVAL_MS`) enquanto o estado for de
  erro/sem-WiFi (backoff, ver "Tratamento de erros");
- quando o usuario forca com a tecla `/` (mapeada como `Key::Right`).

`maybeFetchWeather()` registra `lastFetch` no instante da **tentativa** (nao so
no sucesso). Assim, mesmo um fetch que falha "consome" o intervalo e o backoff
consegue segurar a proxima requisicao.

### A requisicao

Usamos `noir::net::get()` (cliente HTTP do nucleo) + **ArduinoJson v7**:

```cpp
String url = "https://api.openweathermap.org/data/2.5/weather?q=" +
             urlEncode(city) + "&units=" + units +
             "&lang=pt_br&appid=" + key;

noir::net::Resp resp = noir::net::get(url, {}, false, 6000);  // TLS valido, 6s
if (!resp.ok()) { /* status = Error */ }

JsonDocument doc;
if (deserializeJson(doc, resp.body)) { /* JSON invalido -> Error */ }
float temp        = doc["main"]["temp"].as<float>();
const char* desc  = doc["weather"][0]["description"] | "";
```

Pontos importantes:

- **`urlEncode`**: a cidade vem como `"Sao Paulo,BR"`. O espaco precisa virar
  `%20` na query. Escrevemos um encoder minimo que preserva letras/numeros e
  alguns simbolos (incluindo a virgula que separa cidade/pais) e percent-encoda
  o resto.
- **TLS validado (`insecure=false`)**: `api.openweathermap.org` e' um host
  publico com certificado de CA valido, entao validamos o cert (diferente de um
  homelab com cert self-signed, onde usariamos `insecure=true`).
- **Timeout curto (6 s)**: e' a tela inicial; nao queremos congelar por muito
  tempo. A requisicao e' bloqueante, mas acontece raramente (10 em 10 min).
- **ArduinoJson v7 (heap)**: o `JsonDocument` do v7 aloca seus dados no heap
  dinamicamente. Sem PSRAM isso preocuparia, mas o documento do clima e' pequeno,
  vive so dentro da funcao e e' liberado ao sair.
- **`| ""`**: o operador de "valor padrao" do ArduinoJson - se o campo faltar,
  usamos string vazia em vez de crashar.

### Tratamento de erros (nunca travar)

`fetchWeather()` cobre todos os caminhos ruins sem lancar excecao:

| Situacao | Status resultante | Texto na tela |
| --- | --- | --- |
| Sem `owm_key` salva | `Unconfigured` | `Clima: configure em Config` |
| WiFi desconectado | `NoWifi` | `Clima: sem WiFi` |
| Requisicao em andamento | `Updating` | `Clima: atualizando...` |
| HTTP != 2xx / JSON ruim / chave 401 | `Error` | `Clima: indisponivel` |
| Tudo certo | `Ok` | `23C  nuvens dispersas` |

Quando nao ha chave, **nem chamamos a API** - so ajustamos o status. Isso
respeita o requisito de nao gastar rede a toa.

**Backoff de erro (nao trave a tela):** um erro poderia virar uma "tempestade de
retries". O GET e' bloqueante (ate' 6 s) e o loop chama `maybeFetchWeather` a
cada ~1 s; se toda tentativa que falha deixasse a requisicao "vencida", o
dashboard ficaria disparando GET a cada frame, congelando o relogio e o ENTER.
Por isso: (1) so o estado `Ok` usa o intervalo de 10 min - erro/sem-WiFi usam
`WEATHER_ERROR_INTERVAL_MS` (60 s); e (2) `lastFetch` marca a **tentativa**, nao
o sucesso. Antes do GET bloqueante, o status vai para `Updating` e a tela e'
redesenhada (via callback `onBeforeFetch`), para que "atualizando..." apareca de
fato em vez de ser sobrescrito pelo resultado.

**Truncamento UTF-8:** a linha `Ok` e' limitada a 30 bytes para nao estourar a
largura. Como a descricao vem em `pt_br` (com acentos), cortamos numa **fronteira
de caractere** - recuamos enquanto o byte no corte for uma continuacao UTF-8
(`10xxxxxx`) - para nao deixar meio caractere e virar lixo na tela.

## App de configuracao "Clima"

`appClima()` (namespace anonimo) usa os widgets prontos:

1. `ui::textInput(..., mask=true)` para a chave da API (mascarada).
2. `ui::textInput(...)` para a cidade, com o valor atual como default.
3. Salva em NVS via `noir::config::setStr`.

Chaves NVS tem no maximo 15 caracteres, entao usamos nomes curtos:
`owm_key`, `owm_city`, `owm_units` (fixada em `metric` = Celsius).

Ao salvar, zeramos `g_weather.everFetched` para que o dashboard busque o clima
novo assim que voltar. O app e' so configuracao (nao ha TX), entao entra no
array com `danger=false`.

## Armadilhas e decisoes

- **Por que `readKey` e nao `waitKey`?** Porque precisamos redesenhar o relogio
  periodicamente mesmo sem tecla. `waitKey` bloquearia ate' o usuario tocar no
  teclado, congelando o relogio.
- **Por que o vermelho quase nao aparece?** No design Noir, `noir::BLOOD` e'
  reservado para perigo/TX. A unica excecao aqui e' a bateria fraca (< 15%),
  que segue o mesmo padrao ja usado pela statusbar.
- **Por que checar `is<float>()` antes de ler `main.temp`?** Uma chave invalida
  faz a OWM devolver `{"cod":401,...}` (sem `main`). Sem essa checagem,
  leriamos `NaN` e mostrariamos algo sem sentido; com ela, cai em `Error`.
- **Fetch bloqueante na tela inicial?** Sim, mas com timeout de 6 s e frequencia
  baixa. Para um dispositivo single-thread simples, e' um compromisso aceitavel
  e muito mais facil de entender do que HTTP assincrono.

## Contrato de integracao (resumo)

```cpp
// app_registry.cpp (feito pela integracao, NAO por nos):
AppRun dashboardApp() { return apps::home::runDashboard; }
// + adicionar HOME_CFG_APPS na categoria Config.
```

Dependencias: `bblanchon/ArduinoJson` (ja no `platformio.ini`). Nenhuma
`build_flag` extra.
