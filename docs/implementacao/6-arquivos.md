<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Modulo Arquivos (cartao SD)

Este modulo implementa quatro apps que trabalham sobre o cartao microSD do
M5Stack Cardputer v1.1. O objetivo deste documento e' didatico: explicar como o
codigo funciona, quais APIs do Noir OS ele usa e as armadilhas que aparecem num
ESP32-S3 **sem PSRAM**.

Arquivos do modulo:

- `src/apps/arquivos/arquivos.h` — contrato de exportacao (`ARQUIVOS_APPS`).
- `src/apps/arquivos/arquivos.cpp` — implementacao dos quatro apps.

## Os quatro apps

| App           | Funcao interna | O que faz                                             |
|---------------|----------------|-------------------------------------------------------|
| Explorador    | `appExplorer`  | Navega pastas; abrir / renomear / apagar / criar pasta|
| Editor texto  | `appEditor`    | Cria ou abre um `.txt` pequeno e edita                |
| Notas         | `appNotes`     | Abre `/noir/notas.txt` direto, com autosave ao sair   |
| Imagens       | `appImages`    | Lista `.jpg/.png/.bmp` da raiz e navega com as setas  |

Todos seguem o **padrao de app do Noir**: uma funcao `void()` bloqueante que
roda um loop com os widgets e **retorna** quando o usuario sai (tecla crase `` ` ``
= voltar, ou um `listView`/`messageBox` que retorna).

## Inicializando o cartao SD

O Cartao do Cardputer usa um barramento SPI dedicado com estes pinos:

```
SCLK = 40   MISO = 39   MOSI = 14   CS = 12
```

A funcao `ensureSD()` cuida disso:

```cpp
bool ensureSD() {
    static bool spiStarted = false;
    if (!spiStarted) { SPI.begin(40, 39, 14, 12); spiStarted = true; }
    if (!SD.begin(12, SPI)) return false;
    return SD.cardType() != CARD_NONE;
}
```

Pontos importantes:

- `SPI.begin(...)` so' precisa rodar **uma vez** (guardado no `static`). Chamar
  de novo poderia reconfigurar o barramento sem necessidade.
- `SD.begin(12, SPI)` monta o cartao e e' seguro chamar toda vez que um app
  inicia — isso permite trocar o cartao entre usos.
- Se nao houver cartao, `SD.cardType()` devolve `CARD_NONE`. Nesse caso cada app
  chama `noCardMsg()` (um `ui::messageBox`) e retorna. **Nunca** presuma que o
  cartao existe.

## Lendo um diretorio

`listDir()` usa a API classica do Arduino `SD`:

```cpp
File dir = SD.open(path);
for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    ...
    e.close();
}
dir.close();
```

Detalhes:

- **`File::name()` no core ESP32 devolve o caminho completo** (ex.:
  `/noir/notas.txt`), nao so' o nome. Por isso existe o helper `baseName()`, que
  corta tudo ate' a ultima `/`.
- Limitamos a leitura a 200 entradas para nao estourar a RAM com pastas enormes.
- Ordenamos com `std::sort`: pastas primeiro, depois por nome (case-insensitive).

## Editor de texto (sem PSRAM)

O ponto mais delicado: **nao ha' PSRAM**. Um `String` grande fragmenta e esgota
o heap. Por isso definimos:

```cpp
constexpr size_t EDIT_LIMIT = 8 * 1024;   // 8 KB
```

- `loadFile()` le byte a byte para um `String` (`buf.reserve(lim+1)` evita
  realocacoes) ate' o limite. Se o arquivo for maior, devolve `false`
  (truncado).
- Arquivos truncados **nao** sao editaveis: `openTextFile()` avisa e chama
  `viewText()`, um visualizador **somente-leitura** com rolagem pelas setas.
- `saveFile()` grava com `SD.open(path, FILE_WRITE)`, que **trunca** o arquivo
  (modo `"w"`) e regrava o buffer inteiro.

### Como o editor le o teclado

O widget `ui::textInput` edita apenas uma linha. Para varias linhas, `editText()`
le o teclado **cru**, do mesmo jeito que o `textInput` faz internamente:

```cpp
M5Cardputer.update();
if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto ks = M5Cardputer.Keyboard.keysState();
    if (ks.del)   { /* backspace */ }
    if (ks.enter) { buf += '\n'; }
    for (char c : ks.word) {
        if (c == '`') { /* sair */ }
        buf += c;
    }
}
```

Decisoes de design:

- **Edicao so' no fim do buffer** (append/backspace/enter). E' o modelo mais
  simples e robusto; sem cursor livre nao ha' estados complicados para gerenciar.
- A tela sempre mostra o **fim** do texto (auto-scroll para o cursor). O cursor
  e' um `_` desenhado na ultima linha.
- A tecla crase `` ` `` sai do editor (convencao "voltar" do Noir). Isso significa
  que a crase **nao** pode ser digitada no texto — limitacao aceita e documentada.
