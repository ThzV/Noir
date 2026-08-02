// Noir OS  -  App de configuracao de WiFi (referencia de padrao)
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/wifi_config.h"
#include "core/wifi_service.h"
#include "core/config.h"
#include "core/time_service.h"
#include "ui/widgets.h"
#include <algorithm>
#include <vector>

namespace noir {

void setupWifiApp() {
    // 1) Escanear (feedback de progresso enquanto o scan bloqueia).
    ui::progress("WiFi", "Escaneando redes...", 30);
    std::vector<wifi::Network> nets;
    int n = wifi::scan(nets);
    if (n == 0) {
        ui::messageBox("WiFi", "Nenhuma rede encontrada.\nTente novamente.");
        return;
    }

    // Ordena por sinal (mais forte primeiro).
    std::sort(nets.begin(), nets.end(),
              [](const wifi::Network& a, const wifi::Network& b) { return a.rssi > b.rssi; });

    // 2) Lista de redes para o usuario escolher.
    std::vector<String> items;
    items.reserve(nets.size());
    for (const auto& net : nets) {
        String lock = net.open ? "  " : " *";
        items.push_back(net.ssid + "  (" + String((int)net.rssi) + "dBm)" + lock);
    }
    int sel = ui::listView("Escolha a rede", items);
    if (sel < 0) return;

    String ssid = nets[sel].ssid;

    // 3) Senha (a menos que a rede seja aberta).
    String pass = "";
    if (!nets[sel].open) {
        bool ok = true;
        String title = "Senha: " + ssid;
        pass = ui::textInput(title.c_str(), "", true, &ok);
        if (!ok) return;   // cancelado
    }

    // 4) Salva e conecta.
    config::setStr("wifi_ssid", ssid);
    config::setStr("wifi_pass", pass);

    if (wifi::connectSaved(true)) {
        timeservice::sync(6000);   // ja' aproveita p/ acertar o relogio via NTP
        ui::messageBox("WiFi", String("Conectado a ") + ssid + "\nIP: " + wifi::ip());
    } else {
        ui::messageBox("WiFi", "Nao foi possivel conectar.\nVerifique a senha.");
    }
}

} // namespace noir
