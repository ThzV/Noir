// Noir OS  -  Widgets de UI reutilizaveis
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/widgets.h"
#include "ui/theme.h"
#include "ui/input.h"
#include "ui/statusbar.h"
#include <cstdio>

namespace ui {

void redStripe(const String& msg, uint32_t ms) {
    M5Canvas& d = gfx();
    const int h = 26;
    const int y = (noir::SCREEN_H - h) / 2;
    d.fillRect(0, y, noir::SCREEN_W, h, noir::BLOOD);
    d.drawFastHLine(0, y, noir::SCREEN_W, noir::WHITE);
    d.drawFastHLine(0, y + h - 1, noir::SCREEN_W, noir::WHITE);
    d.setFont(&fonts::Font2);
    d.setTextDatum(middle_center);
    d.setTextColor(noir::WHITE, noir::BLOOD);
    d.drawString(msg.c_str(), noir::SCREEN_W / 2, y + h / 2);
    present();
    delay(ms);
}

void messageBox(const char* title, const String& msg) {
    M5Canvas& d = gfx();
    clearNoir();
    statusBar(title);
    d.setFont(&fonts::Font2);
    d.setTextColor(noir::BONE, noir::BLACK);
    d.setTextWrap(true);
    d.setCursor(8, noir::STATUSBAR_H + 8);
    d.print(msg);
    d.setTextWrap(false);
    d.setFont(&fonts::Font0);
    d.setTextDatum(bottom_center);
    d.setTextColor(noir::STEEL, noir::BLACK);
    d.drawString("ENTER voltar", noir::SCREEN_W / 2, noir::SCREEN_H - 2);
    present();
    for (;;) {
        KeyEvent e = waitKey();
        if (e.key == Key::Enter || e.key == Key::Back) return;
    }
}

bool confirm(const char* title, const String& msg, bool danger) {
    M5Canvas& d = gfx();
    int sel = 0;  // 0 = NAO, 1 = SIM
    auto draw = [&]() {
        clearNoir();
        statusBar(title);
        d.setFont(&fonts::Font2);
        d.setTextColor(noir::BONE, noir::BLACK);
        d.setTextWrap(true);
        d.setCursor(8, noir::STATUSBAR_H + 8);
        d.print(msg);
        d.setTextWrap(false);

        const char* labels[2] = {"NAO", "SIM"};
        const int bw = 92, by = noir::SCREEN_H - 30, bh = 22;
        for (int i = 0; i < 2; i++) {
            int bx = (i == 0) ? 16 : noir::SCREEN_W - 16 - bw;
            bool on = (i == sel);
            uint16_t fill = on ? noir::RED : noir::INK;
            uint16_t fg   = on ? noir::WHITE : noir::BONE;
            d.fillRect(bx, by, bw, bh, fill);
            d.drawRect(bx, by, bw, bh, noir::ASH);
            d.setFont(&fonts::Font2);
            d.setTextDatum(middle_center);
            d.setTextColor(fg, fill);
            d.drawString(labels[i], bx + bw / 2, by + bh / 2);
        }
        present();
    };
    draw();
    for (;;) {
        KeyEvent e = waitKey();
        if (e.key == Key::Left || e.key == Key::Up)         { sel = 0; draw(); }
        else if (e.key == Key::Right || e.key == Key::Down) { sel = 1; draw(); }
        else if (e.key == Key::Enter)                       { return sel == 1; }
        else if (e.key == Key::Back)                        { return false; }
    }
}

String textInput(const char* title, const String& initial, bool mask, bool* okOut) {
    M5Canvas& d = gfx();
    String buf = initial;
    bool ok = true;

    auto draw = [&]() {
        clearNoir();
        statusBar(title);
        panel(10, 52, noir::SCREEN_W - 20, 34, nullptr);
        String shown;
        if (mask) { for (size_t i = 0; i < buf.length(); ++i) shown += '*'; }
        else        shown = buf;
        String disp = shown + "_";
        const int maxChars = 24;   // trunca pela esquerda p/ mostrar o fim
        if ((int)disp.length() > maxChars) disp = disp.substring(disp.length() - maxChars);
        d.setFont(&fonts::Font2);
        d.setTextDatum(middle_left);
        d.setTextColor(noir::WHITE, noir::INK);
        d.drawString(disp.c_str(), 16, 69);
        d.setFont(&fonts::Font0);
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString("ENTER ok   DEL apaga   ` cancela", noir::SCREEN_W / 2, noir::SCREEN_H - 2);
        present();
    };
    draw();

    for (;;) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto ks = M5Cardputer.Keyboard.keysState();
            if (ks.enter) { ok = true; break; }
            if (ks.del) {
                if (buf.length() > 0) { buf.remove(buf.length() - 1); draw(); continue; }
                ok = false; break;   // DEL com campo vazio = cancelar
            }
            bool changed = false, cancel = false;
            for (char c : ks.word) {
                if (c == '`') { cancel = true; break; }
                if (c == '\n' || c == '\r') continue;
                buf += c;
                changed = true;
            }
            if (cancel) { ok = false; break; }
            if (changed) draw();
        }
        delay(8);
    }
    if (okOut) *okOut = ok;
    return ok ? buf : initial;
}

