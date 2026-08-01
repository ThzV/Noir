<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Módulo Produtividade (offline)

Quatro ferramentas simples que **não usam rede**: cronômetro, conversor de
unidades, pomodoro e calendário. É um bom módulo para estudar porque cada app
mostra um padrão diferente de UI do Noir OS.

Arquivos:

- `src/apps/produtividade/produtividade.h` — contrato de exportação (só o array).
- `src/apps/produtividade/produtividade.cpp` — implementação dos 4 apps.

## O modelo de "app" do Noir

Um app é uma **função `void()` bloqueante**. Ela roda seu próprio laço,
desenha no canvas compartilhado (`ui::gfx()`) e **retorna** quando o usuário
sai. O `app_registry` monta o menu a partir de um array de `noir::AppEntry`:

```cpp
struct AppEntry { const char* name; const char* hint; AppRun run; bool danger; };
```

Todos os apps daqui são `danger=false` (nenhum transmite rádio nem apaga dados),
então não mexem em `noir::setTxActive()`.

### Por que tudo em um só `.cpp`?

As funções `run()` ficam em **namespace anônimo** (ligação interna, "arquivo-
local"). O array `PRODUTIVIDADE_APPS` precisa enxergá-las, e um símbolo de
namespace anônimo só é visível dentro do próprio arquivo. Logo, funções e array
convivem na mesma *translation unit*. Separar em vários `.cpp` exigiria expor as
funções num header — o que quebraria a regra "run() é arquivo-local".

## Dois estilos de laço

Repare na diferença entre `waitKey()` e `readKey()`:

- **`ui::waitKey()`** bloqueia até uma tecla. Use quando a tela é *estática*
  (menus, calendário): nada muda sem input.
- **`ui::readKey()`** retorna na hora (`Key::None` se nada). Use quando a tela é
  *animada* (cronômetro contando, pomodoro decrescendo): você redesenha a cada
  quadro e só reage se houver tecla.

Nos laços animados fechamos com um `delay(20~40)` para dar ~30–50 fps sem fritar
a CPU.

## App 1 — Cronômetro

Base de tempo: `millis()` (milissegundos desde o boot). O truque para start/stop
sem perder frações é guardar **dois** valores:

```cpp
uint32_t accum;      // tempo congelado quando pausado
uint32_t startMark;  // millis() no último "start"
// decorrido = running ? accum + (millis() - startMark) : accum;
```

Ao pausar, fazemos `accum = decorrido()` e paramos; ao retomar,
`startMark = millis()`. Assim a pausa nunca perde os centésimos.

**Voltas (laps):** guardamos em `std::vector<uint32_t>` o *tempo total* no
instante de cada volta. A duração da volta *i* é `laps[i] - laps[i-1]`. A lista
é rolável (setas `;`/`.`) e auto-rola para mostrar a volta recém-criada.

**Exibição grande `MM:SS.cc`:** ver `drawBigClock()` abaixo.

Teclas: `ENTER` inicia/pausa · `SPACE` marca volta · `DEL` zera (parado) ·
`` ` `` sai.

## App 2 — Conversor de unidades

A ideia central é o **fator para a unidade-base** de cada categoria:

```
valor_base   = valor * fator_origem
valor_destino = valor_base / fator_destino
```

Cada categoria (comprimento, massa, volume, dados, velocidade) tem uma tabela
`Unidade{ nome, fator }`. Ex.: comprimento tem base metro, então `km` = 1000,
`cm` = 0,01. Dados usam base **byte binário** (1 KB = 1024 B).

**Temperatura é a exceção** (tem *offset*, não só escala): convertemos usando
Celsius como pivô — `tempParaC()` e depois `tempDeC()`. Por isso a categoria tem
o flag `temperatura=true`, e o código desvia da fórmula de fator.

Fluxo (tudo com `ui::listView` + `ui::textInput`): categoria → unidade origem →
unidade destino → valor → `ui::banner` com o resultado. A entrada aceita vírgula
ou ponto (`s.replace(",", ".")`), e o resultado é formatado sem zeros inúteis.

## App 3 — Pomodoro

Ciclos de foco/pausa configuráveis, salvos na **NVS** (`noir::config`).
Lembrete importante: **chave de NVS tem no máximo 15 caracteres** — por isso as
chaves são curtas: `pm_work`, `pm_short`, `pm_long`, `pm_every`.

Máquina de estados de 3 fases:

```
FOCO --(N focos)--> PAUSA_LONGA
FOCO --(senão)----> PAUSA_CURTA --> FOCO ...
```

`pomos % every == 0` decide se a pausa é longa. Cada foco concluído incrementa o
contador.

**Contagem independente do desenho:** usamos um "tick" de 1 segundo baseado em
`millis()`, separado do fps de renderização:

```cpp
if (running && millis() - lastTick >= 1000) { lastTick += 1000; restanteS--; }
```

**Beep na transição:** `M5.Speaker.tone(freq, ms)` (M5Unified). Tocamos duas
notas — subida ao entrar na pausa, descida ao voltar ao foco — e damos um
"flash" branco na tela (`fillSprite(WHITE)`) para quem estiver sem som.

> Armadilha: o alto-falante precisa estar habilitado no boot
> (`M5.Speaker.begin()` / config do `M5Cardputer.begin()`). Se não estiver,
> `tone()` simplesmente não soa, mas não quebra.

A tela de config ajusta os 4 parâmetros com as setas e grava tudo só no `ENTER`.

## App 4 — Calendário

Grade do mês em 7 colunas. O ponto-chave é descobrir **em qual coluna cai o dia
1**. Fazemos isso com `mktime()`, que normaliza um `struct tm` e preenche
`tm_wday` (0 = domingo):

```cpp
struct tm t = {};
t.tm_year = ano - 1900;  t.tm_mon = mes;  t.tm_mday = 1;
t.tm_hour = 12;          t.tm_isdst = -1;   // meio-dia evita bordas de DST
mktime(&t);              // agora t.tm_wday é o dia da semana do dia 1
```

Dias no mês vêm de uma tabela, com fevereiro tratado por `bissexto()`.

**Precisa de NTP?** Só para saber que dia é **hoje** (o destaque branco). O
Cardputer v1.1 não tem RTC, então a hora vem de NTP (`noir::timeservice`). Se
ainda não há hora (`now()` retorna `false`), mostramos uma mensagem pedindo para
sincronizar e saímos — é o "trate sem hora" do requisito. A navegação por mês/ano
em si não depende de rede.

Teclas: `,`/`/` muda o mês · `;`/`.` muda o ano · `` ` `` sai.

## Utilitário compartilhado: `drawBigClock()`

Desenhar `MM:SS.cc` "grande" sem estourar os 240px é traiçoeiro: fontes têm
larguras diferentes. A solução é **medir com `textWidth()` em tempo de execução**
e centralizar o conjunto:

```cpp
setFont(Font7); wMain = textWidth("MM:SS");   // 7-segmentos, aspecto de relógio
setFont(Font4); wTail = textWidth(".cc");      // centésimos menores, ao lado
x0 = (SCREEN_W - (wMain + wTail)) / 2;         // centralizado, nunca corta
```

Assim o layout se auto-ajusta e é reaproveitado por cronômetro e pomodoro.

## Estética Noir aplicada

- Fundo sempre via `ui::clearNoir()` (preto + grão de filme + vinheta).
- Vermelho (`noir::BLOOD`) é **reservado a perigo/TX** — nenhum app aqui usa.
  O item selecionado é branco (`WHITE`) sobre texto preto; secundário é `STEEL`.
- Barra de status com `ui::statusBar(titulo)`; o título do calendário mostra
  "Mês Ano".
- Rodapé discreto com as dicas de tecla (`drawHints()`), fonte pequena.

## Restrições de hardware lembradas

- **Sem PSRAM:** nada de buffers grandes. As tabelas de unidades são `const`
  (vão para a flash), os vetores de laps crescem só com o uso real.
- ESP32-S3, tela 240×135, barra de status de 16px.
