<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Módulo Rede / Ofensivo (TX ativo — PERIGO)

> **Leia antes:** [`docs/legal-etica.md`](../legal-etica.md). Estas ferramentas
> **transmitem** ou capturam tráfego. Emitir beacons falsos, deauth, evil portal
> ou capturar dados de terceiros **sem autorização escrita pode ser crime**
> (Lei 12.737/2012 e Marco Civil no Brasil; regras da Anatel sobre interferência).
> Use **somente** em redes/dispositivos seus ou com **autorização por escopo**.

Este doc explica como os 5 apps de `src/apps/rede/rede_ofensivo.cpp` funcionam,
as APIs usadas e as armadilhas.

## Onde ficam os arquivos

- `src/apps/rede/rede_ofensivo.h` — contrato de exportação (`OFENSIVO_APPS[]`).
- `src/apps/rede/rede_ofensivo.cpp` — implementação (funções `run()` em namespace anônimo).
- Integração: o `app_registry.cpp` monta a categoria **Rede** colocando os
  `RECON_APPS` (RX) primeiro e os `OFENSIVO_APPS` (TX/perigo) **depois**.

## Conceito central: o driver `esp_wifi` cru

O Arduino `WiFi.h` é uma casca sobre o driver **ESP-IDF** (`esp_wifi.h`). Para
sniffing e injeção precisamos falar direto com o driver:

| Função IDF | Para quê |
|---|---|
| `esp_wifi_set_promiscuous(true)` | liga o modo "escuta tudo" |
| `esp_wifi_set_promiscuous_rx_cb(cb)` | registra o callback que recebe cada quadro |
| `esp_wifi_set_channel(ch, ...)` | fixa o canal 2.4GHz (1..13) |
| `esp_wifi_80211_tx(iface, buf, len, false)` | **injeta um quadro 802.11 cru** |

### A armadilha do "sanity check"

Versões recentes do IDF filtram quadros crus em
`ieee80211_raw_frame_sanity_check()`, bloqueando deauth/beacon. O truque padrão
(herdado dos projetos de deauth para ESP32) é **redefinir** essa função com
linkagem fraca para sempre retornar `0`:

```cpp
extern "C" int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t) { return 0; }
```

Como o símbolo do IDF é `weak`, o nosso vence no link e o envio é liberado.

## O acento vermelho: `TxGuard` (RAII)

`core/app.h` expõe `noir::setTxActive(bool)`. Quando ligado, a barra de status
acende o vermelho — o usuário **sempre sabe** que o rádio está transmitindo.
Para não esquecer de desligar em nenhum caminho de saída (return, erro), usamos
um guard RAII:

```cpp
struct TxGuard {
    TxGuard()  { noir::setTxActive(true);  }
    ~TxGuard() { noir::setTxActive(false); } // apaga ao sair do escopo
};
```

Cada app de TX cria `TxGuard tx;` logo antes de começar a transmitir. O sniffer
**não** usa `TxGuard` porque é RX puro (não transmite) — mas continua marcado
`danger=true` no array por convenção do módulo.

## Confirmação de escopo dupla

Além da confirmação do launcher (porque `danger=true`), **cada app** chama
`confirmarEscopo()`, que usa `ui::confirm(..., danger=true)` com um lembrete
legal. Consentimento **por ação**, nunca genérico.

## App a app

### 1) Sniffer (RX passivo)

- Liga promiscuidade e registra `snifferCb()`. O callback roda na **task do
  WiFi**, então é minimalista: só incrementa contadores `volatile` por tipo
  (`WIFI_PKT_MGMT/CTRL/DATA/MISC`).
- **Channel hopping:** o loop principal troca de canal a cada 250ms (1→13) para
  varrer o espectro, já que a promiscuidade só escuta um canal por vez.
