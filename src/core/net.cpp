// Noir OS  -  Cliente HTTP + helpers
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/net.h"
#include "core/wifi_service.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <memory>
#include "mbedtls/base64.h"

namespace noir {
namespace net {

static Resp request(bool isPost, const String& url, const String& body,
                    const String& contentType, const std::vector<Header>& headers,
                    bool insecure, uint32_t timeoutMs) {
    Resp r{-1, ""};
    if (!noir::wifi::isConnected()) { r.code = -2; return r; }

    bool https = url.startsWith("https");
    std::unique_ptr<WiFiClient> client;
    if (https) {
        auto* c = new WiFiClientSecure();
        if (insecure) c->setInsecure();
        client.reset(c);
    } else {
        client.reset(new WiFiClient());
    }

    HTTPClient http;
    http.setConnectTimeout(timeoutMs);
    http.setTimeout(timeoutMs);
    if (!http.begin(*client, url)) { r.code = -3; return r; }

    for (const auto& h : headers) http.addHeader(h.name, h.value);

    int code;
    if (isPost) {
        if (contentType.length()) http.addHeader("Content-Type", contentType);
        code = http.POST((uint8_t*)body.c_str(), body.length());
    } else {
        code = http.GET();
    }

    r.code = code;
    if (code > 0) r.body = http.getString();
    http.end();
    return r;
}

Resp get(const String& url, const std::vector<Header>& headers, bool insecure, uint32_t timeoutMs) {
    return request(false, url, "", "", headers, insecure, timeoutMs);
}

Resp post(const String& url, const String& body, const String& contentType,
          const std::vector<Header>& headers, bool insecure, uint32_t timeoutMs) {
    return request(true, url, body, contentType, headers, insecure, timeoutMs);
}

Header basicAuth(const String& user, const String& pass) {
    String creds = user + ":" + pass;
    size_t bufLen = ((creds.length() + 2) / 3) * 4 + 1;
    std::vector<unsigned char> out(bufLen + 1, 0);
    size_t olen = 0;
    mbedtls_base64_encode(out.data(), bufLen, &olen,
                          (const unsigned char*)creds.c_str(), creds.length());
    out[olen] = 0;
    return Header{"Authorization", String("Basic ") + String((char*)out.data())};
}

} // namespace net
} // namespace noir
