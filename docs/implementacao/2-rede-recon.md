<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Implementação — Rede / Reconhecimento (RX passivo)

Este documento explica, com objetivo **didático**, como funciona o módulo de
reconhecimento de rede do Noir OS. São seis ferramentas que apenas **observam**
a rede (não injetam frames 802.11 nem apagam nada) e por isso entram no menu
com `danger=false`.

Arquivos:

- `src/apps/rede/rede_recon.h` — contrato de exportação (o array de apps).
- `src/apps/rede/rede_recon.cpp` — implementação das seis ferramentas.

> Antes de tudo, leia `docs/legal-etica.md`. Mesmo ferramentas "passivas" como
> port scan podem ter peso legal fora de redes próprias/autorizadas.

---

## 1. O modelo de "app" do Noir

Um app é só uma função `void()` que **bloqueia** rodando um laço com os widgets
do núcleo e **retorna** quando o usuário sai. O menu principal recebe um array
de `noir::AppEntry`:

```cpp
struct AppEntry { const char* name; const char* hint; AppRun run; bool danger; };
```

As funções `run()` ficam num **namespace anônimo** (arquivo-local), então não
poluem o linker. Só o array `RECON_APPS[]` é exportado, via o header do módulo.
Esse é exatamente o padrão do app de referência `src/core/wifi_config.cpp`.

Todo desenho passa pelos widgets prontos (`ui::listView`, `ui::messageBox`,
`ui::confirm`, `ui::textInput`, `ui::progress`, `ui::banner`, `ui::redStripe`),
que já aplicam o tema Noir e leem o teclado. **Reaproveitar** esses widgets
evita reinventar menu/entrada de texto.

### Contrato de exportação

```cpp
// rede_recon.h
namespace apps { namespace rede {
  extern const noir::AppEntry RECON_APPS[];
  extern const int            RECON_APPS_COUNT;
} }
```

O array é definido no fim do `.cpp`. Como nenhuma ferramenta faz TX ofensivo,
todas são `danger=false` — não há apps de perigo neste módulo.

---

## 2. As seis ferramentas

### 2.1 Scan WiFi

Usa `noir::wifi::scan()`, que preenche um `std::vector<wifi::Network>`
(`ssid`, `rssi`, `channel`, `open`, `bssid`). Ordenamos por sinal (mais forte
primeiro) com `std::sort` e montamos os rótulos da lista com um `*` para as
redes protegidas. Ao selecionar, uma `ui::messageBox` mostra o detalhe
(BSSID, canal, RSSI e se é aberta). O laço volta à lista até o usuário dar `` ` ``.

Armadilha: SSIDs ocultos vêm vazios — tratamos como `<oculto>`.

### 2.2 Scan BLE (NimBLE-Arduino)

NimBLE é uma stack Bluetooth LE bem mais leve que a do core Arduino — ideal
para um chip sem PSRAM. O fluxo:

```cpp
NimBLEDevice::init("");                       // "" = não anunciamos nada
NimBLEScan* scan = NimBLEDevice::getScan();
scan->setActiveScan(true);                    // pede scan response (mais nomes)
scan->setInterval(45); scan->setWindow(15);   // unidades de 0.625 ms, window<=interval
NimBLEScanResults r = scan->getResults(5000, false);  // bloqueia ~5 s
for (int i = 0; i < r.getCount(); i++) {
    const NimBLEAdvertisedDevice* d = r.getDevice(i);
    d->getName();                 // std::string (pode vir vazio)
    d->getAddress().toString();   // std::string -> MAC
    d->getRSSI();                 // int (dBm)
}
scan->clearResults();
NimBLEDevice::deinit(true);        // libera o controlador BT (coexistência c/ WiFi)
```

Pontos importantes:

- **Scan ativo** faz o dispositivo pedir o *scan response*, que normalmente
  traz o nome — scan passivo veria muito mais `<sem nome>`.
- Copiamos nome/MAC/RSSI para `String` nossas **antes** de `deinit()`, senão os
  ponteiros do resultado seriam liberados.
- Chamamos `deinit(true)` no fim para devolver a RAM do controlador BT e não
  brigar com o WiFi por memória/rádio.
- API alvo: **NimBLE-Arduino 2.x** (`getDevice()` devolve `const
  NimBLEAdvertisedDevice*`). Em 1.x a assinatura difere — pinar a major ajuda.

### 2.3 DNS lookup

`WiFi.hostByName(host, IPAddress&)` retorna `1` no sucesso e preenche o IP.
Garantimos `wifi::ensure()` no início. O laço volta ao `textInput` para
resolver outro host; `` ` `` sai.

### 2.4 Ping (ESP32Ping)

A lib `ESP32Ping` expõe `Ping.ping(host, count)` e as médias globais
(`minTime()`, `averageTime()`, `maxTime()`), mas **não** dá diretamente a
contagem de pacotes recebidos. Para obter **perda %** de forma confiável,
fazemos N pings de **1 pacote** num laço e contamos nós mesmos:

```cpp
for (int i = 0; i < N; i++) {
    if (Ping.ping(host, 1)) {         // true = respondeu
        recebidos++;
        float t = Ping.averageTime(); // com count=1, é o RTT desse ping
        // acumula min/soma/max
    }
}
int perda = (N - recebidos) * 100 / N;
```

Assim min/avg/max saem dos pings que chegaram e a perda é exata. Se nada
responder, avisamos que o host pode estar bloqueando ICMP.

