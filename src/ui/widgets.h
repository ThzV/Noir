// ============================================================================
//  Noir OS  -  Widgets de UI reutilizaveis
//
//  Blocos prontos para todos os apps: mensagens, confirmacao, entrada de
//  texto, listas rolaveis, progresso. Todos desenham no canvas compartilhado
//  (ui::gfx) com o tema Noir e usam ui::input para o teclado.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include <Arduino.h>
#include <vector>

namespace ui {

// Faixa vermelha transitoria (erro/perigo) sobre a tela atual.
void redStripe(const String& msg, uint32_t ms = 1200);

// Tela de mensagem (texto com quebra de linha). Espera ENTER/voltar.
void messageBox(const char* title, const String& msg);

// Confirmacao SIM/NAO. Retorna true se confirmado. danger=true pinta em vermelho.
bool confirm(const char* title, const String& msg, bool danger = false);

// Editor de uma linha. Retorna o texto digitado.
//  - mask=true esconde os caracteres (senhas).
//  - Se cancelado (tecla ` ou DEL com campo vazio) e okOut != nullptr, *okOut=false.
String textInput(const char* title, const String& initial = "",
                 bool mask = false, bool* okOut = nullptr);

// Lista rolavel. Retorna o indice escolhido ou -1 (voltar).
//  - dangerFrom >= 0: itens com indice >= dangerFrom sao "perigo" (vermelho).
int listView(const char* title, const std::vector<String>& items,
             int start = 0, int dangerFrom = -1);

// Barra de progresso (0..100). Desenha e da present() imediatamente.
void progress(const char* title, const String& label, int pct);

// Tela com um texto grande centralizado + subtitulo opcional.
// wait=true espera uma tecla antes de retornar.
void banner(const char* title, const String& big, const String& sub = "",
            bool wait = true);

} // namespace ui
