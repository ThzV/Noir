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

    // Emblema da aranha (vermelho Spider-Noir) acima do titulo.
    ui::drawSpider(noir::SCREEN_W / 2, 32, 12, noir::RED);

    d.setFont(&fonts::Font0);
    d.setTextSize(4);
    d.setTextColor(noir::WHITE, noir::BLACK);
    d.drawString("NOIR", noir::SCREEN_W / 2, 70);
    d.setTextSize(1);

    d.fillRect(noir::SCREEN_W / 2 - 48, 88, 96, 2, noir::RED);   // linha de acento

    d.setTextColor(noir::RED, noir::BLACK);
    d.drawString("SPIDER NOIR OS", noir::SCREEN_W / 2, 100);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString("with great power...", noir::SCREEN_W / 2, 118);
    ui::present();
    delay(1600);
}

// Tarefa de diagnostico: imprime um "heartbeat" no serial (USB-CDC) a cada 3s
// com o uptime e o heap livre. Serve para validar que o firmware esta vivo e
// estavel (sem reset-loop nem vazamento de memoria) enquanto roda o launcher.
static void heartbeatTask(void*) {
    for (;;) {
        Serial.printf("[hb] uptime=%lus  heap_livre=%u bytes\n",
                      (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap());
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void setup() {
    Serial.begin(115200);

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);   // true = habilita o teclado
    M5Cardputer.Display.setRotation(1);

    noir::config::begin();
    int b = noir::config::getInt("bright", 90);
    if (b < 10) b = 10;
    if (b > 100) b = 100;
    M5Cardputer.Display.setBrightness((uint8_t)(b * 255 / 100));

    randomSeed(micros());
    bool spriteOk = ui::init();

    delay(200);
    Serial.println();
    Serial.println("========== NOIR OS ==========");
    Serial.printf("Boot OK. Canvas (sprite 240x135): %s\n", spriteOk ? "alocado" : "FALHOU!");
    Serial.printf("Heap livre pos-init: %u bytes\n", (unsigned)ESP.getFreeHeap());

    splash();

    // Tenta reconectar no WiFi salvo e acertar o relogio (nao bloqueia o boot
    // por muito tempo; se falhar, o usuario configura em Config > WiFi).
    noir::timeservice::begin();
    if (noir::wifi::connectSaved(true, 8000)) {
        noir::timeservice::sync(6000);
        Serial.printf("WiFi conectado. IP: %s\n", noir::wifi::ip().c_str());
    } else {
        Serial.println("WiFi: sem rede salva (configure em Config > WiFi).");
    }

    Serial.println("Entrando no launcher (dashboard). ENTER abre o menu.");
    xTaskCreate(heartbeatTask, "hb", 3072, nullptr, 1, nullptr);
}

void loop() {
    noir::runLauncher();   // nunca retorna
}
