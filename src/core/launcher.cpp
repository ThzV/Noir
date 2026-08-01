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
        std::vector<String> cats;
        cats.reserve(reg.size());
        for (const auto& c : reg) cats.push_back(String(c.name));

        int c = ui::listView("MENU", cats, 0);
        if (c < 0) continue;               // volta ao dashboard
        runCategory(reg[c]);
    }
}

} // namespace noir
