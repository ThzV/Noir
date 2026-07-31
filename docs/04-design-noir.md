# 04 — Design system Noir

A identidade **Spider-Man Noir** é o que transforma "mais um multitool de ESP32" no **Noir**. Aqui está o sistema de design: regras concretas, com valores prontos para código.

## Referência estética

Spider-Man Noir vive nos anos 1930, em **preto e branco de alto contraste**, iluminação dramática de *film noir*, granulado de filme, e um universo onde a cor quase não existe (na animação, o personagem literalmente não percebe cores). Traduzimos isso para uma tela de 240×135:

- **Monocromático** com sombras duras e recortes fortes.
- **Textura de filme** (grão) e **vinheta** nas bordas.
- **Tipografia art-déco** para títulos.
- **Um único acento de cor**, usado com significado.

## 1. Paleta

Base em tons de cinza + **um** acento. Valores em RGB565 (formato da M5GFX) e hex.

| Nome | Hex | Uso |
|---|---|---|
| `NOIR_BLACK` | `#000000` | Fundo padrão de tudo. |
| `NOIR_INK` | `#0D0D0D` | Fundo "quase preto" para camadas. |
| `NOIR_ASH` | `#3A3A3A` | Elementos desabilitados, linhas sutis. |
| `NOIR_STEEL` | `#7A7A7A` | Texto secundário, ícones inativos. |
| `NOIR_BONE` | `#D6D6D6` | Texto principal (branco levemente sujo, mais "filme"). |
| `NOIR_WHITE` | `#FFFFFF` | Destaque máximo, item selecionado. |
| `NOIR_BLOOD` (acento) | `#8B0000` | **Somente perigo / TX ativo** (deauth, beacon spam, gravação, alerta). |

### Regra do acento vermelho
O vermelho **nunca** é decorativo. Ele significa **atenção/perigo/transmissão ativa**:
- Piscando → uma ferramenta ofensiva está **transmitindo** (evil portal no ar, beacon spam rodando).
- Sólido → estado destrutivo/irreversível (apagar, sobrescrever).
- Ausente → você está em modo passivo/seguro.

Isso dá ao usuário um **sinal visual honesto** de quando o dispositivo está fazendo algo com peso legal (ver [legal-etica.md](legal-etica.md)).

## 2. Tipografia

- **Títulos / marca:** fonte **art-déco condensada** (ex.: derivada de "Limelight", "Poiret", "Bebas Neue"). Uso: splash, cabeçalhos de tela.
- **Corpo / listas:** fonte **sans limpa e compacta** (a `M5GFX` já traz fontes eficientes; comece com as embutidas e evolua).
- **Mono (opcional):** para logs, hex, endereços MAC/IP — uma fonte monoespaçada melhora leitura.

### Como usar fontes custom na M5GFX
1. Converta uma TTF em fonte GFX/lgfx com uma ferramenta (ver recursos).
2. Inclua o `.h` gerado e faça `display.setFont(&SuaFonte)`.
3. Guarde variações (tamanhos) como sprites/fontes separadas — memória é curta.

> Comece com as fontes internas para não travar; troque por custom quando o resto estiver de pé.

## 3. Textura e atmosfera

O que "vende" o Noir na tela:

- **Grão de filme:** pré-renderize 2–3 sprites pequenos de ruído (ex.: 64×64) e faça *tiling* com baixa opacidade por cima da tela, alternando por frame → sensação de filme velho.
- **Vinheta:** escureça as bordas (gradiente radial) para focar o centro. Pode ser um sprite fixo desenhado por cima.
- **Dithering 1-bit:** imagens e fotos convertidas para preto-e-branco com **Floyd–Steinberg** ganham o visual de quadrinho/jornal antigo. Ótimo para o visualizador de imagens e ícones.
- **Halftone:** títulos e o splash podem usar padrão de pontos (meio-tom) para o clima de impressão vintage.

