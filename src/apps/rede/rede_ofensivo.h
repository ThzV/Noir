// ============================================================================
//  Noir OS  -  Categoria "Rede / Ofensivo" (TX ativo - PERIGO)
//
//  Ferramentas que TRANSMITEM (sniffer promiscuo, AP/hotspot, beacon spam,
//  evil portal e deauth). Todas sao danger=true. Ver docs/legal-etica.md:
//  use SOMENTE em redes/dispositivos proprios ou com autorizacao ESCRITA.
//
//  Contrato de exportacao consumido pelo app_registry.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include "core/app.h"

namespace apps {
namespace rede {

// Array de apps ofensivos (TX ativo). Deve entrar na categoria "Rede" DEPOIS
// dos apps de reconhecimento (RECON_APPS), pois todos sao de perigo.
extern const noir::AppEntry OFENSIVO_APPS[];
extern const int            OFENSIVO_APPS_COUNT;

} // namespace rede
} // namespace apps
