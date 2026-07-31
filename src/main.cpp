// ============================================================================
//  Noir OS  -  firmware para o M5Stack Cardputer v1.1
//
//  Copyright (C) 2026  Noir OS contributors
//
//  This program is free software: you can redistribute it and/or modify it
//  under the terms of the GNU Affero General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or (at your
//  option) any later version. This program is distributed WITHOUT ANY WARRANTY.
//  See the GNU AGPL <https://www.gnu.org/licenses/> for more details.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
//
//  Baseado no ecossistema M5 (M5Cardputer/M5Unified/M5GFX, MIT) e projetado
//  para reaproveitar modulos do Bruce (AGPL-3.0). Ver docs/.
// ============================================================================
#include <M5Cardputer.h>
#include "ui/theme.h"
#include "screens/home.h"

// Splash de abertura com a marca NOIR.
static void splash() {
    M5Canvas& d = ui::gfx();
    ui::clearNoir();
    d.setTextDatum(middle_center);

    d.setFont(&fonts::Font0);
    d.setTextSize(1);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString("M5 CARDPUTER v1.1", noir::SCREEN_W / 2, 30);

    d.setTextColor(noir::WHITE, noir::BLACK);
    d.setTextSize(5);
    d.drawString("NOIR", noir::SCREEN_W / 2, 72);
    d.setTextSize(1);

    // Acento de sangue (unico ponto de cor).
    d.fillRect(noir::SCREEN_W / 2 - 22, 96, 44, 3, noir::BLOOD);

    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString("cybertools . spider-noir", noir::SCREEN_W / 2, 116);
    ui::present();

    delay(1600);
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);   // true = habilita o teclado
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(90);

    randomSeed(micros());   // semente para o grao de filme
    ui::init();             // cria o canvas compartilhado
    splash();
}

void loop() {
    // homeLoop() e' bloqueante e detem o controle principal do OS.
    screens::homeLoop();
}
