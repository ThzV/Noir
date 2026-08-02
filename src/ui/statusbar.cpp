// Noir OS  -  Barra de status (estilo Spider-Noir: aranha + wifi/bateria)
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/statusbar.h"
#include "ui/theme.h"
#include "core/app.h"
#include "core/time_service.h"
#include <WiFi.h>

namespace ui {

void statusBar(const char* title) {
    M5Canvas& d = gfx();
    const int cy = noir::STATUSBAR_H / 2;
    const bool tx = noir::txActive();

    d.fillRect(0, 0, noir::SCREEN_W, noir::STATUSBAR_H, noir::INK);
    d.drawFastHLine(0, noir::STATUSBAR_H, noir::SCREEN_W, tx ? noir::RED : noir::RED);

    // --- Esquerda: emblema da aranha (vermelho) + titulo da tela ---
    drawSpider(7, cy, 4, noir::RED);
    d.setFont(&fonts::Font0);
    d.setTextDatum(middle_left);
    d.setTextColor(noir::BONE, noir::INK);
    d.drawString(title, 15, cy);

    // --- Direita: relogio, bateria, wifi (da borda para dentro) ---
    d.setTextDatum(middle_right);
    d.setTextColor(noir::BONE, noir::INK);
    d.drawString(noir::timeservice::hhmm().c_str(), noir::SCREEN_W - 2, cy);

    int batt = M5.Power.getBatteryLevel();
    uint16_t battCor = (batt >= 0 && batt < 15) ? noir::RED : noir::BONE;
    drawBattery(noir::SCREEN_W - 44, cy - 4, batt < 0 ? 0 : batt, battCor);

    bool wifi = (WiFi.status() == WL_CONNECTED);
    int level = 0;
    if (wifi) {
        int r = WiFi.RSSI();
        level = (r >= -60) ? 3 : (r >= -72) ? 2 : 1;
    }
    drawWifiBars(noir::SCREEN_W - 58, cy + 4, level, noir::BONE, noir::ASH);

    // --- Alerta de transmissao ativa (pisca em vermelho vivo) ---
    if (tx) {
        d.setTextDatum(middle_right);
        d.setTextColor(noir::RED, noir::INK);
        d.drawString("TX", noir::SCREEN_W - 66, cy);
    }
}

} // namespace ui
