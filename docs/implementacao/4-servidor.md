<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Implementação — Módulo 🖥️ Servidor

Este documento explica **como o código do módulo Servidor funciona**, as APIs do Noir OS que ele usa, as decisões de projeto e as armadilhas. É material de aprendizado: leia junto com `src/apps/servidor/servidor.cpp`.

O módulo é um "painel de homelab de bolso" pensado para **acesso remoto**: você analisa o seu servidor pessoal de qualquer lugar com WiFi. Tudo é **HTTP + JSON** sobre a internet, via `noir::net`.

---

## 1. A ideia central: um só padrão para quatro serviços

Portainer, AdGuard Home e Uptime Kuma parecem coisas diferentes, mas do ponto de vista do firmware são **a mesma coisa**:

```
noir::net::get/post  →  corpo (String) com JSON  →  ArduinoJson (com filtro)  →  desenhar na tela Noir
```

Não existe biblioteca mágica para cada serviço. Dominado esse fluxo uma vez, os quatro caem juntos. O que muda de um para o outro é só **a autenticação** e **os endpoints**:

| Serviço | Auth | Endpoints usados |
|---|---|---|
| Portainer | Header `X-API-Key: <token>` | `/api/endpoints`, `/api/endpoints/{id}/docker/containers/json`, `.../restart`, `.../logs` |
| AdGuard Home | Basic Auth (usuário/senha) | `/control/stats`, `/control/status`, `/control/protection` |
| Uptime Kuma | Nenhuma (status page pública) | `/api/status-page/<slug>`, `/api/status-page/heartbeat/<slug>` |

---

## 2. APIs do Noir OS usadas

### `core/net.h` — cliente HTTP
- `noir::net::get(url, headers, insecure, timeout)` e `post(url, body, contentType, headers, insecure, timeout)` devolvem um `Resp { int code; String body; bool ok(); }`.
- `code` é o status HTTP quando `> 0`; valores negativos são erros locais: `-2` sem WiFi, `-3` falha ao iniciar TLS. A função `checarResp()` traduz tudo isso em uma `messageBox` amigável.
- `noir::net::basicAuth(user, pass)` monta o header `Authorization: Basic base64(user:pass)` para o AdGuard.

### `core/config.h` — segredos na NVS
As URLs/tokens ficam na NVS (Preferences). **Nunca** no código versionado — o repositório é público (AGPL). Chaves têm **máximo de 15 caracteres**; por isso os nomes curtos:

```
pt_url  pt_tok            (Portainer)
ag_url  ag_usr  ag_pwd    (AdGuard)
uk_url                    (Uptime Kuma: URL COMPLETA da status page)
```

### `core/wifi_service.h`
`noir::wifi::ensure()` garante conexão antes de qualquer request (`exigirWifi()` embrulha isso com uma mensagem de erro).

### `ui/widgets.h` e `ui/theme.h`
Reaproveitamos os widgets prontos: `listView`, `messageBox`, `confirm`, `textInput`, `progress`. Só desenhamos "na mão" (em `ui::gfx()` após `ui::clearNoir()` + `ui::statusBar()`) em dois lugares que os widgets não cobrem: o **visualizador de logs rolável** e a **lista de monitores com bolinha** do Uptime Kuma.

---

## 3. ArduinoJson com **filtro** — por que é obrigatório aqui

O Cardputer **não tem PSRAM**. A resposta de `containers/json` do Docker é gigante (redes, mounts, labels, portas...). Materializar tudo estoura a RAM.

A solução é o `DeserializationOption::Filter`: você descreve **só os campos que vai desenhar**, e o ArduinoJson descarta o resto durante o parse.

```cpp
JsonDocument filter;
filter[0]["Id"]    = true;   // filter[0] = "aplique a cada elemento do array"
filter[0]["Names"] = true;
filter[0]["Image"] = true;
filter[0]["State"] = true;

JsonDocument doc;
deserializeJson(doc, resp.body, DeserializationOption::Filter(filter));
```

- `filter[0][...]` é a forma de filtrar **cada elemento de um array**.
- Para objetos simples (AdGuard) usamos `filter["num_dns_queries"] = true;`.
- Para estruturas aninhadas (Uptime Kuma) encadeamos: `filter["publicGroupList"][0]["monitorList"][0]["id"] = true;`.

Detalhe importante: `resp.body` **já** é uma `String` inteira na RAM (é o que o `net` devolve). O filtro não reduz o download, mas reduz drasticamente o **documento parseado**, que é onde a memória realmente aperta.

---

## 4. Passo a passo por app

### Config servidor
Um menu (`listView`) onde cada item abre um `textInput`. Campos de token/senha usam `mask=true`. O resumo mostra só `*` (preenchido) ou `-` (vazio) — **nunca** revela o segredo em tela.

### Containers (Portainer)
1. `portainerEndpointId()` faz `GET /api/endpoints` e pega o **Id do primeiro ambiente** (um homelab típico tem um só). É preciso porque todas as rotas de container passam por `/api/endpoints/{id}/docker/...`.
2. `portainerContainers()` faz `GET .../containers/json?all=1` (o `all=1` traz também os parados) e usa o filtro acima.
3. Uma `listView` mostra `nome [ESTADO]`; ao selecionar, uma `messageBox` mostra o detalhe. O nome vem de `Names[0]`, que o Docker prefixa com `/` — removemos a barra.

