// ============================================================================
//  Noir OS  -  Modelo de "App"
//
//  Um app e' simplesmente uma funcao void() que RODA (bloqueante) ate' o
//  usuario sair e retornar ao menu. Esse modelo (estilo Bruce) e' o mais
//  simples de entender e de manter num dispositivo pequeno.
//
//  Cada modulo (rede, servidor, ...) exporta um array de AppEntry. O
//  app_registry monta as categorias do menu principal a partir deles.
//
//  SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
#pragma once

namespace noir {

// Assinatura de um app: roda ate' o usuario sair.
using AppRun = void (*)();

// Uma entrada no menu de uma categoria.
struct AppEntry {
    const char* name;   // rotulo no menu
    const char* hint;   // texto curto a' direita (pode ser "")
    AppRun      run;     // funcao que executa o app
    bool        danger;  // true => TX ativo / acao destrutiva (acento vermelho)
};

// Uma categoria do menu principal (Rede, Servidor, ...).
struct Category {
    const char*     name;
    const char*     hint;
    const AppEntry* apps;
    int             count;
};

// Estado global de "transmissao ativa": qualquer ferramenta de TX (deauth,
// beacon spam, evil portal, AP...) deve marcar isso enquanto transmite. A
// barra de status acende um alerta vermelho. Ver docs/legal-etica.md.
void setTxActive(bool active);
bool txActive();

} // namespace noir
