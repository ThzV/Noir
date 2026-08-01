// ============================================================================
//  Noir OS  -  Modulo SERVIDOR  (implementacao)
//
//  Um "painel de homelab" de bolso. Tudo aqui e' HTTP + JSON sobre a internet
//  (via noir::net), para poder analisar o SERVIDOR PESSOAL de QUALQUER lugar
//  com WiFi. Nada de socket Docker cru nem lib especial: dominado o padrao
//  "GET/POST com token -> ArduinoJson com filtro -> desenhar Noir", os quatro
//  servicos (Portainer, AdGuard, Uptime Kuma) caem juntos.
//
//  ACESSO REMOTO SEGURO (leia docs/implementacao/4-servidor.md): NAO exponha
//  Portainer/AdGuard direto na internet. Use uma VPN (Tailscale/WireGuard),
//  um Cloudflare Tunnel ou um reverse-proxy (Caddy/Nginx) com HTTPS + auth. Por
//  padrao este modulo VALIDA o certificado TLS (config "srv_verify"=true =>
//  insecure=false), protegendo token/credenciais contra MITM. O toggle
//  "Verificar cert TLS" em Config servidor permite aceitar cert self-signed
//  (insecure=true) SO' em rede confiavel / VPN; pela internet, prefira cert valido.
//
//  Memoria: o Cardputer NAO tem PSRAM. As respostas de Docker/Portainer podem
//  ser enormes, entao usamos SEMPRE um filtro do ArduinoJson (Filter) para so'
//  materializar os campos que desenhamos.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#include "apps/servidor/servidor.h"

#include "core/net.h"
#include "core/config.h"
#include "core/wifi_service.h"
#include "ui/theme.h"
#include "ui/statusbar.h"
#include "ui/input.h"
#include "ui/widgets.h"

#include <ArduinoJson.h>
#include <M5Cardputer.h>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
//  Chaves de configuracao (NVS)  -  LEMBRETE: no maximo 15 caracteres cada!
// ----------------------------------------------------------------------------
constexpr const char* K_PT_URL = "pt_url";   // Portainer: base URL (ex. https://host:9443)
constexpr const char* K_PT_TOK = "pt_tok";   // Portainer: API token (header X-API-Key)
constexpr const char* K_AG_URL = "ag_url";   // AdGuard:   base URL (ex. http://host:3000)
constexpr const char* K_AG_USR = "ag_usr";   // AdGuard:   usuario (Basic Auth)
constexpr const char* K_AG_PWD = "ag_pwd";   // AdGuard:   senha   (Basic Auth)
constexpr const char* K_UK_URL = "uk_url";   // Uptime Kuma: URL COMPLETA da status page
                                             //   ex. http://host:3001/api/status-page/casa
constexpr const char* K_SRV_VERIFY = "srv_verify"; // valida cert TLS (true=seguro, padrao)

// ----------------------------------------------------------------------------
//  Helpers gerais
// ----------------------------------------------------------------------------

// Garante WiFi antes de qualquer request. Mostra erro e retorna false se falhar.
bool exigirWifi() {
    if (noir::wifi::ensure()) return true;
    ui::messageBox("Servidor", "Sem WiFi.\nConecte em Config > WiFi\ne tente de novo.");
    return false;
}

// TLS: por padrao VALIDAMOS o certificado (insecure=false), evitando MITM que
// roubaria token/credenciais no acesso remoto. O toggle "Verificar cert TLS"
// em Config servidor permite aceitar cert self-signed (insecure=true) quando
// se esta' numa rede confiavel/VPN. Ver docs/implementacao/4-servidor.md.
bool servidorInsecure() {
    return !noir::config::getBool(K_SRV_VERIFY, true);
}

// Remove uma barra final da URL (evita "http://x//api").
String semBarraFinal(String u) {
    while (u.endsWith("/")) u.remove(u.length() - 1);
    return u;
}

// Traduz um Resp de erro do net em mensagem amigavel; retorna true se OK.
bool checarResp(const noir::net::Resp& r, const char* oque) {
    if (r.ok()) return true;
    String msg;
    if (r.code == -2)      msg = "Sem WiFi.";
    else if (r.code == -3) msg = "Falha ao iniciar\nconexao (TLS).";
    else if (r.code < 0)   msg = "Erro de rede (" + String(r.code) + ").";
    else                   msg = "HTTP " + String(r.code) + ".";
    ui::messageBox("Servidor", String(oque) + " falhou.\n" + msg);
    return false;
}

