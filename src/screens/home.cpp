// Noir OS  -  Tela Home + menu principal
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "screens/home.h"
#include "ui/theme.h"
#include "ui/menu.h"
#include "ui/statusbar.h"
#include <cstdio>

namespace {

// Tela "em construcao" mostrada ao selecionar um modulo ainda nao implementado.
void placeholder(const char* name) {
    M5Canvas& d = ui::gfx();
    ui::clearNoir();
    ui::statusBar(name);

    ui::panel(24, 42, noir::SCREEN_W - 48, 58, nullptr);
    d.setFont(&fonts::Font2);
    d.setTextDatum(middle_center);
    d.setTextColor(noir::BONE, noir::INK);
    d.drawString("EM CONSTRUCAO", noir::SCREEN_W / 2, 62);
    d.setFont(&fonts::Font0);
    d.setTextColor(noir::STEEL, noir::INK);
    d.drawString("veja o ROADMAP.md", noir::SCREEN_W / 2, 82);

    d.setTextDatum(bottom_center);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString("ENTER ou ` para voltar", noir::SCREEN_W / 2, noir::SCREEN_H - 2);
    ui::present();

    for (;;) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto ks = M5Cardputer.Keyboard.keysState();
            if (ks.enter) return;
            for (char c : ks.word) if (c == '`') return;
        }
        delay(10);
    }
}

void openMainMenu() {
    static const ui::MenuItem items[] = {
        { "Rede",          "wifi/ble" },
        { "Servidor",      "docker"   },
        { "Seguranca",     "cofre"    },
        { "Arquivos",      "sd"       },
        { "Produtividade", "timer"    },
        { "Config",        "sys"      },
    };
    const int count = sizeof(items) / sizeof(items[0]);

    int sel = 0;
    for (;;) {
        int r = ui::menu("MENU", items, count, sel);
        if (r < 0) return;           // volta para a home
        sel = r;
        placeholder(items[r].label); // modulo ainda nao implementado
    }
}

} // namespace

namespace screens {

void drawHome() {
    M5Canvas& d = ui::gfx();
    ui::clearNoir();
    ui::statusBar("NOIR");

    // Relogio grande (placeholder = tempo desde o boot).
    // Sem RTC/NTP ainda; ver docs/modulos/1-home.md.
    unsigned long s = millis() / 1000UL;
    char clk[16];
    std::snprintf(clk, sizeof(clk), "%02lu:%02lu", (s / 60UL) % 100UL, s % 60UL);

    d.setFont(&fonts::Font7);
    d.setTextDatum(middle_center);
    d.setTextColor(noir::WHITE, noir::BLACK);
    d.drawString(clk, noir::SCREEN_W / 2, 64);

    d.setFont(&fonts::Font0);
    d.setTextDatum(middle_center);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString("sem sync NTP", noir::SCREEN_W / 2, 96);

    d.setTextColor(noir::BONE, noir::BLACK);
    d.drawString("ENTER  abrir menu", noir::SCREEN_W / 2, 116);

    ui::present();
}

void homeLoop() {
    drawHome();
    unsigned long lastDraw = millis();

    for (;;) {
        M5Cardputer.update();

        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto ks = M5Cardputer.Keyboard.keysState();
            if (ks.enter) {
                openMainMenu();
                drawHome();
                lastDraw = millis();
            }
        }

        // Atualiza o relogio (e regenera o grao = leve "flicker" de filme) a cada 1s.
        if (millis() - lastDraw >= 1000UL) {
            drawHome();
            lastDraw = millis();
        }
        delay(10);
    }
}

} // namespace screens
