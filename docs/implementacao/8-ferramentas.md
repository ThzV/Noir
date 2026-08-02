<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Modulo Ferramentas (hardware / I2C)

Ferramentas que falam com o mundo fisico pela **porta Grove** do Cardputer, que
e um barramento **I2C**. Nenhum app transmite RF nem apaga dados de terceiros,
entao todos sao `danger=false` (acertar o relogio sobrescreve a hora do chip,
mas so apos `ui::confirm`).

Arquivos:

- `src/apps/ferramentas/ferramentas.h` — contrato de exportacao (`FERRAMENTAS_APPS[]`).
- `src/apps/ferramentas/ferramentas.cpp` — helpers I2C/BM8563 + os 2 apps.

Bibliotecas extras: **nenhuma**. A `Wire` (I2C) ja vem no core do ESP32/Arduino.

## Porta Grove = I2C

No Cardputer, a porta Grove expoe o barramento I2C nestes pinos:

```
SDA = GPIO 2     SCL = GPIO 1
```

Inicializamos uma unica vez com `Wire.begin(2, 1)` (helper `ensureWire()`, com
flag `static` para configurar os pinos so na primeira chamada).

## 1) Scanner I2C

Varre a faixa "de usuario" do enderecamento de 7 bits, **0x08..0x77** (fora dela
ficam enderecos reservados pela norma, que nao vale a pena sondar). Para cada
endereco fazemos um "ping" de barramento:

```cpp
Wire.beginTransmission(addr);
bool presente = (Wire.endTransmission() == 0);   // 0 = ACK => alguem respondeu
```

Um `endTransmission()` que retorna `0` significa que o dispositivo puxou a linha
(ACK) — logo esta **presente**. Qualquer outro retorno = ninguem naquele
endereco.

Os achados sao listados em hex (`0xNN`) num `ui::listView` rolavel. Para
enderecos **conhecidos** anexamos um palpite do chip:

| Endereco | Palpite |
|----------|---------|
| `0x51`   | RTC BM8563/HYM8563 |
| `0x68`   | DS3231 / RTC / IMU |
| `0x76`, `0x77` | BMP/BME280 |
| `0x3C`   | OLED SSD1306 |
| `0x23`   | BH1750 |

Se nada responder, mostramos **"Nada encontrado"** com a dica dos pinos.

## 2) Unit RTC (BM8563)

Conversa com o relogio **BM8563** da M5 Unit-RTC (endereco fixo **0x51**). O
BM8563 e identico ao PCF8563/HYM8563. Primeiro detectamos o chip com o mesmo
"ping" do scanner; se nao responder, um `messageBox` avisa e o app sai.

Com o chip presente, um menu oferece:

- **Ler hora do RTC** — le os registradores e mostra `DD/MM/YYYY HH:MM:SS`.
- **Sincronizar do NTP** — grava no RTC a hora que o `timeservice` obteve por NTP.

### Registradores (BCD)

O BM8563 guarda data/hora em **BCD** nos registradores `0x02..0x08`. Lemos os 7
bytes de uma vez posicionando o ponteiro em `0x02` e fazendo `requestFrom` com um
*repeated start* (`endTransmission(false)`):

| Reg | Campo | Observacao |
|-----|-------|------------|
| `0x02` | segundos | bit7 = **VL** (Voltage Low) |
| `0x03` | minutos | |
| `0x04` | horas | formato 24h |
| `0x05` | dia do mes | |
| `0x06` | dia da semana | 0..6 |
| `0x07` | mes | bit7 = **seculo** (1 => 19xx, 0 => 20xx) |
| `0x08` | ano | 00..99, relativo ao seculo |

**BCD <-> decimal** com dois helpers de uma linha:

```cpp
uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0f); }
uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }
```

Ao ler, mascaramos cada campo (ex.: horas com `0x3f`, mes com `0x1f`) para
descartar os bits de flag. Convertemos para os campos padrao de `struct tm`
(`tm_mon` e 0..11, `tm_year` e "anos desde 1900"), reaproveitando o mesmo tipo
que o `timeservice` usa — assim a sincronizacao vira uma copia direta de campos.

### Bit VL (hora suspeita)

O bit **VL** no registrador de segundos fica ligado quando o oscilador parou em
algum momento (bateria fraca ou primeira energizacao): a hora guardada pode nao
ser confiavel. Ao **ler**, se `VL=1`, avisamos que convem sincronizar. Ao
**escrever**, gravamos o registrador de segundos com `VL=0`, declarando a nova
hora confiavel.

### Sincronizacao

`Sincronizar do NTP` so funciona se ja houver hora valida:

```cpp
if (!noir::timeservice::have()) { /* avisa: conecte o WiFi e sincronize */ }
struct tm agora;
noir::timeservice::now(agora);        // hora local (fuso da config)
// confirma e grava:
bm8563Write(agora);                   // escreve 0x02..0x08 em BCD, VL=0
```

Antes de gravar, um `ui::confirm` mostra a data/hora que sera escrita. O bit de
**seculo** e ajustado pelo ano (`< 2000` => `0x80`), e o ano vira `00..99`
relativo ao seculo.

## Armadilhas que valem lembrar

- **Pinos I2C do Grove: SDA=2, SCL=1.** Trocar SDA/SCL por engano faz o scanner
  achar "nada". `Wire.begin(2, 1)` — a ordem e `(sda, scl)`.
- **Endereco do BM8563 e fixo (0x51).** Nao ha jumper de endereco.
- **`requestFrom` com *repeated start*.** Posicionamos o ponteiro com
  `endTransmission(false)` (sem STOP) antes de ler os 7 bytes.
- **`tm_mon` e 0..11.** Somamos/subtraimos 1 ao converter de/para o registrador
  de mes (que e 1..12).
- **Sem lib externa.** Tudo via `Wire`; nada de bibliotecas de RTC no
  `platformio.ini`.

## Integracao

Nada a editar aqui — a INTEGRACAO inclui `apps/ferramentas/ferramentas.h` no
`app_registry.cpp` e monta uma **nova categoria "Ferramentas"** a partir de:

```cpp
apps::ferramentas::FERRAMENTAS_APPS       // array
apps::ferramentas::FERRAMENTAS_APPS_COUNT // contagem
```

Bibliotecas extras: **nenhuma** (a `Wire` ja vem no core).