// ----------------------------------------------------------------------------
//  Visualizador de texto rolavel (fonte mono/Font0)  -  usado pelos logs
// ----------------------------------------------------------------------------
//  Recebe as linhas ja' prontas; quebra cada uma na largura da tela e permite
//  rolar com cima/baixo. Sai com voltar (`) ou ENTER.
void visualizadorTexto(const char* titulo, const std::vector<String>& linhas) {
    // Quebra as linhas na largura da tela (Font0 ~6px/char => ~39 colunas).
    const int COLS = 39;
    std::vector<String> wrap;
    wrap.reserve(linhas.size());
    for (const String& ln : linhas) {
        if (ln.length() == 0) { wrap.push_back(""); continue; }
        for (int i = 0; i < (int)ln.length(); i += COLS)
            wrap.push_back(ln.substring(i, i + COLS));
    }
    if (wrap.empty()) wrap.push_back("(vazio)");

    // Area util abaixo da barra de status.
    const int y0        = noir::STATUSBAR_H + 3;
    const int lineH     = 9;                                  // altura por linha (Font0 + folga)
    const int visiveis  = (noir::SCREEN_H - y0 - 2) / lineH;  // quantas cabem
    int top = 0;
    const int maxTop = (int)wrap.size() > visiveis ? (int)wrap.size() - visiveis : 0;

    for (;;) {
        ui::clearNoir();
        ui::statusBar(titulo);
        M5Canvas& g = ui::gfx();
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_left);
        for (int i = 0; i < visiveis && (top + i) < (int)wrap.size(); ++i) {
            g.setTextColor(noir::BONE, noir::BLACK);
            g.drawString(wrap[top + i], 4, y0 + i * lineH);
        }
        // Indicador de rolagem (quando ha mais conteudo).
        g.setTextColor(noir::STEEL, noir::BLACK);
        if (maxTop > 0) {
            String pos = String(top + 1) + "/" + String((int)wrap.size());
            g.setTextDatum(top_right);
            g.drawString(pos, noir::SCREEN_W - 3, noir::STATUSBAR_H + 2);
        }
        ui::present();

        ui::KeyEvent e = ui::waitKey();
        if (e.key == ui::Key::Back || e.key == ui::Key::Enter) return;
        else if (e.key == ui::Key::Up)   { top -= 1; if (top < 0) top = 0; }
        else if (e.key == ui::Key::Down) { top += 1; if (top > maxTop) top = maxTop; }
        else if (e.key == ui::Key::Left) { top -= visiveis; if (top < 0) top = 0; }
        else if (e.key == ui::Key::Right){ top += visiveis; if (top > maxTop) top = maxTop; }
    }
}

// ============================================================================
//  PORTAINER
// ============================================================================

// Descobre o Id do primeiro "endpoint" (ambiente Docker) do Portainer.
//  GET {pt_url}/api/endpoints  ->  [ { "Id": 1, "Name": "local", ... }, ... ]
//  Retorna o Id ou -1 (com mensagem de erro ja' exibida se pediu).
int portainerEndpointId(const String& base, const String& tok, bool mostrarErro) {
    std::vector<noir::net::Header> h = {{"X-API-Key", tok}};
    noir::net::Resp r = noir::net::get(base + "/api/endpoints", h, servidorInsecure());
    if (!r.ok()) { if (mostrarErro) checarResp(r, "Listar endpoints"); return -1; }

    // Filtro: so' precisamos do Id de cada elemento do array.
    JsonDocument filter;
    filter[0]["Id"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, r.body, DeserializationOption::Filter(filter))) {
        if (mostrarErro) ui::messageBox("Portainer", "JSON de endpoints\ninvalido.");
        return -1;
    }
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) {
        if (mostrarErro) ui::messageBox("Portainer", "Nenhum endpoint\nDocker encontrado.");
        return -1;
    }
    return arr[0]["Id"] | -1;   // primeiro ambiente (homelab tipico tem 1)
}

// Um container, so' com o que desenhamos.
struct Container {
    String id;     // hash (curto ja' basta para as rotas)
    String nome;   // Names[0] sem a barra inicial
    String imagem;
    String estado; // "running", "exited", ...
};

