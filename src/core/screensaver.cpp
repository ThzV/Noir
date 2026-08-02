// Noir OS  -  Screensaver + repouso
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/screensaver.h"
#include "core/config.h"
#include "core/time_service.h"
#include "ui/theme.h"
#include "ui/input.h"
#include <M5Cardputer.h>
#include "esp_sleep.h"

namespace noir {
namespace screensaver {

int idleSeconds() { return noir::config::getInt("ss_idle", 30); }

// Brilho salvo (%) -> valor 0..255 para setBrightness.
static uint8_t savedBrightness() {
    int b = noir::config::getInt("bright", 90);
    if (b < 10)  b = 10;
    if (b > 100) b = 100;
    return (uint8_t)(b * 255 / 100);
}

void run() {
    M5Canvas& d = ui::gfx();
    // Brilho baixo durante o screensaver (o backlight domina o consumo).
    M5Cardputer.Display.setBrightness((uint8_t)(15 * 255 / 100));

    const int      sleepAfter = noir::config::getInt("ss_sleep", 60);  // s -> repouso
    const uint32_t start      = millis();

    for (;;) {
        // --- Cena Noir: aranha vai-e-volta + relogio + grao de filme --------
        ui::clearNoir();
        const int span = noir::SCREEN_W - 80;
        int t  = (int)((millis() / 90) % (2 * span));
        int sx = 40 + (t <= span ? t : (2 * span - t));   // bounce horizontal
        ui::drawSpider(sx, 44, 13, noir::RED);

        d.setFont(&fonts::Font7);
        d.setTextDatum(middle_center);
        d.setTextColor(noir::BONE, noir::BLACK);
        d.drawString(noir::timeservice::hhmm().c_str(), noir::SCREEN_W / 2, 94);

        d.setFont(&fonts::Font0);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString("with great power", noir::SCREEN_W / 2, 118);

        ui::drawGrain(50);
        ui::present();

        // ~150ms respondendo a teclas (qualquer tecla acorda).
        for (int i = 0; i < 3; ++i) {
            if (ui::readKey().key != ui::Key::None) {
                M5Cardputer.Display.setBrightness(savedBrightness());
                return;
            }
            delay(50);
        }

        // --- Ocio prolongado -> REPOUSO: tela apagada + light sleep ---------
        //  Light sleep e' seguro (nao reseta, sem risco de download mode) e
        //  acorda em ~200ms. Qualquer tecla encerra e restaura o brilho.
        if (sleepAfter > 0 && (millis() - start) / 1000 >= (uint32_t)sleepAfter) {
            M5Cardputer.Display.setBrightness(0);
            for (;;) {
                esp_sleep_enable_timer_wakeup(200000ULL);   // acorda a cada 200ms p/ checar teclado
                esp_light_sleep_start();
                M5Cardputer.update();
                if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                    M5Cardputer.Display.setBrightness(savedBrightness());
                    return;
                }
            }
        }
    }
}

} // namespace screensaver
} // namespace noir
