// ============================================================================
//  Noir OS  -  Configuracao persistente (NVS via Preferences)
//
//  Guarda ajustes e segredos (SSID/senha, tokens, fuso, brilho...) na NVS.
//
//  ATENCAO: chaves da NVS tem no maximo 15 caracteres. Use nomes curtos
//  (ex.: "wifi_ssid", "pt_url", "ag_tok"). Ver docs/03-arquitetura.md.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include <Arduino.h>

namespace noir {
namespace config {

void   begin();                                       // abre a NVS (idempotente)

String getStr(const char* key, const String& def = "");
void   setStr(const char* key, const String& val);
int    getInt(const char* key, int def = 0);
void   setInt(const char* key, int val);
bool   getBool(const char* key, bool def = false);
void   setBool(const char* key, bool val);
void   remove(const char* key);

} // namespace config
} // namespace noir