// Busca os containers (todos, inclusive parados) do endpoint informado.
//  GET {base}/api/endpoints/{id}/docker/containers/json?all=1
bool portainerContainers(const String& base, const String& tok, int endpoint,
                         std::vector<Container>& out) {
    std::vector<noir::net::Header> h = {{"X-API-Key", tok}};
    String url = base + "/api/endpoints/" + String(endpoint) +
                 "/docker/containers/json?all=1";
    noir::net::Resp r = noir::net::get(url, h, servidorInsecure());
    if (!checarResp(r, "Listar containers")) return false;

    // Filtro por elemento do array: manter apenas 4 campos (economiza MUITA RAM,
    // pois a resposta bruta do Docker traz redes, mounts, labels, etc.).
    JsonDocument filter;
    filter[0]["Id"]    = true;
    filter[0]["Names"] = true;
    filter[0]["Image"] = true;
    filter[0]["State"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, r.body, DeserializationOption::Filter(filter))) {
        ui::messageBox("Portainer", "JSON de containers\ninvalido.");
        return false;
    }
    for (JsonObject c : doc.as<JsonArray>()) {
        Container cc;
        cc.id     = String((const char*)(c["Id"] | ""));
        cc.imagem = String((const char*)(c["Image"] | "?"));
        cc.estado = String((const char*)(c["State"] | "?"));
        const char* nm = c["Names"][0] | "";
        cc.nome = String(nm);
        if (cc.nome.startsWith("/")) cc.nome.remove(0, 1);   // Docker prefixa "/"
        if (cc.nome.length() == 0) cc.nome = cc.id.substring(0, 12);
        out.push_back(cc);
    }
    return true;
}

// Fluxo comum: valida config, resolve endpoint e lista containers.
// Retorna false (com mensagem) se algo faltar. Preenche base/tok/endpoint/out.
bool prepararContainers(String& base, String& tok, int& endpoint,
                        std::vector<Container>& out) {
    base = semBarraFinal(noir::config::getStr(K_PT_URL));
    tok  = noir::config::getStr(K_PT_TOK);
    if (base.length() == 0 || tok.length() == 0) {
        ui::messageBox("Portainer", "Configure a URL e o\ntoken em 'Config servidor'.");
        return false;
    }
    if (!exigirWifi()) return false;

    ui::progress("Portainer", "Resolvendo endpoint...", 30);
    endpoint = portainerEndpointId(base, tok, true);
    if (endpoint < 0) return false;

    ui::progress("Portainer", "Listando containers...", 70);
    if (!portainerContainers(base, tok, endpoint, out)) return false;
    if (out.empty()) { ui::messageBox("Portainer", "Nenhum container."); return false; }
    return true;
}

// Monta os rotulos "nome  [estado]" para a listView.
std::vector<String> rotulosContainers(const std::vector<Container>& cs) {
    std::vector<String> items;
    items.reserve(cs.size());
    for (const Container& c : cs) {
        String tag = c.estado;
        tag.toUpperCase();
        items.push_back(c.nome + "  [" + tag + "]");
    }
    return items;
}

// --- App 1: listar containers (leitura) ------------------------------------
void appContainers() {
    String base, tok; int endpoint;
    std::vector<Container> cs;
    if (!prepararContainers(base, tok, endpoint, cs)) return;

    int sel = 0;
    for (;;) {
        sel = ui::listView("Containers", rotulosContainers(cs), sel);
        if (sel < 0) return;
        const Container& c = cs[sel];
        // Detalhe do container escolhido.
        ui::messageBox("Container",
            c.nome + "\nEstado: " + c.estado + "\nImagem:\n" + c.imagem);
    }
}

