# Módulo 🖥️ Servidor

Um painel de bolso para o seu homelab: ver containers, status de serviços, logs e disparar ações — tudo via **REST/JSON** sobre a sua rede.

## Objetivo
- Listar **containers Docker** (nome, estado, imagem).
- Atalhos/ações no **Portainer**.
- Estatísticas do **AdGuard Home** (queries, bloqueios).
- Status do **Uptime Kuma** (monitores up/down).
- **Reiniciar serviços** e **ver logs**.

## Princípio comum
Todos esses serviços expõem **APIs HTTP com JSON**. O padrão é sempre o mesmo:
```
HTTPClient + WiFiClientSecure  →  GET/POST com token  →  ArduinoJson  →  desenhar na tela Noir
```
Não existe lib mágica para cada um — é HTTP + JSON. Domine esse padrão uma vez e todos os quatro serviços caem.

## APIs / bibliotecas
| Função | O que usar |
|---|---|
| HTTP(S) | `HTTPClient`, `WiFiClientSecure` (core ESP32) |
| JSON | `ArduinoJson` (com `JsonDocument` dimensionado + filtros) |
| Segredos | tokens/URLs na **NVS** (`Preferences`) — nunca no código versionado |

## Detalhe por serviço

### Docker (Engine API ou via Portainer)
- **Direto (Docker Engine API):** normalmente exposto em socket Unix; para TCP precisa habilitar (com TLS!). Endpoints: `GET /containers/json`, `POST /containers/{id}/restart`, `GET /containers/{id}/logs`.
- **Recomendado:** falar com o Docker **através do Portainer** (tem auth por token e HTTPS), evitando expor a Engine API crua.

### Portainer
- **Auth:** gere um **API token** no Portainer (`/api/auth` ou token de acesso).
- **Endpoints úteis:** `GET /api/endpoints` (ambientes), `GET /api/endpoints/{id}/docker/containers/json`, `POST .../containers/{id}/restart`, `GET .../containers/{id}/logs`.
- **Header:** `X-API-Key: <token>`.

### AdGuard Home
- **Auth:** Basic Auth (usuário/senha do AdGuard).
- **Endpoints:** `GET /control/status`, `GET /control/stats` (queries, blocked), `POST /control/protection` (ligar/desligar).
- **Noir:** cartão com total de queries e % bloqueado; toggle de proteção com confirmação.

### Uptime Kuma
- **Sem API REST oficial rica** — opções: **status page** (`/api/status-page/<slug>` retorna JSON dos monitores) ou **metrics** Prometheus (`/metrics` com API key). Também há push endpoints.
- **Noir:** lista de monitores com bolinha up (osso) / down (vermelho).

## Reiniciar serviços & logs
- **Reiniciar:** `POST` de restart (Portainer/Docker) — **sempre** com modal de confirmação (ação com efeito real!). Acento vermelho enquanto executa.
- **Logs:** `GET` de logs com `tail` pequeno (ex.: últimas 50 linhas) — a tela é pequena e a RAM é curta. Visualizador rolável, fonte mono, quebra de linha.

## Cuidados (importantes)
- **HTTPS + certificados:** `WiFiClientSecure` valida cert. Para homelab com cert self-signed, você pode usar `setInsecure()` **apenas em rede confiável** — entenda o risco.
- **Segredos fora do Git:** URLs e tokens vão para NVS via uma tela de configuração. O repo é público (AGPL) — nada de credencial hardcoded.
- **Memória:** use **filtros do ArduinoJson** (`deserializeJson` com filtro) para pegar só os campos que você desenha; respostas de Docker/Portainer podem ser enormes.
- **Timeouts e task:** requisições em task FreeRTOS com timeout; a UI mostra "carregando" sem travar.

## Dificuldade
🟡 **Média.** O conceito (HTTP+JSON) é simples; o trabalho está em auth de cada serviço, TLS em homelab e caber tudo na tela/RAM.

## Ordem recomendada
1. Tela de **config** (salvar base URL + token na NVS).
2. **AdGuard** stats (GET simples, Basic Auth) → aquece o padrão.
3. **Portainer** listar containers (token) → JSON maior, use filtro.
4. **Uptime Kuma** status page.
5. **Ações** (restart) + **logs** (com confirmação e visualizador).

## Recursos
- **HTTPClient:** <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/http.html>
- **ArduinoJson (+ filtros):** <https://arduinojson.org/v7/how-to/deserialize-a-very-large-document/>
- **Portainer API:** <https://docs.portainer.io/api/docs>
- **Docker Engine API:** <https://docs.docker.com/engine/api/>
- **AdGuard Home OpenAPI:** <https://github.com/AdguardTeam/AdGuardHome/tree/master/openapi>
- **Uptime Kuma (wiki/API):** <https://github.com/louislam/uptime-kuma/wiki>
