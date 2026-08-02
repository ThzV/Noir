// ============================================================================
//  Noir OS  -  Modulo HOME  (dashboard + app de configuracao de clima)
//
//  Este arquivo implementa duas coisas:
//
//   1) runDashboard()  -> a TELA INICIAL do Noir. Um loop bloqueante que
//      desenha um relogio grande, a data, uma linha de clima e um resumo de
//      bateria/WiFi. Retorna quando o usuario aperta ENTER (o launcher abre
//      o menu). Atualiza o relogio ~1x/seg e o clima a cada ~10 min.
//
//   2) HOME_CFG_APPS[] -> o app "Clima", que pede a chave da OpenWeatherMap e
//      a cidade (via ui::textInput) e salva na config (NVS).
//
//  DESIGN Noir: preto/branco de alto contraste. O vermelho (noir::BLOOD) fica
//  reservado para perigo/TX, entao aqui NAO usamos vermelho (exceto o alerta
//  de bateria fraca, que segue o padrao da statusbar).
//
//  Sem PSRAM: nada de buffers grandes. O JSON do clima e' pequeno e parseado
//  com ArduinoJson v7 (o JsonDocument aloca no heap dinamicamente, mas e'
//  pequeno) e imediatamente descartado.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#include "apps/home/home.h"

#include "core/config.h"
#include "core/wifi_service.h"
#include "core/time_service.h"
#include "core/net.h"
#include "core/screensaver.h"
#include "ui/theme.h"
#include "ui/statusbar.h"
#include "ui/widgets.h"
#include "ui/input.h"

#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <functional>

