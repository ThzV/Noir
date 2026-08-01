// Noir OS  -  Barra de status (bateria, titulo, WiFi, relogio, alerta TX)
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/statusbar.h"
#include "ui/theme.h"
#include "core/app.h"
#include "core/time_service.h"
#include <WiFi.h>
#include <cstdio>

namespace ui {

void statusBar(const char* title) {
    M5Canvas& d = gfx();
    const int cy = noir::STATUSBAR_H / 2;
    const bool tx = noir::txActive();

    d.fillRect(0, 0, noir::SCREEN_W, noir::STATUSBAR_H, noir::INK);
    d.drawFastHLine(0, noir::STATUSBAR_H, noir::SCREEN_W, tx ? noir::BLOOD : noir::ASH);
    d.setFont(&fonts::Font0);

    // Bateria (esquerda).
    int batt = M5.Power.getBatteryLevel();
    d.setTextDatum(middle_left);
    d.setTextColor((batt >= 0 && batt < 15) ? noir::BLOOD : noir::BONE, noir::INK);
    char b[16];
    if (batt < 0) std::snprintf(b, sizeof(b), "BAT --");
    else          std::snprintf(b, sizeof(b), "BAT %d%%", batt);
    d.drawString(b, 4, cy);

    // Titulo (centro).
    d.setTextDatum(middle_center);
    d.setTextColor(noir::WHITE, noir::INK);
    d.drawString(title, noir::SCREEN_W / 2, cy);

    // Relogio (direita).
    d.setTextDatum(middle_right);
    d.setTextColor(noir::BONE, noir::INK);
    d.drawString(noir::timeservice::hhmm().c_str(), noir::SCREEN_W - 4, cy);

    // Indicador de WiFi (bolinha).
    bool wifi = (WiFi.status() == WL_CONNECTED);
    d.fillCircle(noir::SCREEN_W - 42, cy, 2, wifi ? noir::BONE : noir::ASH);

    // Alerta de transmissao ativa.
    if (tx) {
        d.setTextDatum(middle_right);
        d.setTextColor(noir::BLOOD, noir::INK);
        d.drawString("TX", noir::SCREEN_W - 52, cy);
    }
}

} // namespace ui
