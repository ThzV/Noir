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

// Emblema da aranha (Spider-Noir): corpo + 8 pernas, desenhado com primitivas.
void drawSpider(int cx, int cy, int r, uint16_t color);

// Icone de WiFi: 3 barras crescentes; 'level' 0..3 acesas em 'on', resto 'off'.
void drawWifiBars(int x, int yBase, int level, uint16_t on, uint16_t off);

// Icone de bateria com nivel proporcional a 'pct' (0..100).
void drawBattery(int x, int y, int pct, uint16_t color);

} // namespace ui