// --- App 2: ver logs (leitura) ---------------------------------------------
void appLogs() {
    String base, tok; int endpoint;
    std::vector<Container> cs;
    if (!prepararContainers(base, tok, endpoint, cs)) return;

    int sel = ui::listView("Logs: escolha", rotulosContainers(cs));
    if (sel < 0) return;
    const Container& c = cs[sel];

    if (!exigirWifi()) return;
    ui::progress("Logs", "Baixando (tail 50)...", 50);

    // tail pequeno: a tela e' minuscula e a RAM tambem. stdout+stderr.
    std::vector<noir::net::Header> h = {{"X-API-Key", tok}};
    String url = base + "/api/endpoints/" + String(endpoint) +
                 "/docker/containers/" + c.id +
                 "/logs?stdout=1&stderr=1&tail=50";
    noir::net::Resp r = noir::net::get(url, h, servidorInsecure());
    if (!checarResp(r, "Baixar logs")) return;

    // A Docker Engine multiplexa stdout/stderr com um cabecalho binario de 8
    // bytes por quadro. Sem parsear o protocolo, fazemos limpeza best-effort:
    // trocamos caracteres de controle (exceto \n) por espaco e quebramos em
    // linhas. Detalhes/armadilhas no doc de implementacao.
    std::vector<String> linhas;
    String atual;
    for (size_t i = 0; i < r.body.length(); ++i) {
        char ch = r.body[i];
        if (ch == '\n') { linhas.push_back(atual); atual = ""; }
        else if (ch == '\r') { /* ignora */ }
        else if ((uint8_t)ch < 0x20 || (uint8_t)ch == 0x7F) { /* controle/header: descarta */ }
        else atual += ch;
    }
    if (atual.length()) linhas.push_back(atual);
    if (linhas.empty()) linhas.push_back("(sem saida de log)");

    String titulo = "Log: " + c.nome;
    visualizadorTexto(titulo.c_str(), linhas);
}

// --- App (PERIGO): reiniciar servico ---------------------------------------
//  Acao REAL no servidor: sempre confirmar. setTxActive NAO se aplica (nao ha'
//  transmissao de radio), mas o app e' marcado danger=true por ser destrutivo.
void appReiniciar() {
    String base, tok; int endpoint;
    std::vector<Container> cs;
    if (!prepararContainers(base, tok, endpoint, cs)) return;

    // Marca todos como "perigo" na lista (dangerFrom=0 => tudo vermelho).
    int sel = ui::listView("Reiniciar", rotulosContainers(cs), 0, 0);
    if (sel < 0) return;
    const Container& c = cs[sel];

    if (!ui::confirm("Reiniciar", "Reiniciar o container\n'" + c.nome + "' ?", true))
        return;

    if (!exigirWifi()) return;
    ui::progress("Reiniciar", c.nome, 50);

    std::vector<noir::net::Header> h = {{"X-API-Key", tok}};
    String url = base + "/api/endpoints/" + String(endpoint) +
                 "/docker/containers/" + c.id + "/restart";
    noir::net::Resp r = noir::net::post(url, "", "application/json", h, servidorInsecure());

    // O Docker responde 204 (No Content) no restart bem-sucedido.
    if (r.code == 204 || r.ok())
        ui::messageBox("Reiniciar", c.nome + "\nreiniciado.");
    else
        checarResp(r, "Reiniciar");
}

// ============================================================================
//  ADGUARD HOME
// ============================================================================

// Le base/usuario/senha da config; retorna false (com aviso) se faltar.
bool prepararAdGuard(String& base, noir::net::Header& auth) {
    base = semBarraFinal(noir::config::getStr(K_AG_URL));
    String usr = noir::config::getStr(K_AG_USR);
    String pwd = noir::config::getStr(K_AG_PWD);
    if (base.length() == 0 || usr.length() == 0) {
        ui::messageBox("AdGuard", "Configure URL/usuario/\nsenha em 'Config servidor'.");
        return false;
    }
    auth = noir::net::basicAuth(usr, pwd);   // Authorization: Basic base64(usr:pwd)
    return exigirWifi();
}

