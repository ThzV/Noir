// ============================================================================
//  Noir OS  -  Categoria "Ferramentas" (hardware / barramento I2C)
//
//  Dois apps que falam com o mundo fisico pela porta Grove do Cardputer, que
//  e um barramento I2C:
//    1) Scanner I2C  - varre 0x08..0x77 e lista os enderecos que respondem,
//                      com um "palpite" do chip para os enderecos conhecidos.
//    2) Unit RTC     - detecta e conversa com o relogio BM8563 (M5 Unit-RTC):
//                      le a hora atual e, se houver hora valida por NTP, grava
//                      a hora nos registradores do chip ("acertar o relogio").
//
//  Porta Grove do Cardputer = I2C:  SDA = GPIO 2,  SCL = GPIO 1.
//  Usamos a Wire (TwoWire) do Arduino: Wire.begin(2, 1). Nenhuma lib externa.
//
//  O BM8563 (identico ao PCF8563/HYM8563) guarda data/hora em BCD nos
//  registradores 0x02..0x08. Implementamos leitura/escrita direto via Wire,
//  sem depender de nenhuma biblioteca de RTC.
//
//  Nenhum app aqui transmite RF nem apaga dados: todos danger=false. Acertar o
//  relogio sobrescreve a hora do chip, mas so acontece apos ui::confirm.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#include "apps/ferramentas/ferramentas.h"

#include "ui/widgets.h"     // listView, confirm, messageBox, redStripe, ...
#include "core/time_service.h"

#include <Arduino.h>        // String
#include <Wire.h>           // barramento I2C (porta Grove)
#include <vector>
#include <cstdio>           // snprintf
#include <time.h>           // struct tm

namespace apps {
namespace ferramentas {

// ============================================================================
//  Tudo abaixo (helpers + apps) e' file-local: namespace anonimo.
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
//  Barramento I2C (porta Grove)
// ----------------------------------------------------------------------------

// Pinos I2C da porta Grove do Cardputer.
constexpr int I2C_SDA = 2;
constexpr int I2C_SCL = 1;

// Endereco do BM8563 (M5 Unit-RTC). Fixo pelo chip (nao configuravel).
constexpr uint8_t RTC_ADDR = 0x51;

// Inicializa a Wire uma unica vez (idempotente). Chamar antes de qualquer
// transacao I2C. begin() pode ser chamado varias vezes sem dano, mas guardamos
// um flag para deixar claro que a configuracao dos pinos so precisa acontecer
// uma vez por sessao.
void ensureWire() {
    static bool started = false;
    if (!started) {
        Wire.begin(I2C_SDA, I2C_SCL);
        started = true;
    }
}

// Testa se ha um dispositivo respondendo em 'addr'. Um endTransmission() com
// ACK (retorno 0) significa que alguem puxou a linha -> dispositivo presente.
bool i2cPresent(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// Palpite de chip para enderecos I2C conhecidos (retorna "" se desconhecido).
const char* knownHint(uint8_t addr) {
    switch (addr) {
        case 0x51: return "RTC BM8563/HYM8563";
        case 0x68: return "DS3231/RTC/IMU";
        case 0x76: return "BMP/BME280";
        case 0x77: return "BMP/BME280";
        case 0x3C: return "OLED SSD1306";
        case 0x23: return "BH1750";
        default:   return "";
    }
}

// ----------------------------------------------------------------------------
//  BCD <-> decimal (o BM8563 guarda cada campo em BCD)
// ----------------------------------------------------------------------------
uint8_t bcd2dec(uint8_t b) { return (uint8_t)((b >> 4) * 10 + (b & 0x0f)); }
uint8_t dec2bcd(uint8_t d) { return (uint8_t)(((d / 10) << 4) | (d % 10)); }

// ----------------------------------------------------------------------------
//  BM8563: leitura/escrita dos registradores de tempo (0x02..0x08)
//
//  Mapa (todos BCD, exceto os bits de flag):
//    0x02 segundos  bit7 = VL (Voltage Low: a hora pode ter sido perdida)
//    0x03 minutos
//    0x04 horas     (formato 24h)
//    0x05 dia do mes
//    0x06 dia da semana (0..6)
//    0x07 mes       bit7 = seculo (1 => 19xx, 0 => 20xx)
//    0x08 ano       (00..99, relativo ao seculo)
// ----------------------------------------------------------------------------

// Le a hora do RTC para 'out' (struct tm, campos padrao da libc). 'vl' recebe
// true se o bit VL estiver setado (bateria fraca/hora suspeita). Retorna false
// se o chip nao responder no meio da transacao.
bool bm8563Read(struct tm& out, bool& vl) {
    Wire.beginTransmission(RTC_ADDR);
    Wire.write((uint8_t)0x02);                     // ponteiro no reg. de segundos
    if (Wire.endTransmission(false) != 0) return false;  // repeated start

    if (Wire.requestFrom((int)RTC_ADDR, 7) != 7) return false;
    uint8_t b[7];
    for (int i = 0; i < 7; i++) b[i] = (uint8_t)Wire.read();

    vl          = (b[0] & 0x80) != 0;
    out.tm_sec  = bcd2dec(b[0] & 0x7f);
    out.tm_min  = bcd2dec(b[1] & 0x7f);
    out.tm_hour = bcd2dec(b[2] & 0x3f);
    out.tm_mday = bcd2dec(b[3] & 0x3f);
    out.tm_wday = b[4] & 0x07;
    out.tm_mon  = bcd2dec(b[5] & 0x1f) - 1;         // tm_mon e 0..11
    int base    = (b[5] & 0x80) ? 1900 : 2000;      // bit de seculo
    out.tm_year = base + bcd2dec(b[6]) - 1900;      // tm_year e "anos desde 1900"
    out.tm_isdst = -1;
    return true;
}

// Escreve a hora de 't' nos registradores do RTC. Zera o bit VL (a hora passa a
// ser confiavel) e ajusta o bit de seculo conforme o ano. Retorna false se o
// chip nao confirmar a escrita.
bool bm8563Write(const struct tm& t) {
    int fullYear   = t.tm_year + 1900;
    uint8_t centBit = (fullYear < 2000) ? 0x80 : 0x00;
    uint8_t yy      = (uint8_t)(((fullYear < 2000) ? (fullYear - 1900)
                                                   : (fullYear - 2000)) % 100);

    Wire.beginTransmission(RTC_ADDR);
    Wire.write((uint8_t)0x02);
    Wire.write(dec2bcd((uint8_t)t.tm_sec) & 0x7f);          // segundos + VL=0
    Wire.write(dec2bcd((uint8_t)t.tm_min));
    Wire.write(dec2bcd((uint8_t)t.tm_hour));
    Wire.write(dec2bcd((uint8_t)t.tm_mday));
    Wire.write((uint8_t)(t.tm_wday & 0x07));
    Wire.write((uint8_t)(dec2bcd((uint8_t)(t.tm_mon + 1)) | centBit));
    Wire.write(dec2bcd(yy));
    return Wire.endTransmission() == 0;
}

// Formata "DD/MM/YYYY HH:MM:SS" a partir de um struct tm ja preenchido.
String fmtDateTime(const struct tm& t) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
             t.tm_hour, t.tm_min, t.tm_sec);
    return String(buf);
}

// ============================================================================
//  APP 1  -  Scanner I2C
// ============================================================================
void appScanner() {
    ensureWire();

    // Varre a faixa "de usuario" do I2C de 7 bits (0x08..0x77). Fora dela ficam
    // enderecos reservados pela norma, que nao vale a pena sondar.
    std::vector<String> achados;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (!i2cPresent(addr)) continue;
        char linha[48];
        const char* hint = knownHint(addr);
        if (hint[0])
            snprintf(linha, sizeof(linha), "0x%02X  %s", addr, hint);
        else
            snprintf(linha, sizeof(linha), "0x%02X", addr);
        achados.push_back(String(linha));
    }

