// ============================================================================
//  Noir OS  -  Tokens do design system "Spider-Man Noir"
//  Detalhes e racional: docs/04-design-noir.md
//
//  Cores em RGB565 (formato da M5GFX). Regra do acento:
//  NOIR_BLOOD (vermelho) = SOMENTE perigo / transmissao ativa (TX).
//
//  Copyright (C) 2026  Noir OS contributors
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include <cstdint>

namespace noir {

// Converte RGB888 -> RGB565 em tempo de compilacao.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ---- Paleta -------------------------------------------------------------
constexpr uint16_t BLACK = rgb565(0,   0,   0);    // fundo padrao
constexpr uint16_t INK   = rgb565(13,  13,  13);   // camadas / barras
constexpr uint16_t ASH   = rgb565(58,  58,  58);   // linhas sutis / desabilitado
constexpr uint16_t STEEL = rgb565(122, 122, 122);  // texto secundario
constexpr uint16_t BONE  = rgb565(214, 214, 214);  // texto principal (branco "sujo")
constexpr uint16_t WHITE = rgb565(255, 255, 255);  // destaque / item selecionado
constexpr uint16_t BLOOD = rgb565(139, 0,   0);    // ACENTO: perigo / TX ativo

// ---- Layout -------------------------------------------------------------
constexpr int SCREEN_W    = 240;
constexpr int SCREEN_H    = 135;
constexpr int STATUSBAR_H = 16;

} // namespace noir
