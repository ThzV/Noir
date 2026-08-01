// Noir OS  -  Configuracao persistente (NVS)
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/config.h"
#include <Preferences.h>

namespace {
Preferences s_prefs;
bool        s_open = false;
}

namespace noir {
namespace config {

void begin() {
    if (!s_open) {
        s_prefs.begin("noir", false);   // namespace "noir", modo leitura/escrita
        s_open = true;
    }
}

String getStr(const char* key, const String& def) { begin(); return s_prefs.getString(key, def); }
void   setStr(const char* key, const String& val) { begin(); s_prefs.putString(key, val); }
int    getInt(const char* key, int def)           { begin(); return s_prefs.getInt(key, def); }
void   setInt(const char* key, int val)           { begin(); s_prefs.putInt(key, val); }
bool   getBool(const char* key, bool def)         { begin(); return s_prefs.getBool(key, def); }
void   setBool(const char* key, bool val)         { begin(); s_prefs.putBool(key, val); }
void   remove(const char* key)                    { begin(); s_prefs.remove(key); }

} // namespace config
} // namespace noir
