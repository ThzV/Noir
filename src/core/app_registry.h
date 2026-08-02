// ============================================================================
//  Noir OS  -  Registro de apps (monta o menu principal)
//
//  Cada modulo exporta um array de noir::AppEntry (ex.: apps::rede::RECON_APPS).
//  O app_registry.cpp COMPOE as categorias a partir deles. E' o unico arquivo
//  que conhece todos os modulos -> mantem o launcher desacoplado.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"
#include <vector>

namespace noir {

// Categoria "em tempo de execucao": permite compor apps de varios modulos.
struct CategoryRT {
    const char*           name;
    const char*           hint;
    std::vector<AppEntry> apps;
};

// Categorias do menu principal (montadas na 1a chamada).
const std::vector<CategoryRT>& registry();

// Funcao do dashboard (Home). Pode retornar nullptr -> usa fallbackDashboard().
AppRun dashboardApp();

} // namespace noir