int listView(const char* title, const std::vector<String>& items, int start, int dangerFrom) {
    M5Canvas& d = gfx();
    const int count = (int)items.size();
    if (count == 0) { messageBox(title, "Lista vazia."); return -1; }

    int sel = (start >= 0 && start < count) ? start : 0;
    const int top     = noir::STATUSBAR_H + 4;
    const int rowH    = 18;
    const int visible = (noir::SCREEN_H - top - 12) / rowH;

    auto draw = [&]() {
        clearNoir();
        statusBar(title);
        int first = 0;
        if (visible > 0 && sel >= visible) first = sel - visible + 1;

        d.setFont(&fonts::Font2);
        for (int i = 0; i < visible && (first + i) < count; i++) {
            int idx = first + i;
            int y   = top + i * rowH;
            bool on = (idx == sel);
            bool danger = (dangerFrom >= 0 && idx >= dangerFrom);

            if (on) {
                d.fillRect(6, y, noir::SCREEN_W - 12, rowH - 2, noir::RED);
                d.setTextColor(noir::WHITE, noir::RED);
            } else {
                d.setTextColor(danger ? noir::RED : noir::BONE, noir::BLACK);
            }
            d.setTextDatum(middle_left);
            d.drawString(items[idx].c_str(), 12, y + (rowH - 2) / 2);
        }

        if (count > visible && visible > 0) {   // scrollbar
            int trackH = visible * rowH;
            int barH = (visible * trackH) / count; if (barH < 6) barH = 6;
            int barY = top + (first * trackH) / count;
            d.fillRect(noir::SCREEN_W - 3, top, 2, trackH, noir::INK);
            d.fillRect(noir::SCREEN_W - 3, barY, 2, barH, noir::STEEL);
        }

        d.setFont(&fonts::Font0);
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString(";/. navegar   ENTER ok   ` voltar", noir::SCREEN_W / 2, noir::SCREEN_H - 2);
        present();
    };
    draw();

    for (;;) {
        KeyEvent e = waitKey();
        switch (e.key) {
            case Key::Up:    sel = (sel - 1 + count) % count; draw(); break;
            case Key::Down:  sel = (sel + 1) % count;         draw(); break;
            case Key::Enter: return sel;
            case Key::Back:  return -1;
            default: break;
        }
    }
}

void progress(const char* title, const String& label, int pct) {
    M5Canvas& d = gfx();
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    clearNoir();
    statusBar(title);
    d.setFont(&fonts::Font2);
    d.setTextDatum(middle_center);
    d.setTextColor(noir::BONE, noir::BLACK);
    d.drawString(label.c_str(), noir::SCREEN_W / 2, 54);

    const int bx = 20, by = 78, bw = noir::SCREEN_W - 40, bh = 16;
    d.drawRect(bx, by, bw, bh, noir::BONE);
    d.fillRect(bx + 2, by + 2, (bw - 4) * pct / 100, bh - 4, noir::RED);

    d.setFont(&fonts::Font0);
    d.setTextDatum(middle_center);
    d.setTextColor(noir::STEEL, noir::BLACK);
    char b[8];
    std::snprintf(b, sizeof(b), "%d%%", pct);
    d.drawString(b, noir::SCREEN_W / 2, by + bh + 10);
    present();
}

void banner(const char* title, const String& big, const String& sub, bool wait) {
    M5Canvas& d = gfx();
    clearNoir();
    statusBar(title);
    d.setFont(&fonts::Font4);
    d.setTextDatum(middle_center);
    d.setTextColor(noir::WHITE, noir::BLACK);
    d.drawString(big.c_str(), noir::SCREEN_W / 2, 58);
    if (sub.length()) {
        d.setFont(&fonts::Font2);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString(sub.c_str(), noir::SCREEN_W / 2, 92);
    }
    if (wait) {
        d.setFont(&fonts::Font0);
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString("ENTER voltar", noir::SCREEN_W / 2, noir::SCREEN_H - 2);
    }
    present();
    if (wait) {
        for (;;) {
            KeyEvent e = waitKey();
            if (e.key == Key::Enter || e.key == Key::Back) return;
        }
    }
}

} // namespace ui
