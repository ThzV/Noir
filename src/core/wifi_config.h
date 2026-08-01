// ============================================================================
//  Noir OS  -  App de configuracao de WiFi (tambem serve de REFERENCIA)
//
//  Este arquivo e' o EXEMPLO CANONICO de como escrever um app do Noir:
//  usa os servicos (wifi/config/time) + os widgets (progress/listView/
//  textInput/messageBox). Estude-o antes de escrever novos modulos.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once

namespace noir {

// Escaneia, deixa o usuario escolher a rede, digitar a senha, salva e conecta.
void setupWifiApp();

} // namespace noir
