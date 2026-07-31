# Módulo 🔐 Segurança

Utilitários que funcionam **100% offline**. Ótimo módulo para começar a codar de verdade (não depende de rede).

## Objetivo
- **Gerador de senhas** (configurável).
- **Cofre criptografado** (AES-256) para senhas/segredos.
- **TOTP** (códigos de 2FA, compatível com Google Authenticator).
- **Codificador Base64** (encode/decode).
- **Gerador de QR Code**.

## APIs / bibliotecas
| Função | O que usar |
|---|---|
| Criptografia (AES/SHA) | **mbedTLS** — **já vem no core ESP32**, não precisa instalar |
| Base64 | `mbedtls/base64.h` (`mbedtls_base64_encode/decode`) |
| TOTP | HMAC-SHA1 (mbedTLS) + contador de tempo, ou lib **TOTP-Arduino** |
| QR | **M5GFX nativo** `display.qrcode(texto, x, y, tam, versão)` — sem lib extra! Ou `ricmoo/QRCode` para o bitmap cru |
| Armazenamento | **NVS** (`Preferences`), idealmente com **flash encryption** |
| Aleatoriedade | `esp_random()` (TRNG de hardware) |

## Detalhe por ferramenta

### Gerador de senhas
- Opções: comprimento, incluir maiúsculas/números/símbolos, evitar ambíguos (`l/1/O/0`).
- **Fonte de entropia:** `esp_random()` (gerador de hardware — não use `rand()`).
- **Noir:** senha grande em mono, botão "copiar para cofre" e "gerar QR".
- **Dificuldade:** 🟢 Baixa. **Comece por aqui.**

### Cofre criptografado
- **Modelo:** o usuário define uma **senha-mestra**; dela deriva-se uma chave (**PBKDF2**/SHA-256 com salt) que criptografa (AES-256-CBC/GCM) o banco de entradas.
- **Onde guardar:** blob cifrado na NVS (ou arquivo no SD). A senha-mestra **nunca** é gravada — só o hash/verificador.
- **Fluxo:** desbloquear (pede mestra) → listar/adicionar/ver entradas → bloquear (limpa a chave da RAM).
- **Cuidados:** zere buffers de chave após uso; considere `flash encryption` do ESP32-S3 para proteger a NVS fisicamente.
- **Referência:** **SecureGen** faz exatamente isso (AES-256 + mbedTLS) — bom estudo.
- **Dificuldade:** 🔴 Alta (fazer cripto **certo** exige cuidado: KDF, IV único, MAC/GCM contra adulteração).

### TOTP (2FA)
- **Como funciona:** RFC 6238 — HMAC-SHA1(segredo_base32, tempo/30s) → trunca em 6 dígitos.
- **Passos:** decodificar o segredo Base32 → obter epoch (precisa de **NTP**, pois não há RTC) → HMAC-SHA1 (mbedTLS) → dynamic truncation → 6 dígitos + barra de tempo restante.
- **Guardar segredos** no cofre criptografado (não em texto plano!).
- **Dificuldade:** 🟡 Média (Base32 + HMAC + janela de tempo).

### Codificador Base64
- `mbedtls_base64_encode/decode`. Entrada por teclado ou de um arquivo do SD.
- **Dificuldade:** 🟢 Baixa.

### Gerador de QR Code
- **Caminho fácil:** `M5.Lcd.qrcode("texto", x, y, size, version)` — a M5GFX desenha direto.
- **Caminho Noir:** gerar o bitmap com `ricmoo/QRCode` e renderizar com **estilo** (módulos como quadradinhos de tinta, quiet zone, talvez halftone) para casar com o tema.
- **Usos:** exportar senha/segredo TOTP, compartilhar config de WiFi, etc.
- **Dificuldade:** 🟢 Baixa (fácil) / 🟡 (versão estilizada).

## Boas práticas de segurança
- **Nunca** logar segredos no serial.
- Derivar chave com **KDF** (PBKDF2/scrypt), nunca usar a senha direto como chave.
- **IV/nonce único** por criptografia; com GCM você ganha integridade (detecta adulteração).
- Limpar (`memset`) material sensível da RAM após uso.
- Ativar **flash encryption** e **secure boot** se for levar a sério a proteção física (avançado).

## Dificuldade geral
🟡 Média — mas com uma faixa: gerador/Base64/QR são 🟢, TOTP é 🟡, cofre é 🔴.

## Ordem recomendada
1. **Gerador de senhas** (🟢) — entropia + UI.
2. **Base64** (🟢) — primeiro contato com mbedTLS.
3. **QR Code** (🟢) — feedback visual gratificante.
4. **TOTP** (🟡) — HMAC + NTP.
5. **Cofre** (🔴) — junta tudo: KDF, AES, NVS.

## Recursos
- **mbedTLS no ESP-IDF:** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/protocols/mbedtls.html>
- **Preferences/NVS:** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html>
- **Flash encryption (S3):** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/flash-encryption.html>
- **RFC 6238 (TOTP):** <https://datatracker.ietf.org/doc/html/rfc6238>
- **TOTP-Arduino:** <https://www.arduinolibraries.info/libraries/totp-library>
- **ricmoo/QRCode:** <https://github.com/ricmoo/QRCode>
- **SecureGen (cofre/TOTP referência):** <https://github.com/makepkg/SecureGen>
- **esp_random (TRNG):** <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/random.html>
