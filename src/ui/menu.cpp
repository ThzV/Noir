// Noir OS  -  Menu em lista
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/menu.h"
#include "ui/theme.h"
#include "ui/statusbar.h"

namespace ui {

static void drawMenu(const char* title, const MenuItem* items, int count, int sel) {
    M5Canvas& d = gfx();
    clearNoir();
    statusBar(title);

    const int top    = noir::STATUSBAR_H + 6;
    const int rowH   = 20;
    const int visible = (noir::SCREEN_H - top - 12) / rowH;

    int first = 0;
    if (visible > 0 && sel >= visible) first = sel - visible + 1;

    d.setFont(&fonts::Font2);
    for (int i = 0; i < visible && (first + i) < count; ++i) {
        int idx = first + i;
        int y   = top + i * rowH;
        bool on = (idx == sel);

        if (on) {
            d.fillRect(6, y, noir::SCREEN_W - 12, rowH - 2, noir::WHITE);
            d.setTextColor(noir::BLACK, noir::WHITE);
        } else {
            d.setTextColor(noir::BONE, noir::BLACK);
        }
        d.setTextDatum(middle_left);
        d.drawString(items[idx].label, 14, y + (rowH - 2) / 2);

        if (items[idx].hint && items[idx].hint[0]) {
            d.setTextColor(on ? noir::ASH : noir::STEEL, on ? noir::WHITE : noir::BLACK);
            d.setTextDatum(middle_right);
            d.drawString(items[idx].hint, noir::SCREEN_W - 12, y + (rowH - 2) / 2);
        }
    }

    // Rodape de ajuda.
    d.setFont(&fonts::Font0);
    d.setTextDatum(bottom_center);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString(";/. navegar   ENTER ok   ` voltar", noir::SCREEN_W / 2, noir::SCREEN_H - 2);

    present();
}

int menu(const char* title, const MenuItem* items, int count, int start) {
    if (count <= 0) return -1;
    int sel = (start >= 0 && start < count) ? start : 0;
    drawMenu(title, items, count, sel);

    for (;;) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto ks = M5Cardputer.Keyboard.keysState();

            if (ks.enter) return sel;

            bool changed = false;
            for (char c : ks.word) {
                if (c == ';') { sel = (sel - 1 + count) % count; changed = true; }
                else if (c == '.') { sel = (sel + 1) % count; changed = true; }
                else if (c == '`') { return -1; }
            }
            if (changed) drawMenu(title, items, count, sel);
        }
        delay(10);
    }
}

} // namespace ui