namespace apps {
namespace home {

// ============================================================================
//  Estado do clima (arquivo-local)
//
//  Guardamos o ultimo resultado em memoria para desenhar todo frame sem
//  refazer a requisicao. So chamamos a API na primeira vez e depois a cada
//  ~10 minutos (ou quando o usuario forca com a tecla '/').
// ============================================================================
namespace {

// Intervalo entre atualizacoes automaticas do clima: 10 minutos.
constexpr uint32_t WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;

// Backoff no estado de erro/sem-WiFi: reintenta so a cada 60 s. Sem isso, um
// fetch que falha deixaria a requisicao "sempre vencida" e o GET bloqueante
// rodaria a cada frame (~1 s), congelando o relogio e o ENTER.
constexpr uint32_t WEATHER_ERROR_INTERVAL_MS = 60UL * 1000UL;

// Status possiveis da linha de clima (define o texto e a cor).
enum class WeatherStatus {
    Unconfigured,   // sem "owm_key" salva -> pede para configurar
    NoWifi,         // ha chave, mas o WiFi caiu
    Updating,       // requisicao em andamento
    Ok,             // temos temperatura + descricao validas
    Error,          // rede/HTTP/JSON falhou
};

struct WeatherState {
    WeatherStatus status  = WeatherStatus::Unconfigured;
    float         tempC   = NAN;    // temperatura na unidade escolhida
    char          unit    = 'C';    // 'C' (metric) ou 'F' (imperial)
    String        desc;             // descricao (ex.: "nuvens dispersas")
    uint32_t      lastFetch = 0;    // millis() da ultima TENTATIVA de requisicao
    bool          everFetched = false;
};

WeatherState g_weather;

// ---------------------------------------------------------------------------
//  urlEncode minimo: OpenWeatherMap aceita "Cidade,PAIS", mas o espaco precisa
//  virar "%20". Codificamos qualquer caractere que nao seja seguro em query.
//  (Mantemos letras/numeros e alguns simbolos comuns intactos.)
// ---------------------------------------------------------------------------
String urlEncode(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                    c == '.' || c == '~' || c == ',';
        if (safe) {
            out += c;
        } else {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
//  fetchWeather: faz a requisicao HTTP e atualiza g_weather.
//
//  Retorna true se conseguiu temperatura+descricao validas. Nunca "trava": em
//  qualquer erro (sem WiFi, HTTP != 2xx, JSON invalido) define um status de
//  erro e retorna sem lancar excecao.
// ---------------------------------------------------------------------------
bool fetchWeather() {
    // 1) Precisa de chave configurada.
    String key = noir::config::getStr("owm_key", "");
    if (key.length() == 0) {
        g_weather.status = WeatherStatus::Unconfigured;
        return false;
    }

    // 2) Precisa de WiFi conectado (nao forcamos reconexao aqui para nao
    //    bloquear a tela inicial; a statusbar/Config cuidam disso).
    if (!noir::wifi::isConnected()) {
        g_weather.status = WeatherStatus::NoWifi;
        return false;
    }

    // 3) Monta a URL. Unidade e cidade vem da config (com defaults sensatos).
    String city  = noir::config::getStr("owm_city", "Sao Paulo,BR");
    String units = noir::config::getStr("owm_units", "metric");
    g_weather.unit = (units == "imperial") ? 'F' : 'C';   // metric/standard -> C

    String url = "https://api.openweathermap.org/data/2.5/weather?q=" +
                 urlEncode(city) + "&units=" + urlEncode(units) +
                 "&lang=pt_br&appid=" + urlEncode(key);

    // 4) GET (timeout curto: e' a tela inicial, nao queremos congelar muito).
    //    insecure=false: api.openweathermap.org tem cert de CA publica valido,
    //    entao VALIDAMOS o TLS (host publico na internet).
    noir::net::Resp resp = noir::net::get(url, {}, false, 6000);
    if (!resp.ok()) {
        g_weather.status = WeatherStatus::Error;
        return false;
    }

    // 5) Parseia so os campos que interessam. No ArduinoJson v7 o JsonDocument
    //    aloca no heap (dinamicamente), mas e' pequeno e liberado ao sair.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp.body);
    if (err) {
        g_weather.status = WeatherStatus::Error;
        return false;
    }

    // main.temp e' um numero; weather[0].description e' uma string.
    if (!doc["main"]["temp"].is<float>() && !doc["main"]["temp"].is<int>()) {
        // Ex.: chave invalida devolve {"cod":401,...} sem "main".
        g_weather.status = WeatherStatus::Error;
        return false;
    }
    g_weather.tempC = doc["main"]["temp"].as<float>();
    const char* d   = doc["weather"][0]["description"] | "";
    g_weather.desc  = d;

    // Nota: lastFetch/everFetched sao registrados por maybeFetchWeather no
    // instante da TENTATIVA (nao so no sucesso), para o backoff funcionar.
    g_weather.status = WeatherStatus::Ok;
    return true;
}

// ---------------------------------------------------------------------------
//  maybeFetchWeather: decide se e' hora de atualizar o clima.
//
//  force=true ignora o intervalo (usado quando o usuario aperta '/').
//  onBeforeFetch (opcional) e' chamado com o status ja' em Updating, ANTES do
//  GET bloqueante, para que o chamador desenhe "atualizando..." na tela.
//  Retorna true se uma requisicao foi disparada (para o chamador redesenhar).
// ---------------------------------------------------------------------------
bool maybeFetchWeather(bool force,
                       const std::function<void()>& onBeforeFetch = {}) {
    // Sem chave: nao ha o que buscar; so garante o status correto.
    if (noir::config::getStr("owm_key", "").length() == 0) {
        g_weather.status = WeatherStatus::Unconfigured;
        return false;
    }

    // Backoff: so o estado Ok usa o intervalo longo (10 min). Em erro/sem-WiFi
    // reintentamos a cada 60 s. Isso evita a "tempestade de retries": sem esse
    // limite, um fetch que falha ficaria sempre "vencido" e o GET bloqueante
    // rodaria a cada frame (~1 s), congelando relogio e ENTER.
    uint32_t interval = (g_weather.status == WeatherStatus::Ok)
                            ? WEATHER_INTERVAL_MS
                            : WEATHER_ERROR_INTERVAL_MS;
    bool due = force || !g_weather.everFetched ||
               (millis() - g_weather.lastFetch >= interval);
    if (!due) return false;

    // Registra o instante da TENTATIVA (nao so do sucesso): mesmo que o fetch
    // falhe, o backoff acima segura a proxima ate' o intervalo vencer.
    g_weather.lastFetch   = millis();
    g_weather.everFetched = true;

    // Feedback visual: a requisicao bloqueia por ate' 6 s. Mostra "atualizando"
    // ANTES de bloquear (senao esse status nunca chega a ser desenhado).
    g_weather.status = WeatherStatus::Updating;
    if (onBeforeFetch) onBeforeFetch();
    fetchWeather();
    return true;
}

// ---------------------------------------------------------------------------
//  Texto e cor da linha de clima conforme o status atual.
// ---------------------------------------------------------------------------
String weatherLine() {
    switch (g_weather.status) {
        case WeatherStatus::Unconfigured: return "Clima: configure em Config";
        case WeatherStatus::NoWifi:       return "Clima: sem WiFi";
        case WeatherStatus::Updating:     return "Clima: atualizando...";
        case WeatherStatus::Error:        return "Clima: indisponivel";
        case WeatherStatus::Ok: {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%d%c  %s",
                          (int)lroundf(g_weather.tempC), g_weather.unit,
                          g_weather.desc.c_str());
            // Trunca para nao estourar a largura da tela (Font2), mas numa
            // fronteira de caractere UTF-8: substring(0,30) cru poderia cortar
            // no meio de um acento (ex.: "nublado com garoa") e virar lixo.
            String s = buf;
            if (s.length() > 30) {
                size_t cut = 30;
                // Recua enquanto o byte no corte for continuacao UTF-8 (10xxxxxx),
                // ou seja, enquanto estivermos no meio de um caractere multibyte.
                while (cut > 0 && ((uint8_t)s[cut] & 0xC0) == 0x80) --cut;
                s = s.substring(0, cut);
            }
            return s;
        }
    }
    return "";
}

} // namespace (anonimo)

// ============================================================================
//  runDashboard  -  a tela inicial
// ============================================================================
void runDashboard() {
    M5Canvas& d = ui::gfx();

    // Fecha a lambda por referencia para redesenhar sob demanda.
    auto draw = [&]() {
        ui::clearNoir();
        ui::statusBar("NOIR");

        // ---- Relogio grande (Font7 = display de 7 segmentos) --------------
        d.setFont(&fonts::Font7);
        d.setTextDatum(middle_center);
        d.setTextColor(noir::WHITE, noir::BLACK);
        d.drawString(noir::timeservice::hhmm().c_str(), noir::SCREEN_W / 2, 48);

        // ---- Data (ou aviso de falta de sincronismo NTP) ------------------
        d.setFont(&fonts::Font2);
        d.setTextDatum(middle_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString(noir::timeservice::have()
                         ? noir::timeservice::dateStr().c_str()
                         : "sem sync NTP",
                     noir::SCREEN_W / 2, 80);

        // ---- Linha de clima ------------------------------------------------
        // Cinza claro quando ha dado; cinza medio para avisos/erros.
        bool climaOk = (g_weather.status == WeatherStatus::Ok);
        d.setFont(&fonts::Font2);
        d.setTextColor(climaOk ? noir::BONE : noir::STEEL, noir::BLACK);
        d.drawString(weatherLine().c_str(), noir::SCREEN_W / 2, 100);

        // ---- Resumo de bateria + WiFi (alem do que a statusbar mostra) ----
        // Bateria com nivel numerico e WiFi com RSSI quando conectado.
        int  batt = M5.Power.getBatteryLevel();
        bool wifi = noir::wifi::isConnected();
        char status[40];
        if (wifi) {
            std::snprintf(status, sizeof(status), "BAT %d%%   WiFi %ddBm",
                          batt, (int)noir::wifi::rssi());
        } else {
            std::snprintf(status, sizeof(status), "BAT %d%%   WiFi off", batt);
        }
        d.setFont(&fonts::Font0);
        d.setTextDatum(middle_center);
        d.setTextColor((batt >= 0 && batt < 15) ? noir::BLOOD : noir::STEEL,
                       noir::BLACK);
        d.drawString(status, noir::SCREEN_W / 2, 116);

        // ---- Dica de navegacao --------------------------------------------
        d.setFont(&fonts::Font0);
        d.setTextDatum(bottom_center);
        d.setTextColor(noir::STEEL, noir::BLACK);
        d.drawString("ENTER menu    / atualiza clima",
                     noir::SCREEN_W / 2, noir::SCREEN_H - 2);

        ui::present();
    };

    // Puxa o clima uma vez ja' na entrada. Passamos 'draw' para que a tela
    // mostre "atualizando..." ANTES do fetch bloqueante.
    maybeFetchWeather(false, draw);

    draw();
    uint32_t lastClock    = millis();
    uint32_t lastActivity = millis();

    // ---- Loop principal da tela inicial -----------------------------------
    for (;;) {
        ui::KeyEvent e = ui::readKey();
        if (e.key != ui::Key::None) lastActivity = millis();   // qualquer tecla reseta o ocio

        // ENTER: sai do dashboard -> o launcher abre o menu.
        if (e.key == ui::Key::Enter) return;

        // '/' (mapeada como seta Direita): forca atualizar o clima agora.
        if (e.key == ui::Key::Right) {
            maybeFetchWeather(true, draw);
            draw();
            lastClock = millis();
            continue;
        }

        // Atualiza o relogio ~1x/seg. De quebra, verifica se o clima venceu
        // o intervalo de 10 min (maybeFetchWeather so age quando "due").
        if (millis() - lastClock >= 1000) {
            maybeFetchWeather(false, draw);
            draw();
            lastClock = millis();
        }

        // Screensaver ao ficar ocioso (config "ss_idle"; 0 = desligado).
        int ssIdle = noir::screensaver::idleSeconds();
        if (ssIdle > 0 && (millis() - lastActivity) >= (uint32_t)ssIdle * 1000) {
            noir::screensaver::run();     // bloqueia ate' tecla; restaura o brilho
            lastActivity = millis();
            lastClock    = millis();
            draw();
        }

        delay(10);   // cede CPU (evita busy-loop de 100%)
    }
}

// ============================================================================
//  App de configuracao "Clima"
//
//  Pede a chave da OpenWeatherMap (mascarada) e a cidade, salvando na NVS.
//  Chaves NVS <= 15 chars: "owm_key", "owm_city", "owm_units".
// ============================================================================
namespace {

void appClima() {
    // Valores atuais como default (facilita editar sem redigitar tudo).
    String curCity = noir::config::getStr("owm_city", "Sao Paulo,BR");

    // 1) Chave da API (mascarada). Mantem a atual se o usuario cancelar.
    bool ok = true;
    String key = ui::textInput("OpenWeather API key", "", true, &ok);
    if (!ok) return;                 // cancelou -> nao mexe em nada
    key.trim();
    if (key.length() == 0) {
        ui::messageBox("Clima", "Chave vazia.\nNada foi salvo.");
        return;
    }

    // 2) Cidade no formato "Cidade,PAIS" (ex.: Sao Paulo,BR).
    String city = ui::textInput("Cidade  (ex: Sao Paulo,BR)", curCity, false, &ok);
    if (!ok) return;
    city.trim();
    if (city.length() == 0) city = curCity;

    // 3) Salva. Fixamos a unidade em "metric" (Celsius) por padrao.
    noir::config::setStr("owm_key", key);
    noir::config::setStr("owm_city", city);
    if (noir::config::getStr("owm_units", "").length() == 0)
        noir::config::setStr("owm_units", "metric");

    // 4) Forca uma atualizacao para o usuario ver o resultado no proximo
    //    retorno ao dashboard.
    g_weather.everFetched = false;

    ui::messageBox("Clima", String("Salvo.\nCidade: ") + city +
                                 "\nUnidade: Celsius");
}

} // namespace (anonimo)

// ---- Array exportado para a categoria Config -------------------------------
// So configuracao (nao ha TX/perigo aqui), entao danger=false.
const noir::AppEntry HOME_CFG_APPS[] = {
    {"Clima", "owm", appClima, false},
};
const int HOME_CFG_APPS_COUNT = sizeof(HOME_CFG_APPS) / sizeof(HOME_CFG_APPS[0]);

} // namespace home
} // namespace apps