// --- App 3: estatisticas do AdGuard (leitura) ------------------------------
void appAdGuard() {
    String base; noir::net::Header auth;
    if (!prepararAdGuard(base, auth)) return;

    ui::progress("AdGuard", "Buscando stats...", 50);

    // /control/stats: totais do dia. Filtramos so' os dois numeros que usamos.
    noir::net::Resp rs = noir::net::get(base + "/control/stats", {auth}, servidorInsecure());
    if (!checarResp(rs, "Stats AdGuard")) return;

    JsonDocument fStats;
    fStats["num_dns_queries"]           = true;
    fStats["num_blocked_filtering"]     = true;
    fStats["num_replaced_safebrowsing"] = true;
    fStats["num_replaced_safesearch"]   = true;
    fStats["num_replaced_parental"]     = true;
    JsonDocument stats;
    if (deserializeJson(stats, rs.body, DeserializationOption::Filter(fStats))) {
        ui::messageBox("AdGuard", "JSON de stats\ninvalido.");
        return;
    }
    long queries  = stats["num_dns_queries"] | 0L;
    // "Bloqueadas" no dashboard = filtragem + reescritas (safebrowsing/parental/
    // safesearch); somamos tudo para casar com o painel do AdGuard.
    long blocked  = (stats["num_blocked_filtering"]     | 0L)
                  + (stats["num_replaced_safebrowsing"] | 0L)
                  + (stats["num_replaced_safesearch"]   | 0L)
                  + (stats["num_replaced_parental"]     | 0L);
    // blocked*100 pode passar de 2^31 em contadores grandes => calcular em 64 bits.
    int  pct      = queries > 0 ? (int)(((int64_t)blocked * 100) / queries) : 0;

    // /control/status: saber se a protecao esta' ligada.
    JsonDocument fStatus;
    fStatus["protection_enabled"] = true;
    noir::net::Resp rp = noir::net::get(base + "/control/status", {auth}, servidorInsecure());
    bool protecao = false;
    if (rp.ok()) {
        JsonDocument st;
        if (!deserializeJson(st, rp.body, DeserializationOption::Filter(fStatus)))
            protecao = st["protection_enabled"] | false;
    }

    ui::messageBox("AdGuard",
        String("Queries: ") + String(queries) + "\n" +
        "Bloqueadas: " + String(blocked) + "\n" +
        "Taxa: " + String(pct) + "%\n" +
        "Protecao: " + (protecao ? "LIGADA" : "desligada"));
}

// --- App (PERIGO): ligar/desligar protecao ---------------------------------
//  POST /control/protection {"enabled":bool}. Acao real => confirmar. danger.
void appToggleProtecao() {
    String base; noir::net::Header auth;
    if (!prepararAdGuard(base, auth)) return;

    // Le o estado atual para saber qual acao oferecer.
    ui::progress("Protecao", "Lendo estado...", 40);
    JsonDocument fStatus;
    fStatus["protection_enabled"] = true;
    noir::net::Resp rp = noir::net::get(base + "/control/status", {auth}, servidorInsecure());
    if (!checarResp(rp, "Status AdGuard")) return;
    JsonDocument st;
    if (deserializeJson(st, rp.body, DeserializationOption::Filter(fStatus))) {
        ui::messageBox("AdGuard", "JSON de status\ninvalido.");
        return;
    }
    bool atual  = st["protection_enabled"] | false;
    bool alvo   = !atual;

    String pergunta = atual ? "Protecao esta' LIGADA.\nDesligar filtragem DNS?"
                            : "Protecao esta' desligada.\nLigar filtragem DNS?";
    if (!ui::confirm("Protecao", pergunta, true)) return;

    ui::progress("Protecao", alvo ? "Ligando..." : "Desligando...", 70);
    String body = String("{\"enabled\":") + (alvo ? "true" : "false") + "}";
    noir::net::Resp r = noir::net::post(base + "/control/protection", body,
                                        "application/json", {auth}, servidorInsecure());
    if (r.ok() || r.code == 200)
        ui::messageBox("Protecao", String("Protecao agora: ") +
                       (alvo ? "LIGADA" : "desligada"));
    else
        checarResp(r, "Alterar protecao");
}

// ============================================================================
//  UPTIME KUMA  (status page publica)
// ============================================================================
//  Dois endpoints da mesma status page:
//    {uk_url}                         -> nomes/ids dos monitores (publicGroupList)
//    .../heartbeat/<slug>             -> ultimo batimento (status) de cada monitor
//  Derivamos a URL de heartbeat inserindo "/heartbeat" apos "/api/status-page".

