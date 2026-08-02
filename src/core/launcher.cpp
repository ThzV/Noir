// Noir OS  -  Launcher
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/launcher.h"
#include "core/app_registry.h"
#include "core/time_service.h"
#include "ui/theme.h"
#include "ui/statusbar.h"
#include "ui/widgets.h"
#include "ui/input.h"
#include <vector>

namespace noir {

void fallbackDashboard() {
    M5Canvas& d = ui::gfx();
    uint32_t last = 0;
    auto draw = [&]() {
        ui::clearNoir();
        ui::statusBar("NOIR");
        d.setFont(&fonts::Font7);
        d.setTextDatum(middle_center);
        d.setTextColor(noir::WHITE, noir::BLACK);
        d.drawString(timeservice::hhmm().c_str(), noir::SCREEN_W / 2, 60);
        d.setFont(&fonts::Font0);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString(timeservice::have() ? timeservice::dateStr().c_str() : "sem sync NTP",
                     noir::SCREEN_W / 2, 92);
        d.setTextColor(noir::BONE, noir::BLACK);
        d.drawString("ENTER  menu", noir::SCREEN_W / 2, 116);
        ui::present();
    };
    draw();
    last = millis();
    for (;;) {
        ui::KeyEvent e = ui::readKey();
        if (e.key == ui::Key::Enter) return;
        if (millis() - last >= 1000) { draw(); last = millis(); }
        delay(10);
    }
}

// Icone simples por categoria (desenhado com primitivas). 'col' = cor do traco.
static void drawCatIcon(M5Canvas& d, const String& n, int cx, int cy, uint16_t col) {
    if (n == "Rede") {
        ui::drawWifiBars(cx - 6, cy + 6, 3, col, col);
    } else if (n == "Servidor") {
        for (int i = 0; i < 3; ++i) {
            int yy = cy - 8 + i * 6;
            d.drawRect(cx - 10, yy, 20, 4, col);
            d.fillCircle(cx - 6, yy + 1, 1, col);
        }
    } else if (n == "Seguranca") {
        d.drawRect(cx - 7, cy, 14, 10, col);          // corpo do cadeado
        d.drawLine(cx - 4, cy, cx - 4, cy - 5, col);  // alca
        d.drawLine(cx + 4, cy, cx + 4, cy - 5, col);
        d.drawLine(cx - 4, cy - 5, cx + 4, cy - 5, col);
        d.fillCircle(cx, cy + 5, 1, col);
    } else if (n == "Arquivos") {
        d.fillRect(cx - 10, cy - 8, 8, 3, col);       // aba da pasta
        d.drawRect(cx - 10, cy - 5, 20, 12, col);
    } else if (n == "Produtividade") {
        d.drawCircle(cx, cy, 8, col);
        d.drawLine(cx, cy, cx, cy - 5, col);
        d.drawLine(cx, cy, cx + 4, cy + 1, col);
    } else if (n == "Config") {
        d.drawCircle(cx, cy, 6, col);
        d.fillRect(cx - 1, cy - 10, 2, 4, col);
        d.fillRect(cx - 1, cy + 6, 2, 4, col);
        d.fillRect(cx - 10, cy - 1, 4, 2, col);
        d.fillRect(cx + 6, cy - 1, 4, 2, col);
    } else if (n == "Ferramentas") {
        d.drawLine(cx - 8, cy + 8, cx + 5, cy - 5, col);   // chave inglesa
        d.drawCircle(cx + 6, cy - 6, 3, col);
        d.drawCircle(cx - 8, cy + 8, 2, col);
    } else {
        ui::drawSpider(cx, cy, 8, col);                    // fallback: aranha
    }
}

// Menu principal em GRADE (estilo cyberdeck): 3 colunas de icones+rotulo.
// Retorna o indice da categoria escolhida ou -1 (voltar ao dashboard).
static int gridMenu(const std::vector<CategoryRT>& reg) {
    M5Canvas& d = ui::gfx();
    const int n = (int)reg.size();
    if (n == 0) return -1;
    const int cols = 3;
    const int rows = (n + cols - 1) / cols;
    const int top  = noir::STATUSBAR_H + 3;
    const int gw   = noir::SCREEN_W / cols;
    const int gh   = (noir::SCREEN_H - top - 10) / (rows < 1 ? 1 : rows);
    int sel = 0;

    auto draw = [&]() {
        ui::clearNoir();
        ui::statusBar("NOIR");
        for (int i = 0; i < n; ++i) {
            int c = i % cols, r = i / cols;
            int x = c * gw, y = top + r * gh;
            int cxp = x + gw / 2, cyp = y + gh / 2 - 5;
            bool on = (i == sel);
            if (on) {
                d.drawRect(x + 3, y + 2, gw - 6, gh - 4, noir::RED);
                d.drawRect(x + 4, y + 3, gw - 8, gh - 6, noir::RED);
            }
            drawCatIcon(d, String(reg[i].name), cxp, cyp, on ? noir::RED : noir::BONE);
            d.setFont(&fonts::Font0);
            d.setTextDatum(bottom_center);
            d.setTextColor(on ? noir::WHITE : noir::STEEL, noir::BLACK);
            d.drawString(reg[i].name, cxp, y + gh - 3);
        }
        d.setFont(&fonts::Font0);
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString("setas mover   ENTER abrir   ` volta", noir::SCREEN_W / 2, noir::SCREEN_H - 1);
        ui::present();
    };
    draw();

    for (;;) {
        ui::KeyEvent e = ui::waitKey();
        int c = sel % cols, r = sel / cols;
        if      (e.key == ui::Key::Left)  { if (c > 0) sel--; }
        else if (e.key == ui::Key::Right) { if (c < cols - 1 && sel + 1 < n) sel++; }
        else if (e.key == ui::Key::Up)    { if (r > 0) sel -= cols; }
        else if (e.key == ui::Key::Down)  { if (sel + cols < n) sel += cols; }
        else if (e.key == ui::Key::Enter) return sel;
        else if (e.key == ui::Key::Back)  return -1;
        else continue;
        draw();
    }
}

static void runCategory(const CategoryRT& cat) {
    int sel = 0;
    for (;;) {
        std::vector<String> items;
        int dangerFrom = -1;
        for (size_t i = 0; i < cat.apps.size(); ++i) {
            items.push_back(String(cat.apps[i].name));
            if (cat.apps[i].danger && dangerFrom < 0) dangerFrom = (int)i;
        }
        int r = ui::listView(cat.name, items, sel, dangerFrom);
        if (r < 0) return;
        sel = r;

        const AppEntry& app = cat.apps[r];
        if (app.danger) {
            if (!ui::confirm(app.name,
                             "TX ativo / acao sensivel.\nVoce tem autorizacao?\n(docs/legal-etica.md)",
                             true)) {
                continue;
            }
        }
        if (app.run) app.run();
    }
}

void runLauncher() {
    AppRun dash = dashboardApp();
    for (;;) {
        if (dash) dash();
        else      fallbackDashboard();

        const auto& reg = registry();
        int c = gridMenu(reg);
        if (c < 0) continue;               // volta ao dashboard
        runCategory(reg[c]);
    }
}

} // namespace noir
