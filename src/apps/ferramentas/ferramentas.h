// ============================================================================
//  Noir OS  -  Categoria "Ferramentas" (hardware / barramento I2C)
//
//  Contrato de exportacao do modulo. O app_registry monta a categoria a partir
//  destes simbolos. As funcoes run() de cada app ficam em namespace anonimo
//  dentro do .cpp (sao file-local, ninguem de fora precisa enxerga-las).
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"

namespace apps {
namespace ferramentas {

// Array de apps desta categoria e sua contagem (definidos em ferramentas.cpp).
extern const noir::AppEntry FERRAMENTAS_APPS[];
extern const int            FERRAMENTAS_APPS_COUNT;

} // namespace ferramentas
} // namespace apps
