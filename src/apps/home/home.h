// ============================================================================
//  Noir OS  -  Modulo HOME (dashboard + configuracao de clima)
//
//  Contrato de exportacao do modulo. O app_registry usa runDashboard() como a
//  tela inicial (chamada pelo launcher) e HOME_CFG_APPS[] para adicionar o app
//  "Clima" na categoria de configuracao.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"

namespace apps {
namespace home {

// Tela inicial (dashboard). Loop bloqueante: desenha relogio/data/clima/status
// e RETORNA quando o usuario aperta ENTER (o launcher entao abre o menu).
void runDashboard();

// App(s) de configuracao da Home (atualmente so o "Clima"). Entram na
// categoria Config do menu principal via configAppArrays.
extern const noir::AppEntry HOME_CFG_APPS[];
extern const int            HOME_CFG_APPS_COUNT;

} // namespace home
} // namespace apps