- `wrapLines()` quebra o buffer em linhas de tela: respeita `\n`, ignora `\r`
  (tolera CRLF do Windows) e faz *wrap* a cada `TEXT_COLS` (40 colunas na Font0).

O Editor (`appEditor`) pergunta com `ui::confirm` antes de gravar. As Notas
(`appNotes`) fazem **autosave** — gravam sempre ao sair, sem perguntar.

## Visualizador de imagens

O M5GFX decodifica imagens **direto do cartao** (streaming), entao nao carregamos
o arquivo em RAM:

```cpp
d.drawPngFile(SD, path.c_str(), 0, noir::STATUSBAR_H);
d.drawBmpFile(SD, path.c_str(), 0, noir::STATUSBAR_H);
d.drawJpgFile(SD, path.c_str(), 0, noir::STATUSBAR_H);
```

- Desenhamos abaixo da `STATUSBAR_H` para nao cobrir a barra de status.
- A imagem e' desenhada no tamanho nativo (o M5GFX recorta o que passar da tela).
- Depois da imagem, redesenhamos a `statusBar` e o nome do arquivo por cima.
- As setas (`; . , /`) trocam de imagem (com *wrap* circular); crase sai.

## Acoes destrutivas e a regra do vermelho

No Noir, **vermelho (`BLOOD`) = perigo**. As acoes destrutivas deste modulo
(apagar, sobrescrever) sao protegidas assim:

- Apagar aparece por **ultimo** no menu de acoes do arquivo e e' marcado como
  perigo via `ui::listView(..., dangerFrom=2)` (pinta em vermelho).
- Antes de apagar, `ui::confirm(..., danger=true)` exige confirmacao.
- Feedback de sucesso usa `toast()` (um `ui::banner` neutro) — o vermelho fica
  reservado a **erros** (`ui::redStripe`).

Como o perigo e' tratado item-a-item (com `confirm` interno), os quatro apps
ficam com `danger=false` na barra do menu principal. Nao ha' transmissao (TX)
neste modulo, entao `noir::setTxActive()` nao e' usado.

## Contrato de exportacao

```cpp
// arquivos.h
extern const noir::AppEntry ARQUIVOS_APPS[];
extern const int            ARQUIVOS_APPS_COUNT;
```

As funcoes `run()` ficam em **namespace anonimo** (arquivo-local) e o array
publico e' definido no `.cpp`. O `app_registry` (mantido pela integracao)
inclui `apps/arquivos/arquivos.h` e copia as entradas para a categoria.

## Armadilhas resumidas

1. **Sem PSRAM**: limite de 8 KB no editor; arquivos maiores viram somente-leitura.
2. **`File::name()` = caminho completo** no ESP32 — sempre use `baseName()`.
3. **`FILE_WRITE` trunca** o arquivo (`"w"`), nao anexa. Para anexar seria `FILE_APPEND`.
4. **Sempre trate a ausencia de cartao** com `ensureSD()` + `messageBox`.
5. **Feche os `File`** (entradas de `openNextFile` e o proprio diretorio) para
   nao vazar descritores.
6. A crase `` ` `` e' reservada como "voltar/sair" e nao pode ser digitada no editor.