- **.pcap opcional no SD:** o callback **não** escreve no cartão (I/O em contexto
  de task de rádio é perigoso). Em vez disso empurra uma cópia truncada
  (`SNAP_LEN=256`) numa **fila FreeRTOS** (`xQueueSendFromISR`, não bloqueante —
  descarta se encher). O loop principal **drena** a fila e grava registros no
  formato **libpcap** clássico (cabeçalho global + registros), `LINKTYPE 105`
  (802.11 cru). O arquivo abre em Wireshark.
- Pinos do microSD do Cardputer v1.1 (SPI): `SCK=40 MISO=39 MOSI=14 CS=12`
  (confira em [`docs/01-hardware.md`](../01-hardware.md)).

### 2) Hotspot / AP

- `WiFi.softAP(ssid, pass)` faz o ESP virar um ponto de acesso. Como o AP
  **transmite beacons**, isso é TX ativo → `TxGuard`.
- SSID/senha configuráveis e persistidos (`ap_ssid`, `ap_pass` — chaves NVS ≤ 15
  chars). Senha com **< 8 chars** vira rede **aberta** (regra do WPA2).
- Mostra ao vivo `WiFi.softAPIP()` e `WiFi.softAPgetStationNum()` (nº de clientes).

### 3) Beacon Spam

- Monta um quadro **beacon** cru (cabeçalho MAC + timestamp + interval +
  capability + tags SSID/rates/DS-param) e injeta um por SSID de uma **lista
  curta e fixa** (`FAKE_SSIDS`), num canal escolhido.
- MAC de origem randomizado ("locally administered", primeiro byte `0x02`) por
  SSID, para cada rede fantasma parecer distinta.
- Precisa de `esp_wifi_set_promiscuous(true)` para habilitar o caminho de
  injeção crua, mesmo sem escutar.

### 4) Evil Portal

- Três engrenagens: `WiFi.softAP()` (AP **aberto**), `DNSServer` respondendo
  **todo** domínio (`"*"`) com o IP do AP (captive portal), e um `WebServer`
  servindo um HTML de login embutido.
- `onNotFound(portalRoot)` faz **qualquer URL** cair no portal — é o que dispara
  a detecção de captive portal do celular/PC.
- Credenciais do POST `/login` ficam **só em memória** (`std::vector`, limite 20)
  e são mostradas na tela; **nada é persistido** e a lista é apagada ao sair.
  Isso é para **teste autorizado** — coletar credenciais de terceiros configura
  fraude/interceptação.
- O loop bombeia `g_dns.processNextRequest()` **e** `g_web.handleClient()` a cada
  volta — esquecer um deles trava o portal.

### 5) Deauth (perigo máximo)

- `noir::wifi::scan()` lista os APs; o usuário escolhe o alvo (`listView` com
  `dangerFrom=0` → tudo vermelho). Convertemos o BSSID textual em 6 bytes e
  fixamos o canal do alvo.
- Injeta quadros **deauth** (frame control `0xC0`, reason `0x07`) com destino
  **broadcast** (expulsa todos os clientes do AP). É a ferramenta de maior peso
  legal — só em rede própria/autorizada.

## Sem PSRAM: cuidados de memória

- Frames 802.11 cabem em buffers na pilha (`≤ 128` bytes beacon, `26` deauth).
- A fila do sniffer é `16 × ~268B ≈ 4.3 KB` — modesta de propósito.
- O evil portal limita as credenciais a 20 entradas na RAM.

## Como isto entra no menu (integração)

O `rede_ofensivo.h` exporta:

```cpp
namespace apps { namespace rede {
  extern const noir::AppEntry OFENSIVO_APPS[];
  extern const int OFENSIVO_APPS_COUNT;
} }
```

O `app_registry.cpp` (fora deste módulo) inclui o header e concatena
`OFENSIVO_APPS` na categoria **Rede**, **após** os `RECON_APPS`, porque todos são
`danger=true`. Não editamos `platformio.ini`, `app_registry.cpp` nem `src/core`.

## Dependências de build

Nenhuma biblioteca nova: `esp_wifi`, `DNSServer`, `WebServer`, `SD` e `SPI` já
vêm com o core Arduino-ESP32. Nenhuma `build_flag` adicional.
