// ============================================================================
//  Noir OS  -  Servico de WiFi
//
//  Conexao (com credenciais salvas na config), scan e status. Usado pela Home,
//  Servidor e todos os modulos de rede.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include <Arduino.h>
#include <vector>

namespace noir {
namespace wifi {

struct Network {
    String  ssid;
    int32_t rssi;
    int32_t channel;
    bool    open;
    String  bssid;
};

// Escaneia redes 2.4GHz. Retorna a quantidade e preenche out.
int scan(std::vector<Network>& out);

// Conecta usando "wifi_ssid"/"wifi_pass" da config. showUi mostra progresso.
bool connectSaved(bool showUi = true, uint32_t timeoutMs = 15000);

// Se ja' conectado retorna true; senao tenta connectSaved().
bool ensure();

bool   isConnected();
int    rssi();
String ip();
String ssid();
void   disconnect();

} // namespace wifi
} // namespace noir
