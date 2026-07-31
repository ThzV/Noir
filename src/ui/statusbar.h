// Noir OS  -  Barra de status (bateria, titulo, WiFi)
// Inspirada no drawStatusBar() do Bruce. Detalhes: docs/04-design-noir.md
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace ui {

// Desenha a barra superior no canvas compartilhado.
void statusBar(const char* title);

} // namespace ui
