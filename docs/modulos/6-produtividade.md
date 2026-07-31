# Módulo ⏱️ Produtividade

Utilitários leves do dia a dia. **Ótimos primeiros projetos** — pouca dependência externa, muito retorno visual, e perfeitos para treinar o toolkit de UI e o tema Noir.

## Objetivo
- **Calendário** (visualizar mês, navegar).
- **Cronômetro** (start/stop/lap).
- **Pomodoro** (ciclos foco/pausa).
- **Conversor de unidades** (comprimento, massa, temperatura, dados, etc.).

## APIs / bibliotecas
| Função | O que usar |
|---|---|
| Tempo | `getLocalTime()` (NTP) para calendário/relógio; `millis()` para cronômetro/pomodoro |
| Som | `M5.Speaker.tone()` para alarmes do pomodoro |
| Persistência | `Preferences` (NVS) para guardar config do pomodoro, histórico |
| UI | toolkit Noir (`ui/theme`, `ui/menu`) |

## Detalhe por ferramenta

### Calendário
- Renderizar a grade do mês (7 colunas). Calcular dia da semana do dia 1 (algoritmo de Zeller ou via `struct tm`).
- Navegar mês anterior/próximo; destacar o dia de hoje (invertido).
- Depende de data correta → **NTP** (ver [home](1-home.md)).
- **Futuro:** eventos salvos no SD/NVS.
- **Dificuldade:** 🟢 Baixa-média (a matemática de calendário é o único truque).

### Cronômetro
- Base em `millis()`. Start/stop/reset + **laps** (lista rolável).
- Exibição grande `MM:SS.cc`; laps abaixo.
- **Dificuldade:** 🟢 Baixa. **Excelente primeiro módulo de verdade.**

### Pomodoro
- Ciclos configuráveis (padrão 25 min foco / 5 pausa / 15 pausa longa a cada 4).
- Barra de progresso circular ou linear; **beep** (`M5.Speaker`) e piscar a tela na transição.
- Contar ciclos completos; salvar no NVS.
- **Toque Noir:** tela vira quase toda preta no "foco"; o acento aparece sutil na pausa.
- **Dificuldade:** 🟢 Baixa-média.

### Conversor de unidades
- Categorias: comprimento, massa, temperatura, volume, dados (KB/MB/GB), velocidade, tempo.
- Entrada numérica pelo teclado; escolher unidade origem/destino; converter em tempo real.
- Estrutura: tabela de fatores por categoria (temperatura é caso especial — fórmula, não fator).
- **Dificuldade:** 🟢 Baixa (só cuidado com temperatura e precisão de float).

## Por que começar por aqui
Estes quatro **não dependem de rede nem de hardware especial** (só tela, teclado, `millis()` e som). São ideais para:
- Validar o **toolkit de UI** e o **tema Noir** no mundo real.
- Praticar o padrão `Screen` (enter/tick/exit) e o menu.
- Ter vitórias rápidas antes dos módulos difíceis (rede/cofre).

## Ordem recomendada
1. **Cronômetro** (🟢) — `millis()` + UI grande.
2. **Conversor** (🟢) — entrada de teclado + lógica pura.
3. **Pomodoro** (🟢) — timers + som + estado.
4. **Calendário** (🟡) — precisa de data/NTP.

## Recursos
- **millis()/timing:** <https://randomnerdtutorials.com/esp32-pir-motion-sensor-interrupts-timers/>
- **M5.Speaker (tons):** <https://github.com/m5stack/M5Unified>
- **struct tm / time.h:** <https://cplusplus.com/reference/ctime/tm/>
- **Algoritmo de Zeller (dia da semana):** <https://en.wikipedia.org/wiki/Zeller%27s_congruence>
- **Preferences/NVS:** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html>
