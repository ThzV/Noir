// Noir OS  -  Estado global de app
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/app.h"

namespace {
volatile bool s_tx_active = false;
}

namespace noir {

void setTxActive(bool active) { s_tx_active = active; }
bool txActive()               { return s_tx_active; }

} // namespace noir
