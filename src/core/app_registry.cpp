// Noir OS  -  Registro de apps (montagem final do menu principal)
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Unico arquivo que conhece todos os modulos. Compoe as categorias do menu a
// partir dos arrays exportados por cada modulo (ver os respectivos headers).
#include "core/app_registry.h"
#include "core/config_apps.h"
#include "apps/home/home.h"
#include "apps/rede/rede_recon.h"
#include "apps/rede/rede_ofensivo.h"
#include "apps/servidor/servidor.h"
#include "apps/seguranca/seguranca.h"
#include "apps/arquivos/arquivos.h"
#include "apps/produtividade/produtividade.h"

namespace noir {

static void append(std::vector<AppEntry>& v, const AppEntry* arr, int n) {
    for (int i = 0; i < n; ++i) v.push_back(arr[i]);
}

const std::vector<CategoryRT>& registry() {
    static std::vector<CategoryRT> cats;
    if (!cats.empty()) return cats;

    // --- Rede: reconhecimento (RX) primeiro, ofensivo (TX/perigo) por ultimo ---
    {
        CategoryRT c{"Rede", "wifi/ble", {}};
        append(c.apps, apps::rede::RECON_APPS,    apps::rede::RECON_APPS_COUNT);
        append(c.apps, apps::rede::OFENSIVO_APPS, apps::rede::OFENSIVO_APPS_COUNT);
        cats.push_back(c);
    }
    // --- Servidor (homelab remoto) ---
    {
        CategoryRT c{"Servidor", "homelab", {}};
        append(c.apps, apps::servidor::SERVIDOR_APPS, apps::servidor::SERVIDOR_APPS_COUNT);
        cats.push_back(c);
    }
    // --- Seguranca (offline) ---
    {
        CategoryRT c{"Seguranca", "cofre", {}};
        append(c.apps, apps::seguranca::SEGURANCA_APPS, apps::seguranca::SEGURANCA_APPS_COUNT);
        cats.push_back(c);
    }
    // --- Arquivos (SD) ---
    {
        CategoryRT c{"Arquivos", "sd", {}};
        append(c.apps, apps::arquivos::ARQUIVOS_APPS, apps::arquivos::ARQUIVOS_APPS_COUNT);
        cats.push_back(c);
    }
    // --- Produtividade ---
    {
        CategoryRT c{"Produtividade", "tempo", {}};
        append(c.apps, apps::produtividade::PRODUTIVIDADE_APPS, apps::produtividade::PRODUTIVIDADE_APPS_COUNT);
        cats.push_back(c);
    }
    // --- Config: ajustes do nucleo + configuracao de Clima (Home) ---
    {
        CategoryRT c{"Config", "sys", {}};
        append(c.apps, apps::config::CONFIG_APPS,  apps::config::CONFIG_APPS_COUNT);
        append(c.apps, apps::home::HOME_CFG_APPS,  apps::home::HOME_CFG_APPS_COUNT);
        cats.push_back(c);
    }

    return cats;
}

AppRun dashboardApp() { return apps::home::runDashboard; }

} // namespace noir
