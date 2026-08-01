// ============================================================================
//  Noir OS  -  Modulo REDE / Reconhecimento (RX passivo)
//
//  Exporta o array de apps de reconhecimento de rede (baixo risco): scan WiFi,
//  scan BLE, DNS lookup, ping, port scanner e speed test. Nenhum deles faz TX
//  ofensivo (injecao 802.11), por isso todos sao danger=false.
//
//  Contrato consumido pelo app_registry na integracao.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"

namespace apps {
namespace rede {

// Array com as ferramentas de reconhecimento (RX passivo).
extern const noir::AppEntry RECON_APPS[];
extern const int            RECON_APPS_COUNT;

} // namespace rede
} // namespace apps
