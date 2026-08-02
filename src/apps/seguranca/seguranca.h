// ============================================================================
//  Noir OS  -  Categoria "Seguranca" (ferramentas offline)
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
namespace seguranca {

// Array de apps desta categoria e sua contagem (definidos em seguranca.cpp).
extern const noir::AppEntry SEGURANCA_APPS[];
extern const int            SEGURANCA_APPS_COUNT;

} // namespace seguranca
} // namespace apps
