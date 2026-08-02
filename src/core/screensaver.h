// ============================================================================
//  Noir OS  -  Screensaver + repouso (deep sleep leve)
//
//  Protetor de tela tematico (aranha Spider-Noir + relogio + grao). Depois de
//  um tempo extra ocioso, entra em REPOUSO: tela apagada + light sleep (seguro,
//  nao reseta o chip; acorda em qualquer tecla). Grande economia de bateria.
//
//  Config (NVS):
//    "ss_idle"  -> segundos de ocio para disparar o screensaver (0 = desligado)
//    "ss_sleep" -> segundos no screensaver ate' o repouso (0 = nunca)
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once

namespace noir {
namespace screensaver {

// Roda o screensaver (e o repouso, se configurado). Bloqueia ate' o usuario
// apertar qualquer tecla; ao voltar, restaura o brilho salvo.
void run();

// Segundos de ocio para disparar (config "ss_idle"; default 30; 0 = desligado).
int idleSeconds();

} // namespace screensaver
} // namespace noir
