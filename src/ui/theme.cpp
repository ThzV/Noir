// Noir OS  -  Toolkit de desenho (tema Noir)
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/theme.h"

namespace {
// Alocado em init() (depois de M5Cardputer.begin()) para evitar problemas de
// ordem de inicializacao estatica ao referenciar M5Cardputer.Display.
M5Canvas* s_canvas = nullptr;
bool      s_ready  = false;
}

namespace ui {

bool init() {
    if (!s_canvas) s_canvas = new M5Canvas(&M5Cardputer.Display);
    s_canvas->setColorDepth(16);
    s_ready = (s_canvas->createSprite(noir::SCREEN_W, noir::SCREEN_H) != nullptr);
    s_canvas->setTextWrap(false);
    return s_ready;
}

M5Canvas& gfx() { return *s_canvas; }

void present() {
    if (s_ready) s_canvas->pushSprite(0, 0);
}

void drawGrain(int density) {
    for (int i = 0; i < density; ++i) {
        int x = (int)random(noir::SCREEN_W);
        int y = (int)random(noir::SCREEN_H);
        s_canvas->drawPixel(x, y, (random(2) == 0) ? noir::ASH : noir::INK);
    }
}

void drawVignette() {
    s_canvas->drawRect(0, 0, noir::SCREEN_W, noir::SCREEN_H, noir::INK);
    s_canvas->drawRect(1, 1, noir::SCREEN_W - 2, noir::SCREEN_H - 2, noir::BLACK);
    // "cantos de filme"
    s_canvas->drawFastHLine(0, 0, 12, noir::STEEL);
    s_canvas->drawFastVLine(0, 0, 12, noir::STEEL);
    s_canvas->drawFastHLine(noir::SCREEN_W - 12, noir::SCREEN_H - 1, 12, noir::STEEL);
    s_canvas->drawFastVLine(noir::SCREEN_W - 1, noir::SCREEN_H - 12, 12, noir::STEEL);
}

void clearNoir() {
    s_canvas->setTextSize(1);
    s_canvas->fillSprite(noir::BLACK);
    drawGrain(110);
    drawVignette();
}

void panel(int x, int y, int w, int h, const char* title) {
    s_canvas->fillRect(x, y, w, h, noir::INK);
    s_canvas->drawRect(x, y, w, h, noir::BONE);
    s_canvas->drawRect(x + 1, y + 1, w - 2, h - 2, noir::ASH);
    if (title && *title) {
        s_canvas->setFont(&fonts::Font2);
        s_canvas->setTextDatum(top_left);
        s_canvas->setTextColor(noir::WHITE, noir::INK);
        s_canvas->drawString(title, x + 6, y + 4);
    }
}

void drawSpider(int cx, int cy, int r, uint16_t color) {
    // Corpo: abdomen (maior, embaixo) + cabeca (menor, em cima).
    s_canvas->fillCircle(cx, cy + r / 4, r / 2, color);
    s_canvas->fillCircle(cx, cy - r / 3, r / 3, color);
    // 4 pernas de cada lado, com uma "dobra" (joelho).
    for (int i = 0; i < 4; ++i) {
        int ky   = cy - r / 3 + i * (2 * r / 3) / 3;
        int knee = r * 2 / 3;
        s_canvas->drawLine(cx - r / 3, ky, cx - knee, ky - r / 3, color);
        s_canvas->drawLine(cx - knee, ky - r / 3, cx - knee - r / 3, ky + r / 4, color);
        s_canvas->drawLine(cx + r / 3, ky, cx + knee, ky - r / 3, color);
        s_canvas->drawLine(cx + knee, ky - r / 3, cx + knee + r / 3, ky + r / 4, color);
    }
}

void drawWifiBars(int x, int yBase, int level, uint16_t on, uint16_t off) {
    for (int i = 0; i < 3; ++i) {
        int h  = 3 + i * 3;
        int bx = x + i * 4;
        s_canvas->fillRect(bx, yBase - h, 2, h, (i < level) ? on : off);
    }
}

void drawBattery(int x, int y, int pct, uint16_t color) {
    const int w = 16, h = 8;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    s_canvas->drawRect(x, y, w, h, color);
    s_canvas->fillRect(x + w, y + 2, 2, h - 4, color);        // "pino" +
    s_canvas->fillRect(x + 1, y + 1, (w - 2) * pct / 100, h - 2, color);
}

} // namespace ui
