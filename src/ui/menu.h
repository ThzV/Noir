// Noir OS  -  Menu em lista (estilo loopOptions() do Bruce)
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace ui {

struct MenuItem {
    const char* label;
    const char* hint;   // texto curto a' direita (pode ser "")
};

// Loop de menu bloqueante. Navega com ';' (cima) e '.' (baixo),
// seleciona com ENTER, volta com '`'. Retorna o indice ou -1 (voltar).
int menu(const char* title, const MenuItem* items, int count, int start = 0);

} // namespace ui
