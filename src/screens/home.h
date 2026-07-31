// Noir OS  -  Tela Home
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace screens {

// Desenha a home (barra de status + relogio + dica).
void drawHome();

// Loop principal da home (bloqueante). ENTER abre o menu principal.
void homeLoop();

} // namespace screens
