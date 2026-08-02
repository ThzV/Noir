// ============================================================================
//  Noir OS  -  Categoria "Arquivos" (cartao SD)
//
//  Contrato de exportacao do modulo: o app_registry monta a categoria a partir
//  deste array. As funcoes run() ficam em namespace anonimo dentro do .cpp.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"

namespace apps {
namespace arquivos {

extern const noir::AppEntry ARQUIVOS_APPS[];
extern const int            ARQUIVOS_APPS_COUNT;

} // namespace arquivos
} // namespace apps
