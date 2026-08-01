// ============================================================================
//  Noir OS  -  Servico de tempo (NTP)
//
//  O Cardputer v1.1 NAO tem RTC persistente: a hora vem de NTP via WiFi.
//  O fuso e' um TZ POSIX guardado na config (chave "tz"). Ex. Brasilia: "<-03>3".
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include <Arduino.h>
#include <time.h>

namespace noir {
namespace timeservice {

void   begin();                          // configura SNTP com o fuso da config
bool   sync(uint32_t timeoutMs = 8000);  // aguarda hora valida (precisa de WiFi)
bool   have();                           // ja' temos hora valida?
bool   now(struct tm& out);              // hora local; false se ainda sem sync

String hhmm();     // "HH:MM"     ou "--:--"
String hhmmss();   // "HH:MM:SS"  ou "--:--:--"
String dateStr();  // "DD/MM/YYYY" ou "--/--/----"

} // namespace timeservice
} // namespace noir
