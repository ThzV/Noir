# 02 — Ambiente de desenvolvimento

Este documento faz duas coisas:
1. **Diagnóstico da sua máquina** — o que já está pronto e o que falta (análise real, feita neste projeto).
2. **Passo a passo do setup PlatformIO** — como sair do zero e dar o primeiro flash.

---

## Parte 1 — Diagnóstico de prontidão (sua máquina, Windows 11)

| Item | Estado | Observação |
|---|---|---|
| VS Code 1.130.0 | ✅ Instalado | Editor principal. |
| Extensão ESP-IDF 2.1.0 | ✅ Instalada | Fica; convive com o PlatformIO. |
| Git 2.54 | ✅ Instalado | Controle de versão OK. |
| Python 3.14 e 3.13 | ✅ Instalados | ⚠️ `python`→3.14 e `python3`→3.13 são interpretadores **diferentes**. O PlatformIO usa o próprio Python interno, então não te afeta. |
| Node 24 / npm 11 | ✅ Instalados | Não é necessário para o firmware, mas ok. |
| USB nativo do Cardputer | ✅ Reconhecido | Enumera como `USB\VID_303A&PID_1001` com o driver nativo do Windows (`usbser.sys`) — **nenhum driver CP210x/CH340 necessário**. Atualmente aparece como "phantom" só porque a placa está desconectada. |
| ESP-IDF v6.0.2 (framework) | ✅ Clonado em `C:\esp\v6.0.2\esp-idf` | **Não** será o caminho principal (vamos de PlatformIO). |
| Toolchain ESP-IDF (compiladores/venv) | ⚠️ **Incompleta** | Só o instalador EIM (`eim-gui`) está no disco; faltam compiladores/CMake/Ninja/venv. |
| **PlatformIO** | ❌ **Ausente** | **É o que falta instalar.** Ver Parte 2. |
| Core ESP32 do Arduino IDE | ❌ Ausente | Não é necessário no fluxo PlatformIO. |

### 🎯 A boa notícia
Você **não precisa** terminar a instalação travada do ESP-IDF (EIM). Ao adotar o **PlatformIO**, ele **baixa e gerencia a própria toolchain** (compilador xtensa, CMake, Ninja, esptool, framework Arduino) automaticamente no **primeiro build**. 

**Resumo:** só falta **1 passo** — instalar a extensão PlatformIO IDE e abrir o projeto. O resto é automático.

---

## Parte 2 — Setup PlatformIO (o único passo pendente)

### 2.1 Instalar a extensão
No VS Code:
1. Aba **Extensions** (`Ctrl+Shift+X`).
2. Busque **"PlatformIO IDE"** (publisher `platformio`).
3. **Install**. A primeira instalação baixa o core do PlatformIO (~alguns minutos) e pede para recarregar a janela.

> Alternativa por linha de comando (opcional): `pip install platformio` — mas a extensão é o caminho recomendado no VS Code.

### 2.2 Abrir o projeto
1. **File → Open Folder →** `C:\Users\mathe\Documents\Personal\Projetos\Noir`.
2. O PlatformIO detecta o `platformio.ini` e prepara o ambiente.
3. **No primeiro build**, ele baixa a plataforma **pioarduino** (fork do espressif32 que o Bruce usa) e a toolchain. Isso é normal e só acontece uma vez.

### 2.3 Compilar
- Ícone de "✓" (Build) na barra inferior do PlatformIO, **ou** terminal:
```bash
pio run -e m5stack-cardputer
```
- Sucesso = binário gerado em `.pio/build/m5stack-cardputer/firmware.bin`.

### 2.4 Gravar (flash) no Cardputer
1. Conecte o Cardputer via USB-C.
2. (Se necessário) coloque em modo download: segure o botão **G0** e dê reset — na maioria das vezes o ESP32-S3 entra sozinho.
3. Build + Upload:
```bash
pio run -e m5stack-cardputer -t upload
```
4. O PlatformIO acha a porta COM sozinho. Para fixar, adicione `upload_port = COM3` no `platformio.ini`.

### 2.5 Monitor serial
```bash
pio device monitor -b 115200
```
Ou o ícone de "tomada" (Monitor). Útil para ver logs de boot e depurar (`esp32_exception_decoder` já está ligado no `platformio.ini`).

---

## Fluxo de trabalho diário

| Ação | Atalho / comando |
|---|---|
| Compilar | Build (✓) ou `pio run` |
| Compilar + gravar | Upload (→) ou `pio run -t upload` |
| Monitor serial | `pio device monitor` |
| Limpar build | `pio run -t clean` |
| Só a env do Cardputer | acrescente `-e m5stack-cardputer` |

## Convivência com a extensão ESP-IDF
As duas extensões coexistem. Se a ESP-IDF tentar "assumir" o projeto, ignore — o `platformio.ini` é a fonte de verdade aqui. Não precisa desinstalar nada.

## Dicas Windows
- Se a porta COM não aparecer: troque o cabo (alguns são só carga), teste outra USB, confirme no **Gerenciador de Dispositivos** se surge um "Dispositivo Serial USB (COMx)".
- O driver é nativo — **não** instale CH340/CP210x (não se aplica ao S3 nativo).

## Recursos de aprendizado
- 📘 **PlatformIO — instalação no VS Code:** <https://docs.platformio.org/en/latest/integration/ide/vscode.html>
- 📘 **pioarduino (plataforma usada):** <https://github.com/pioarduino/platform-espressif32>
- 📘 **Quickstart Arduino Cardputer (oficial M5):** <https://docs.m5stack.com/en/arduino/m5cardputer/program>
- 📘 **Guia de flash do Bruce (referência):** <https://github.com/pr3y/Bruce/wiki>

## Próximo passo
➡️ Entenda como o firmware é organizado: **[03 — Arquitetura do OS](03-arquitetura.md)**.
