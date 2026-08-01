// ============================================================================
//  Noir OS  -  Launcher (a "casca" do sistema)
//
//  Mostra o dashboard (Home) e, ao ENTER, o menu de categorias montado pelo
//  app_registry. Trata a confirmacao obrigatoria dos apps de perigo (TX).
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once

namespace noir {

// Loop principal do OS (nunca retorna).
void runLauncher();

// Dashboard minimo do nucleo (usado se nenhum modulo Home for registrado).
void fallbackDashboard();

} // namespace noir
