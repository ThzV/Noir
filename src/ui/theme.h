// ============================================================================
//  Noir OS  -  Toolkit de desenho (tema Noir)
//  Renderiza tudo em um unico canvas (sprite) de tela cheia para evitar
//  flicker. Sem PSRAM: um sprite de 240x135x16bpp (~63 KB) e' suficiente.
//
//  Copyright (C) 2026  Noir OS contributors
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include <M5Cardputer.h>
#include "noir_theme.h"

namespace ui {

// Cria o canvas compartilhado. Retorna false se a alocacao falhar.
bool init();

// Canvas compartilhado onde todas as telas desenham.
M5Canvas& gfx();

// Envia o canvas para o display.
void present();

// Fundo Noir completo: preto + grao de filme + moldura/vinheta.
void clearNoir();

// Espalha 'density' pixels de ruido (grao de filme).
void drawGrain(int density);

// Moldura escura nas bordas (efeito vinheta simplificado).
void drawVignette();

// Painel com borda grossa estilo "quadro de HQ".
void panel(int x, int y, int w, int h, const char* title);

} // namespace ui
