// ============================================================================
//  Noir OS  -  Cliente HTTP + helpers
//
//  Base de TODO o modulo Servidor (Docker/Portainer/AdGuard/Uptime Kuma) e de
//  varias telas (clima, etc.). Fala HTTP/HTTPS e devolve corpo cru (use
//  ArduinoJson para parsear). Ver docs/modulos/3-servidor.md.
//
//  Nota sobre TLS: insecure=true NAO valida o certificado (util para homelab
//  com cert self-signed em rede confiavel). Para acesso remoto pela internet,
//  prefira um tunel/VPN (Tailscale/WireGuard) ou reverse-proxy com cert valido.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once
#include <Arduino.h>
#include <vector>

namespace noir {
namespace net {

struct Header { String name; String value; };

struct Resp {
    int    code;   // status HTTP (>0) ou erro (<0: -2 sem WiFi, -3 begin falhou)
    String body;
    bool ok() const { return code >= 200 && code < 300; }
};

Resp get(const String& url, const std::vector<Header>& headers = {},
         bool insecure = true, uint32_t timeoutMs = 8000);

Resp post(const String& url, const String& body,
          const String& contentType = "application/json",
          const std::vector<Header>& headers = {},
          bool insecure = true, uint32_t timeoutMs = 8000);

// Monta "Authorization: Basic base64(user:pass)".
Header basicAuth(const String& user, const String& pass);

} // namespace net
} // namespace noir
