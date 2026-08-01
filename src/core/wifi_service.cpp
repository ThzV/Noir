// Noir OS  -  Servico de WiFi
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/wifi_service.h"
#include "core/config.h"
#include "ui/widgets.h"
#include <WiFi.h>

namespace noir {
namespace wifi {

int scan(std::vector<Network>& out) {
    out.clear();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);   // nao apaga credenciais salvas
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        Network net;
        net.ssid    = WiFi.SSID(i);
        net.rssi    = WiFi.RSSI(i);
        net.channel = WiFi.channel(i);
        net.open    = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        net.bssid   = WiFi.BSSIDstr(i);
        out.push_back(net);
    }
    WiFi.scanDelete();
    return (n < 0) ? 0 : n;
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }
int    rssi()      { return WiFi.RSSI(); }
String ip()        { return WiFi.localIP().toString(); }
String ssid()      { return WiFi.SSID(); }
void   disconnect(){ WiFi.disconnect(true, false); }

bool connectSaved(bool showUi, uint32_t timeoutMs) {
    String ssid = noir::config::getStr("wifi_ssid", "");
    String pass = noir::config::getStr("wifi_pass", "");
    if (ssid.length() == 0) {
        if (showUi) ui::redStripe("Sem WiFi salvo (veja Config)");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        if (showUi) {
            int pct = (int)((millis() - start) * 100 / timeoutMs);
            ui::progress("WiFi", String("Conectando a ") + ssid, pct);
        }
        delay(200);
    }

    bool ok = (WiFi.status() == WL_CONNECTED);
    if (!ok && showUi) ui::redStripe("Falha ao conectar");
    return ok;
}

bool ensure() {
    if (isConnected()) return true;
    return connectSaved(true);
}

} // namespace wifi
} // namespace noir
