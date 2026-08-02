// ============================================================================
//  Noir OS  -  Modulo SERVIDOR  (contrato de exportacao)
//
//  Painel de bolso do homelab, pensado para ACESSO REMOTO: fala com Portainer,
//  AdGuard Home e Uptime Kuma via HTTP+JSON (noir::net), de qualquer lugar com
//  WiFi. Este header so' expoe o array de apps para o app_registry montar o
//  menu; a implementacao (funcoes run) fica em servidor.cpp, arquivo-local.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"

namespace apps {
namespace servidor {

// Array de apps do modulo Servidor (ordem: leitura primeiro, PERIGO por ultimo).
extern const noir::AppEntry SERVIDOR_APPS[];
extern const int            SERVIDOR_APPS_COUNT;

} // namespace servidor
} // namespace apps
