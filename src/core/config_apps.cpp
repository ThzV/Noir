// Noir OS  -  Categoria "Config"
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/config_apps.h"
#include "core/wifi_config.h"
#include "core/config.h"
#include "core/time_service.h"
#include "core/wifi_service.h"
#include "ui/widgets.h"
#include "ui/input.h"
#include "ui/theme.h"
#include <M5Cardputer.h>
#include <vector>

namespace {

void appWifi() { noir::setupWifiApp(); }

void appBrightness() {
    int b = noir::config::getInt("bright", 90);
    auto apply = [&]() {
        if (b < 10) b = 10;
        if (b > 100) b = 100;
        M5Cardputer.Display.setBrightness((uint8_t)(b * 255 / 100));
    };
    for (;;) {
        apply();
        ui::progress("Brilho", "< diminui  > aumenta  ENTER salva", b);
        ui::KeyEvent e = ui::waitKey();
        if (e.key == ui::Key::Left)       b -= 10;
        else if (e.key == ui::Key::Right) b += 10;
        else if (e.key == ui::Key::Enter) { noir::config::setInt("bright", b); return; }
        else if (e.key == ui::Key::Back)  return;
    }
}

void appTimezone() {
    std::vector<String> zones = {
        "Brasilia  (UTC-3)",
        "Noronha   (UTC-2)",
        "Acre      (UTC-5)",
        "UTC",
        "Personalizado (TZ POSIX)...",
    };
    std::vector<String> vals = {"<-03>3", "<-02>2", "<-05>5", "UTC0", ""};

    int r = ui::listView("Fuso horario", zones);
    if (r < 0) return;

    String tz;
    if (r == (int)zones.size() - 1) {
        bool ok = true;
        tz = ui::textInput("TZ POSIX", "<-03>3", false, &ok);
        if (!ok) return;
    } else {
        tz = vals[r];
    }

    noir::config::setStr("tz", tz);
    noir::timeservice::begin();
    if (noir::wifi::isConnected()) noir::timeservice::sync(6000);
    ui::messageBox("Fuso", String("Fuso salvo: ") + tz + "\nHora: " + noir::timeservice::hhmm());
}

void appAbout() {
    ui::banner("Sobre", "NOIR", "v0.1  -  AGPL-3.0");
}

} // namespace

namespace apps {
namespace config {

const noir::AppEntry CONFIG_APPS[] = {
    {"WiFi",         "rede", appWifi,       false},
    {"Brilho",       "tela", appBrightness, false},
    {"Fuso horario", "hora", appTimezone,   false},
    {"Sobre",        "info", appAbout,      false},
};
const int CONFIG_APPS_COUNT = sizeof(CONFIG_APPS) / sizeof(CONFIG_APPS[0]);

} // namespace config
} // namespace apps
