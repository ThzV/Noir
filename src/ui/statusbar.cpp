// Noir OS  -  Barra de status
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/statusbar.h"
#include "ui/theme.h"
#include <WiFi.h>
#include <cstdio>

namespace ui {

void statusBar(const char* title) {
    M5Canvas& d = gfx();

    d.fillRect(0, 0, noir::SCREEN_W, noir::STATUSBAR_H, noir::INK);
    d.drawFastHLine(0, noir::STATUSBAR_H, noir::SCREEN_W, noir::ASH);
    d.setFont(&fonts::Font0);

    // Bateria (esquerda) - vermelho quando fraca.
    int batt = M5.Power.getBatteryLevel();
    d.setTextDatum(middle_left);
    d.setTextColor((batt >= 0 && batt < 15) ? noir::BLOOD : noir::BONE, noir::INK);
    char buf[16];
    if (batt < 0) std::snprintf(buf, sizeof(buf), "BAT --");
    else          std::snprintf(buf, sizeof(buf), "BAT %d%%", batt);
    d.drawString(buf, 4, noir::STATUSBAR_H / 2);

    // Titulo (centro).
    d.setTextDatum(middle_center);
    d.setTextColor(noir::WHITE, noir::INK);
    d.drawString(title, noir::SCREEN_W / 2, noir::STATUSBAR_H / 2);

    // WiFi (direita).
    bool wifi = (WiFi.status() == WL_CONNECTED);
    d.setTextDatum(middle_right);
    d.setTextColor(wifi ? noir::BONE : noir::STEEL, noir::INK);
    d.drawString(wifi ? "WiFi" : "----", noir::SCREEN_W - 4, noir::STATUSBAR_H / 2);
}

} // namespace ui