### 2.5 Port scanner

Testa uma lista de portas TCP comuns (`21,22,23,80,443,3306,8080,8096,9000,
32400`) com `WiFiClient::connect(ip, porta, timeoutMs)` e **timeout curto**
(400 ms) para não arrastar. É **sequencial** e mostra `ui::progress` a cada
porta, então a UI não parece travada.

Decisões:

- **Confirmação obrigatória** antes (`ui::confirm(..., danger=true)`), com aviso
  de escopo — varrer portas de terceiros pode ser ilegal. O `danger=true` aqui
  só pinta o botão SIM de vermelho; no menu o app continua `danger=false`.
- Resolvemos o host **uma vez** com `hostByName` e reusamos o `IPAddress` em
  cada `connect` (mais rápido que resolver porta a porta).
- As portas abertas viram um `ui::listView` só-leitura.

### 2.6 Speed test

Baixa um recurso conhecido e estima os Mbps. **Não** usamos `noir::net::get()`
aqui: ele carrega o corpo **inteiro** numa `String` (via
`HTTPClient::getString()`) e, como a URL é editável, apontar para um arquivo
grande estouraria o heap (~320 KB, sem PSRAM) ou truncaria em silêncio. Em vez
disso, lemos o stream em **blocos de ~2 KB** e apenas **somamos os bytes**,
descartando o conteúdo — o uso de RAM fica constante para qualquer tamanho:

```cpp
HTTPClient http;
http.begin(*client, url);      // client = WiFiClientSecure (valida cert) p/ https
http.GET();
WiFiClient* stream = http.getStreamPtr();
int contentLen = http.getSize();       // -1 se chunked/desconhecido
uint8_t buf[2048];
size_t bytes = 0;
uint32_t t0 = millis();                // cronômetro SÓ da transferência (pós-handshake)
for (;;) {
    size_t disp = stream->available();
    if (disp) {
        int lidos = stream->readBytes(buf, disp > sizeof(buf) ? sizeof(buf) : disp);
        if (lidos <= 0) break;
        bytes += lidos;
        if (contentLen > 0 && bytes >= (size_t)contentLen) break;
    } else if (!http.connected()) break;
}
uint32_t dt = millis() - t0;
float mbps = (float)bytes * 8.0f / (dt / 1000.0f) / 1e6f;
```

Duas decisões de **metodologia**:

- **`t0` começa depois de abrir o stream** (ou seja, após DNS + TCP + handshake
  TLS). Se cronometrássemos a partir do `GET`, o overhead de conexão inflaria o
  tempo e **subestimaria** os Mbps. Aqui medimos só a fase de transferência.
- **TLS validado.** O host padrão é público (`speed.cloudflare.com`), então
  usamos `WiFiClientSecure` **sem** `setInsecure()` — mesmo caminho seguro do
  núcleo `net::get(insecure=false)`. Não aceitamos cert inválido por padrão.

A URL fica em **config** (chave `spd_url`, ≤15 chars) e é editável pelo usuário
(persistimos com `config::setStr` quando muda). O padrão aponta para
`https://speed.cloudflare.com/__down?bytes=100000`, um endpoint que devolve
exatamente o número de bytes pedido.

> **Sem PSRAM — por que streaming?** Como a URL é editável, um usuário poderia
> apontar `spd_url` para um arquivo de dezenas de MB. Bufferizar isso na RAM
> (o que `net::get()` faria) mataria o processo por falta de heap. Lendo em
> blocos e descartando o conteúdo, o consumo é ~2 KB **independente** do tamanho
> baixado.

---

## 3. APIs do núcleo usadas

| API | Para quê |
|-----|----------|
| `noir::wifi::scan()` / `ensure()` / `isConnected()` | scan e garantir conexão |
| `noir::config::getStr/setStr` | persistir `spd_url` |
| `HTTPClient` + `WiFiClientSecure` (Arduino) | download do speed test por streaming |
| `ui::listView / messageBox / confirm / textInput / progress / banner / redStripe` | toda a UI |
| `WiFi.hostByName`, `WiFiClient::connect` (Arduino) | DNS e port scan |
| `NimBLEDevice / NimBLEScan` (NimBLE-Arduino) | scan BLE |
| `Ping.ping` (ESP32Ping) | ping ICMP |

## 4. Dependências (para a integração)

Estas libs **não** foram adicionadas ao `platformio.ini` por este módulo (a
integração faz isso). Estão no manifesto de retorno:

- `h2zero/NimBLE-Arduino` — scan BLE.
- `marian-craciunescu/ESP32Ping` — ping ICMP.

Sem `build_flags` extras: o `-I src` já existente resolve os includes do módulo
(`apps/rede/rede_recon.h`).

## 5. Armadilhas resumidas

- **BLE**: copie os dados antes de `deinit`; use scan ativo; API alvo 2.x.
- **Ping**: a lib não dá perda direta — conte 1 pacote por vez.
- **Port scan**: timeout curto + confirmação obrigatória.
- **Speed test**: leia o corpo por streaming (blocos de ~2 KB, some os bytes) em
  vez de `net::get()` — a RAM fica constante; cronometre só a transferência
  (t0 pós-handshake); valide o cert TLS (sem `setInsecure`) para o host público.
- **NVS**: a chave `spd_url` tem 6 chars (limite é 15). OK.