### Ver logs
Seleciona um container e faz `GET .../logs?stdout=1&stderr=1&tail=50`. O `tail=50` é deliberado: tela pequena, RAM curta.

**Armadilha do log do Docker:** a Engine API multiplexa stdout/stderr com um **cabeçalho binário de 8 bytes por quadro**. Sem parsear esse protocolo, fazemos uma limpeza *best-effort*: descartamos bytes de controle (`< 0x20`, exceto `\n`, e `0x7F`) e quebramos em linhas. Isso remove o cabeçalho na prática e evita lixo na tela. Se um dia os logs saírem "picotados", é aqui que se implementa o parser real dos frames.

O resultado vai para o `visualizadorTexto()`, um leitor rolável em `Font0` (mono) que quebra cada linha em ~39 colunas e rola com cima/baixo (e página com ←/→).

### AdGuard
`GET /control/stats` (queries e bloqueios) + `GET /control/status` (proteção ligada?). Calculamos a `%` de bloqueio e mostramos tudo numa `messageBox`. Basic Auth via `basicAuth()`.

### Uptime Kuma
São **dois** endpoints da mesma status page:
- a **URL configurada** (`uk_url`) devolve `publicGroupList[].monitorList[]` com `id` e `name`;
- a URL de **heartbeat** devolve o último batimento de cada monitor.

Derivamos a URL de heartbeat inserindo `/heartbeat/` logo após `/api/status-page/`. O `heartbeatList` é um objeto **keado pelo id do monitor** (chaves dinâmicas), então esse parse não usa filtro — pegamos o `status` do último batimento: `1` = up (bolinha branca), `0` = down (vermelha), outros = cinza. Desenhamos a lista à mão com `fillCircle` + nome.

---

## 5. Apps de PERIGO (ordem e confirmação)

Duas ações têm **efeito real** no servidor e ficam **por último** no array, com `danger=true` (acento vermelho no menu):

- **Reiniciar serviço** → `POST .../containers/{id}/restart` (o Docker responde `204`). A lista usa `dangerFrom=0` (tudo em vermelho) e há um `confirm(..., danger=true)` antes.
- **Toggle proteção (AdGuard)** → lê o estado atual, oferece a ação inversa e só age após `confirm`.

Sobre `noir::setTxActive()`: ele existe para ferramentas de **rádio** (deauth, beacon spam...). Aqui **não há transmissão de RF**, então não o chamamos. O `danger=true` é por a ação ser **destrutiva/real**, não por TX. Essa distinção está explicada no comentário do `appReiniciar`.

---

## 6. Acesso remoto **seguro** (leia isto!)

O objetivo é acessar o servidor **de qualquer lugar**. A tentação é abrir a porta do Portainer/AdGuard direto na internet. **Não faça isso.** Um painel de administração exposto é um alvo enorme.

Ordem de preferência:

1. **VPN — Tailscale ou WireGuard** (recomendado). Seu servidor entra numa rede privada criptografada; o Cardputer (via o WiFi de onde estiver) só alcança os serviços **dentro** dessa VPN. Nada fica exposto publicamente. Simples e o mais seguro.
2. **Cloudflare Tunnel.** Um túnel de saída expõe só o que você escolher, com HTTPS e uma camada de autenticação (Access) na frente. Não abre portas no seu roteador.
3. **Reverse-proxy (Caddy ou Nginx) com HTTPS + auth.** O proxy termina o TLS com **certificado válido** (Let's Encrypt) e exige autenticação antes de repassar ao serviço interno. Só use se souber configurar TLS e auth corretamente.

### Tokens e o parâmetro `insecure`
- **Tokens/senhas** ficam na NVS (config), nunca no Git. Trate o token do Portainer como uma senha de administrador — ele **é** isso.
- `noir::net` aceita `insecure=true` (padrão), que **não valida o certificado** TLS. Isso serve para homelab com cert *self-signed* **em rede confiável** (ex.: você já está dentro da VPN). **Pela internet aberta, `insecure=true` é perigoso**: um intermediário pode se passar pelo servidor. Com VPN/Cloudflare/reverse-proxy usando **certificado válido**, você não precisa de `insecure`.

Resumo: **exponha o mínimo, autentique tudo, e prefira um cert válido a desligar a validação.**

---

## 7. Armadilhas e decisões, em resumo

- **Sem PSRAM** → filtro do ArduinoJson em todo parse grande; `tail=50` nos logs; nada de buffers gigantes.
- **Chave NVS ≤ 15 chars** → nomes curtos (`pt_url`, `ag_pwd`...).
- **Log multiplexado do Docker** → limpeza de bytes de controle (best-effort).
- **Endpoint do Portainer** → resolvido dinamicamente (primeiro ambiente) para não gastar mais uma chave de config.
- **Uptime Kuma sem API rica** → usamos a status page pública + heartbeat; `heartbeatList` tem chaves dinâmicas, então esse parse não é filtrável.
- **Segredos fora da tela** → o menu de config mostra só `*`/`-`, e senhas usam `mask=true`.
- **Ações reais por último** com `danger=true` e `confirm()`.

## 8. Bibliotecas / build
- `bblanchon/ArduinoJson @ ^7.0.0` — já presente no `platformio.ini` do projeto. Nenhuma `build_flag` adicional é necessária.