// --- App 4: monitores up/down (leitura) ------------------------------------
void appUptimeKuma() {
    String url = semBarraFinal(noir::config::getStr(K_UK_URL));
    if (url.length() == 0) {
        ui::messageBox("Uptime Kuma",
            "Configure a URL da status\npage em 'Config servidor'.\n"
            "Ex: http://host:3001/\napi/status-page/casa");
        return;
    }
    if (!exigirWifi()) return;

    ui::progress("Uptime Kuma", "Buscando monitores...", 40);

    // 1) Lista de monitores (id + nome), agrupados em publicGroupList.
    JsonDocument fPage;
    fPage["publicGroupList"][0]["monitorList"][0]["id"]   = true;
    fPage["publicGroupList"][0]["monitorList"][0]["name"] = true;
    noir::net::Resp rp = noir::net::get(url, {}, servidorInsecure());
    if (!checarResp(rp, "Status page")) return;
    JsonDocument page;
    if (deserializeJson(page, rp.body, DeserializationOption::Filter(fPage))) {
        ui::messageBox("Uptime Kuma", "JSON da status page\ninvalido.");
        return;
    }

    // Guarda (id, nome) na ordem em que aparecem.
    struct Mon { long id; String nome; int status; };
    std::vector<Mon> mons;
    for (JsonObject grp : page["publicGroupList"].as<JsonArray>())
        for (JsonObject m : grp["monitorList"].as<JsonArray>())
            mons.push_back({ (long)(m["id"] | 0), String((const char*)(m["name"] | "?")), -1 });

    if (mons.empty()) { ui::messageBox("Uptime Kuma", "Nenhum monitor na\nstatus page."); return; }

    // 2) Batimentos: .../heartbeat/<slug>. heartbeatList e' um objeto keyado
    //    pelo id do monitor; o ultimo item traz o status (1=up, 0=down,
    //    2=pending, 3=manutencao).
    static const char* SP = "/api/status-page/";
    const int SP_LEN = 17;   // strlen("/api/status-page/")
    String hbUrl = url;
    int pos = hbUrl.indexOf(SP);
    if (pos >= 0)
        hbUrl = hbUrl.substring(0, pos) + "/api/status-page/heartbeat/" +
                hbUrl.substring(pos + SP_LEN);
    ui::progress("Uptime Kuma", "Lendo status...", 75);
    noir::net::Resp rh = noir::net::get(hbUrl, {}, servidorInsecure());
    bool statusParcial = !rh.ok();   // sem batimentos => monitores ficam em cinza
    if (rh.ok()) {
        // Filtro OBRIGATORIO (sem PSRAM): heartbeatList e' um objeto keyado pelo
        // id do monitor (chaves DINAMICAS => curinga "*"); de cada batimento so'
        // guardamos "status". Sem isso, o array cru (msg/ping/time por beat)
        // poderia estourar o heap.
        JsonDocument filter;
        filter["heartbeatList"]["*"][0]["status"] = true;
        JsonDocument hb;
        if (deserializeJson(hb, rh.body, DeserializationOption::Filter(filter))) {
            statusParcial = true;   // falha ao parsear => segue sem status
        } else {
            JsonObject list = hb["heartbeatList"];
            for (Mon& mm : mons) {
                JsonArray beats = list[String(mm.id)];
                if (!beats.isNull() && beats.size() > 0)
                    mm.status = beats[beats.size() - 1]["status"] | -1;
            }
        }
    }
    if (statusParcial)
        ui::messageBox("Uptime Kuma",
            "Batimentos indisponiveis.\nStatus parcial: monitores\nsem cor definida (cinza).");

    // 3) Desenha a lista com bolinha: branca(up) / vermelha(down) / cinza(?).
    int top = 0;
    const int y0 = noir::STATUSBAR_H + 6;
    const int lineH = 16;
    const int visiveis = (noir::SCREEN_H - y0) / lineH;
    const int maxTop = (int)mons.size() > visiveis ? (int)mons.size() - visiveis : 0;
    for (;;) {
        ui::clearNoir();
        ui::statusBar("Uptime Kuma");
        M5Canvas& g = ui::gfx();
        g.setFont(&fonts::Font2);
        g.setTextDatum(middle_left);
        for (int i = 0; i < visiveis && (top + i) < (int)mons.size(); ++i) {
            const Mon& m = mons[top + i];
            int y = y0 + i * lineH + lineH / 2;
            uint16_t cor = m.status == 1 ? noir::WHITE
                         : m.status == 0 ? noir::BLOOD
                         : noir::STEEL;                    // desconhecido/pending
            g.fillCircle(10, y, 4, cor);
            g.setTextColor(noir::BONE, noir::BLACK);
            g.drawString(m.nome, 22, y);
        }
        ui::present();
        ui::KeyEvent e = ui::waitKey();
        if (e.key == ui::Key::Back || e.key == ui::Key::Enter) return;
        else if (e.key == ui::Key::Up)   { top -= 1; if (top < 0) top = 0; }
        else if (e.key == ui::Key::Down) { top += 1; if (top > maxTop) top = maxTop; }
    }
}

