// ============================================================================
//  Noir OS  -  firmware para o M5Stack Cardputer v1.1
//
//  Copyright (C) 2026  Noir OS contributors
//
//  This program is free software: you can redistribute it and/or modify it
//  under the terms of the GNU Affero General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or (at your
//  option) any later version. Distributed WITHOUT ANY WARRANTY.
//  SPDX-License-Identifier: AGPL-3.0-or-later
//
//  Fluxo: setup() inicializa hardware + servicos, mostra a splash e tenta
//  WiFi/NTP; loop() entrega o controle ao launcher (nucleo do OS).
// ============================================================================
#include <M5Cardputer.h>
#include "core/config.h"
#include "core/time_service.h"
#include "core/wifi_service.h"
#include "core/launcher.h"
#include "ui/theme.h"

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

    d.fillRect(noir::SCREEN_W / 2 - 22, 96, 44, 3, noir::BLOOD);   // acento

    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString("cybertools . spider-noir", noir::SCREEN_W / 2, 116);
    ui::present();
    delay(1600);
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);   // true = habilita o teclado
    M5Cardputer.Display.setRotation(1);

    noir::config::begin();
    int b = noir::config::getInt("bright", 90);
    if (b < 10) b = 10;
    if (b > 100) b = 100;
    M5Cardputer.Display.setBrightness((uint8_t)(b * 255 / 100));

    randomSeed(micros());
    ui::init();
    splash();

    // Tenta reconectar no WiFi salvo e acertar o relogio (nao bloqueia o boot
    // por muito tempo; se falhar, o usuario configura em Config > WiFi).
    noir::timeservice::begin();
    if (noir::wifi::connectSaved(true, 8000)) {
        noir::timeservice::sync(6000);
    }
}

void loop() {
    noir::runLauncher();   // nunca retorna
}
