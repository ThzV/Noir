// Noir OS  -  Servico de tempo (NTP)
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/time_service.h"
#include "core/config.h"

namespace {
bool s_configured = false;
}

namespace noir {
namespace timeservice {

void begin() {
    // TZ POSIX guardado na config. Padrao: Brasilia (UTC-3, sem horario de verao).
    String tz = noir::config::getStr("tz", "<-03>3");
    configTzTime(tz.c_str(), "pool.ntp.org", "a.st1.ntp.br", "time.google.com");
    s_configured = true;
}

bool now(struct tm& out) {
    // getLocalTime aguarda ate' 10ms; retorna false se a hora ainda nao foi
    // sincronizada. Reforcamos checando o ano.
    if (!getLocalTime(&out, 10)) return false;
    return (out.tm_year + 1900) >= 2016;
}

bool have() {
    struct tm t;
    return now(t);
}

bool sync(uint32_t timeoutMs) {
    if (!s_configured) begin();
    uint32_t start = millis();
    struct tm t;
    while (millis() - start < timeoutMs) {
        if (now(t)) return true;
        delay(200);
    }
    return false;
}

String hhmm() {
    struct tm t;
    if (!now(t)) return "--:--";
    char b[8];
    strftime(b, sizeof(b), "%H:%M", &t);
    return String(b);
}

String hhmmss() {
    struct tm t;
    if (!now(t)) return "--:--:--";
    char b[12];
    strftime(b, sizeof(b), "%H:%M:%S", &t);
    return String(b);
}

String dateStr() {
    struct tm t;
    if (!now(t)) return "--/--/----";
    char b[16];
    strftime(b, sizeof(b), "%d/%m/%Y", &t);
    return String(b);
}

} // namespace timeservice
} // namespace noir