// ============================================================================
//  CONFIG SERVIDOR  (salva URLs/tokens na NVS)
// ============================================================================
//  Uma unica tela-menu que edita cada segredo com ui::textInput. Os campos de
//  senha/token usam mask=true. NADA de credencial no codigo versionado.
void appConfig() {
    for (;;) {
        // Mostra um resumo (so' indica se esta' preenchido, sem vazar segredo).
        auto marca = [](const char* k) {
            return noir::config::getStr(k).length() ? " *" : " -";
        };
        std::vector<String> itens = {
            String("Portainer URL") + marca(K_PT_URL),
            String("Portainer token") + marca(K_PT_TOK),
            String("AdGuard URL") + marca(K_AG_URL),
            String("AdGuard usuario") + marca(K_AG_USR),
            String("AdGuard senha") + marca(K_AG_PWD),
            String("Uptime Kuma URL") + marca(K_UK_URL),
            // Toggle TLS: ON = valida cert (seguro); OFF = aceita self-signed.
            String("Verificar cert TLS") +
                (noir::config::getBool(K_SRV_VERIFY, true) ? " [ON]" : " [OFF]"),
        };
        int sel = ui::listView("Config servidor", itens);
        if (sel < 0) return;

        bool ok = true;
        String val;
        switch (sel) {
            case 0:
                val = ui::textInput("Portainer URL", noir::config::getStr(K_PT_URL), false, &ok);
                if (ok) noir::config::setStr(K_PT_URL, val);
                break;
            case 1:
                val = ui::textInput("Portainer token", "", true, &ok);
                if (ok) noir::config::setStr(K_PT_TOK, val);
                break;
            case 2:
                val = ui::textInput("AdGuard URL", noir::config::getStr(K_AG_URL), false, &ok);
                if (ok) noir::config::setStr(K_AG_URL, val);
                break;
            case 3:
                val = ui::textInput("AdGuard usuario", noir::config::getStr(K_AG_USR), false, &ok);
                if (ok) noir::config::setStr(K_AG_USR, val);
                break;
            case 4:
                val = ui::textInput("AdGuard senha", "", true, &ok);
                if (ok) noir::config::setStr(K_AG_PWD, val);
                break;
            case 5:
                val = ui::textInput("Uptime Kuma URL", noir::config::getStr(K_UK_URL), false, &ok);
                if (ok) noir::config::setStr(K_UK_URL, val);
                break;
            case 6: {
                // Alterna a validacao do certificado TLS. Feedback NAO-vermelho.
                bool novo = !noir::config::getBool(K_SRV_VERIFY, true);
                noir::config::setBool(K_SRV_VERIFY, novo);
                ui::messageBox("Verificar cert TLS", novo
                    ? "Validacao LIGADA.\nCert TLS sera' verificado\n(recomendado)."
                    : "Validacao DESLIGADA.\nAceita cert self-signed.\nUse so' em rede/VPN\nconfiavel.");
                break;
            }
        }
    }
}

} // namespace (arquivo-local)

// ----------------------------------------------------------------------------
//  Exportacao do modulo (contrato do servidor.h).
//  Ordem: apps de LEITURA primeiro; apps de PERIGO (acao real) por ULTIMO,
//  com danger=true (acento vermelho no menu).
// ----------------------------------------------------------------------------
namespace apps {
namespace servidor {

const noir::AppEntry SERVIDOR_APPS[] = {
    {"Config servidor", "setup",     appConfig,         false},
    {"Containers",      "portainer", appContainers,     false},
    {"Ver logs",        "docker",    appLogs,           false},
    {"AdGuard",         "dns",       appAdGuard,        false},
    {"Uptime Kuma",     "status",    appUptimeKuma,     false},
    // ---- PERIGO (acoes reais no servidor) ----
    {"Reiniciar servico", "restart", appReiniciar,      true},
    {"Toggle protecao",   "adguard", appToggleProtecao, true},
};
const int SERVIDOR_APPS_COUNT = sizeof(SERVIDOR_APPS) / sizeof(SERVIDOR_APPS[0]);

} // namespace servidor
} // namespace apps
