// ============================================================================
//  Noir OS  -  Categoria "Produtividade" (offline)
//
//  Ferramentas simples e uteis que nao dependem de rede:
//    1) Cronometro        - tempo decorrido com voltas (laps)
//    2) Conversor          - unidades (comprimento, massa, temperatura, ...)
//    3) Pomodoro           - ciclos de foco/pausa com beep e config na NVS
//    4) Calendario         - grade do mes (precisa de NTP para saber "hoje")
//
//  Contrato de exportacao: o app_registry monta a categoria a partir do array
//  PRODUTIVIDADE_APPS. As funcoes run() de cada app vivem em namespace anonimo
//  (arquivo-local) dentro do .cpp; aqui expomos apenas o array.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"

namespace apps {
namespace produtividade {

// Array de apps desta categoria (todos danger=false: nada de TX/destrutivo).
extern const noir::AppEntry PRODUTIVIDADE_APPS[];
extern const int            PRODUTIVIDADE_APPS_COUNT;

} // namespace produtividade
} // namespace apps
