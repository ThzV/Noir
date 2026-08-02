// Noir OS  -  Abstracao de teclado
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/input.h"
#include <M5Cardputer.h>

namespace ui {

KeyEvent readKey() {
    KeyEvent e{Key::None, 0};
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        auto ks = M5Cardputer.Keyboard.keysState();

        if (ks.enter) { e.key = Key::Enter; return e; }
        if (ks.del)   { e.key = Key::Del;   return e; }

        for (char c : ks.word) {
            switch (c) {
                case ';': e.key = Key::Up;    e.ch = c; return e;
                case '.': e.key = Key::Down;  e.ch = c; return e;
                case ',': e.key = Key::Left;  e.ch = c; return e;
                case '/': e.key = Key::Right; e.ch = c; return e;
                case '`': e.key = Key::Back;  e.ch = c; return e;
                case ' ': e.key = Key::Space; e.ch = ' '; return e;
                default:  e.key = Key::Char;  e.ch = c; return e;
            }
        }
    }
    return e;
}

KeyEvent waitKey() {
    for (;;) {
        KeyEvent e = readKey();
        if (e.key != Key::None) return e;
        delay(8);
    }
}

} // namespace ui
