# Módulo 📁 Arquivos

Utilitários de arquivos sobre o cartão **microSD**. Também offline. Reforça o padrão de UI (listas, edição de texto, rolagem).

## Objetivo
- **Explorador de arquivos** (navegar SD, criar/renomear/apagar/copiar).
- **Editor de texto** (abrir/editar/salvar `.txt`, `.md`, `.js`...).
- **Notas rápidas** (nota única de acesso imediato).
- **Visualizador de imagens** (JPG/PNG/BMP).

## APIs / bibliotecas
| Função | O que usar |
|---|---|
| SD | `SD.begin(CS=12)` + API `File`/`SD` (Arduino) |
| FS interno | `LittleFS` (para assets do firmware) |
| Imagens | M5GFX nativo: `drawJpgFile`, `drawPngFile`, `drawBmpFile` |
| Teclado | `M5Cardputer.Keyboard` (o Cardputer tem teclado físico — ótimo p/ editor) |

## Detalhe por ferramenta

### Explorador de arquivos
- Navegar diretórios (`File dir = SD.open(path); dir.openNextFile()`).
- Ações: abrir (dispara editor/visualizador conforme extensão), renomear, apagar (com confirmação — vermelho!), criar pasta, copiar/mover.
- **Bruce:** `src/core/sd_functions.cpp` tem utilitários de SD (listar, copiar, apagar) — boa referência.
- **Noir:** ícones por tipo (pasta, txt, img, bin), tamanho do arquivo, item selecionado invertido.
- **Dificuldade:** 🟡 Média.

### Editor de texto
- Carregar arquivo em buffer (cuidado com **tamanho** — sem PSRAM! Edite arquivos pequenos ou por "janela").
- Cursor, inserir/apagar, rolar; salvar de volta no SD.
- Aproveite o teclado físico do Cardputer (Enter, Del, setas, símbolos).
- **Dificuldade:** 🟡 Média-alta (gerência de buffer + cursor + rolagem).

### Notas rápidas
- Uma nota "sempre ali": abre direto num buffer salvo em NVS ou num arquivo fixo do SD.
- Autosave ao sair.
- **Dificuldade:** 🟢 Baixa (é um editor simplificado de um arquivo só).

### Visualizador de imagens
- `M5GFX` decodifica e desenha JPG/PNG/BMP direto do SD.
- **Sem PSRAM:** imagens grandes podem não caber — a M5GFX desenha em streaming, mas evite resoluções enormes; ofereça "ajustar à tela".
- **Toque Noir:** modo **1-bit dithering** (Floyd–Steinberg) para transformar qualquer foto num visual de quadrinho preto-e-branco.
- **Dificuldade:** 🟢 Baixa (desenhar) / 🟡 (dithering estilizado).

## Cuidados
- **Memória:** editor e visualizador são os que mais arriscam estourar a heap. Trabalhe por blocos/linhas.
- **Cartão ausente/corrompido:** trate erros de `SD.begin()` com uma tela clara.
- **Nomes/paths:** cuidado com caracteres e tamanho de path do FAT.

## Ordem recomendada
1. **Explorador** (🟡) — base de navegação.
2. **Visualizador de imagens** (🟢) — recompensa visual + testa a tela.
3. **Notas rápidas** (🟢) — mini-editor.
4. **Editor de texto** (🟡) — evolução das notas.

## Recursos
- **SD (Arduino-ESP32):** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/sdmmc.html>
- **LittleFS:** <https://github.com/lorol/LITTLEFS>
- **M5GFX draw image (exemplos):** <https://github.com/m5stack/M5GFX/tree/master/examples>
- **Bruce sd_functions (referência):** `src/core/sd_functions.cpp` no repo do Bruce.
- **Floyd–Steinberg dithering:** <https://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering>