## 4. Componentes de UI

### Barra de status (topo, ~14 px)
`[ 🔋 82% ]              NOIR              [ 📶  21:47 ]`
- Esquerda: bateria (% e ícone). Vermelho quando <15%.
- Centro: título da tela atual (ou "NOIR" na home).
- Direita: ícone WiFi (barras) + relógio.

### Lista / menu
- Itens em linhas; o **item selecionado é invertido** (fundo `NOIR_WHITE`, texto `NOIR_BLACK`) — recorte duro, bem noir.
- Seta/indicador `▚` ou um bloco sólido à esquerda do item ativo.
- Scroll simples com "câmera" seguindo o cursor.

### Modais / diálogos
- **Borda grossa** estilo painel de HQ (2–3 px, cantos retos).
- Fundo `NOIR_INK`, título em caixa alta condensada.
- Botões: `[ CONFIRMAR ]` invertido quando focado.

### Cabeçalho de módulo
- Faixa preta com título art-déco + uma linha fina `NOIR_STEEL` abaixo.

### Estados
- **Carregando:** spinner minimalista (ex.: teia girando) ou barra pontilhada.
- **Vazio:** ícone grande esmaecido + texto curto.
- **Erro/perigo:** faixa `NOIR_BLOOD` (espelha o `displayRedStripe()` do Bruce).

## 5. Iconografia
- Ícones **monocromáticos, de linha grossa**, alto contraste, 16×16 ou 24×24.
- Consistência: mesma espessura de traço, mesma "luz" (sombras para baixo-direita).
- Guarde como sprites em LittleFS ou arrays em código.

## 6. Movimento
- **Seco e rápido** (o Noir é elegante, não "fofo").
- Transição entre telas: efeito **"obturador"** (uma cortina preta abre/fecha) ou *ink wipe* (mancha de tinta) — curto (~150 ms).
- Nada de easing exagerado; cortes seguem o clima cinematográfico.

## 7. Splash / marca
- Tela inicial: título **"NOIR"** em art-déco com halftone, talvez a silhueta de uma teia/aranha, vinheta forte.
- Som opcional: um "click" de obturador de câmera (via `M5.Speaker`).

## Tokens no código
No esqueleto, esses valores vivem em `include/noir_theme.h` (constantes de cor RGB565) e `src/ui/theme.*` (helpers de desenho: `drawStatusBar`, `drawVignette`, `applyGrain`, `panel`, `listItem`). Mantenha **toda cor e medida** ali — nada de números mágicos espalhados.

## Checklist de "está Noir o suficiente?"
- [ ] Fundo preto, texto osso/branco, contraste alto.
- [ ] Vermelho aparece **só** em perigo/TX.
- [ ] Título em fonte condensada art-déco.
- [ ] Grão + vinheta perceptíveis, mas sem cansar.
- [ ] Item selecionado invertido (branco sólido).
- [ ] Transições secas e curtas.

## Recursos de aprendizado
- 🎨 **M5GFX — API de desenho:** <https://docs.m5stack.com/en/arduino/m5gfx/m5gfx>
- 🎨 **M5GFX na prática (tutorial):** <https://m5stack.lang-ship.com/howto/m5gfx/basic/>
- 🎨 **Conversor de fontes TTF→GFX:** <https://rop.nl/truetype2gfx/> e <https://github.com/gmarty/xbm> (bitmaps)
- 🎨 **Floyd–Steinberg dithering (explicação):** <https://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering>
- 🎨 **`displayRedStripe`/status bar do Bruce (referência de padrão):** `src/core/display.cpp` no repo do Bruce.
- 🎨 **Paletas e teoria de film noir (inspiração):** buscar "film noir lighting chiaroscuro".

## Próximo passo
➡️ Entenda a licença e o que dá pra reusar: **[05 — Reaproveitando o Bruce](05-reaproveitar-bruce.md)**.