    if (achados.empty()) {
        ui::messageBox("Scanner I2C",
            "Nada encontrado.\n\nConecte um dispositivo\nna porta Grove (SDA=2,\nSCL=1) e tente de novo.");
        return;
    }

    // Lista rolavel com os enderecos. Selecionar nao faz nada (informativo);
    // qualquer saida (crase) fecha o app.
    String titulo = "I2C: " + String((int)achados.size()) + " achado(s)";
    ui::listView(titulo.c_str(), achados);
}

// ============================================================================
//  APP 2  -  Unit RTC (BM8563)
// ============================================================================

// Le e exibe a hora atual do RTC.
void rtcLer() {
    struct tm t = {};
    bool vl = false;
    if (!bm8563Read(t, vl)) {
        ui::redStripe("Falha ao ler o RTC");
        return;
    }
    String msg = fmtDateTime(t);
    if (vl) {
        // Bit VL setado: o oscilador parou em algum momento (bateria fraca ou
        // primeira energizacao) e a hora pode nao ser confiavel.
        msg += "\n\n! Bit VL ligado:\nhora pode estar\nerrada. Sincronize.";
    }
    ui::messageBox("Hora do RTC", msg);
}

// Grava no RTC a hora atual obtida por NTP (timeservice).
void rtcSincronizar() {
    if (!noir::timeservice::have()) {
        ui::messageBox("Sincronizar RTC",
            "Ainda nao ha hora valida.\nConecte o WiFi e\nsincronize o relogio\n(Config > Fuso/NTP)\nantes de gravar no RTC.");
        return;
    }
    struct tm agora;
    if (!noir::timeservice::now(agora)) {
        ui::redStripe("Sem hora do sistema");
        return;
    }
    if (!ui::confirm("Sincronizar RTC",
                     "Gravar no RTC:\n" + fmtDateTime(agora) + " ?")) {
        return;
    }
    if (bm8563Write(agora)) ui::messageBox("Sincronizar RTC", "Hora gravada no RTC\ncom sucesso.");
    else                    ui::redStripe("Falha ao gravar no RTC");
}

void appRtc() {
    ensureWire();

    // Sem o chip presente nao ha o que fazer: avisa e sai.
    if (!i2cPresent(RTC_ADDR)) {
        ui::messageBox("Unit RTC",
            "RTC BM8563 nao detectado\nem 0x51.\n\nLigue a Unit-RTC na\nporta Grove (SDA=2,\nSCL=1) e tente de novo.");
        return;
    }

    for (;;) {
        int r = ui::listView("Unit RTC (BM8563)", {
            "Ler hora do RTC",
            "Sincronizar do NTP",
        });
        if (r < 0) return;
        if (r == 0) rtcLer();
        else if (r == 1) rtcSincronizar();
    }
}

} // namespace anonimo

// ============================================================================
//  Exportacao da categoria. Nenhum app transmite/destrói: danger=false.
// ============================================================================
const noir::AppEntry FERRAMENTAS_APPS[] = {
    { "Scanner I2C", "i2c", appScanner, false },
    { "Unit RTC",    "rtc", appRtc,     false },
};
const int FERRAMENTAS_APPS_COUNT =
    (int)(sizeof(FERRAMENTAS_APPS) / sizeof(FERRAMENTAS_APPS[0]));

} // namespace ferramentas
} // namespace apps
