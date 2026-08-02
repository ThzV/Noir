// ============================================================================
//  Noir OS  -  Abstracao de teclado
//
//  Traduz o teclado fisico do Cardputer em eventos logicos. As setas do
//  Cardputer estao impressas nas teclas ; . , /  (cima/baixo/esq/dir).
//  ENTER seleciona; a tecla ` (canto superior esquerdo) funciona como voltar.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once

namespace ui {

enum class Key {
    None, Up, Down, Left, Right, Enter, Back, Del, Space, Char
};

struct KeyEvent {
    Key  key;
    char ch;   // preenchido quando key == Char (e tambem nas setas/space)
};

// Le um evento (nao bloqueante). Chama M5Cardputer.update() internamente.
KeyEvent readKey();

// Bloqueia ate' uma tecla ser pressionada.
KeyEvent waitKey();

} // namespace ui
