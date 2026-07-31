# 00 — Visão geral

## O que é o Noir

O **Noir** é um firmware para o **M5Stack Cardputer v1.1** que se comporta como um pequeno **sistema operacional de bolso**: uma tela *home* com relógio/clima/status, um *launcher* com menus e um conjunto de "apps" (módulos) que cobrem quatro grandes propósitos:

1. **Cyber-ferramentas de rede** — do reconhecimento passivo (scanners) às ferramentas ofensivas do Bruce.
2. **Painel do seu servidor/homelab** — Docker, Portainer, AdGuard, Uptime Kuma, logs.
3. **Cofre e utilitários de segurança** — senhas, TOTP, cofre criptografado, Base64, QR.
4. **Utilitários do dia a dia** — arquivos, notas, editor, calendário, pomodoro, conversor.

Tudo isso amarrado por uma **identidade visual Spider-Man Noir**: preto e branco de alto contraste, textura de filme granulado, tipografia art-déco e um único acento de cor usado com intenção.

## Filosofia

- **Aprender construindo.** Este projeto é tanto um produto quanto um curso prático de ESP32-S3. Cada módulo tem recursos de estudo.
- **Reaproveitar antes de reinventar.** O [Bruce](https://github.com/pr3y/Bruce) já resolve rede ofensiva; as libs M5 já resolvem tela/teclado/áudio. Nós montamos, integramos e vestimos de Noir.
- **Offline-first, servidor-opcional.** Os módulos de segurança e utilitários funcionam sem rede. Rede e servidor são camadas por cima.
- **Design é feature.** A estética Noir não é enfeite: o acento vermelho, por exemplo, sinaliza **transmissão ativa / perigo** (deauth, beacon spam).

## Escopo dos módulos

### 🏠 Home
Relógio grande (sincronizado por **NTP**, já que não há RTC), clima do dia (API OpenWeatherMap), **bateria** e **status de WiFi** na barra superior.

### 🌐 Rede
- **Reconhecimento (RX passivo):** scanner de WiFi, scanner de BLE, DNS lookup, ping, port scanner, speed test.
- **Ferramentas ofensivas (TX ativo — via Bruce):** sniffer de pacotes, evil portal, beacon spam, hotspot/AP, clone de WiFi/deauth.
- ⚠️ Ver **[legalidade e ética](legal-etica.md)** — TX ativo tem peso legal real.

### 🖥️ Servidor
Cliente HTTP/JSON para o seu homelab: listar **containers Docker**, atalhos para **Portainer**, estatísticas do **AdGuard Home**, status do **Uptime Kuma**, **reiniciar serviços** e **ver logs**.

### 🔐 Segurança
Gerador de senhas, **cofre criptografado** (AES-256 via mbedTLS + NVS), **TOTP** (compatível com Google Authenticator), codificador **Base64**, gerador de **QR Code**.

### 📁 Arquivos
Explorador do cartão **microSD**, editor de texto, notas rápidas e visualizador de imagens (JPG/PNG/BMP).

### ⏱️ Produtividade
Calendário, cronômetro, timer **pomodoro** e conversor de unidades.

## O que este repositório entrega HOJE

Este é o **primeiro passo** do projeto. Entregue agora:

- ✅ **Toda esta documentação** — o mapa completo de como construir cada parte.
- ✅ **Um esqueleto de firmware que compila e dá flash** — splash Noir + home + menu navegável (os módulos abrem telas "em construção").
- ✅ **Base de projeto pronta** — `platformio.ini`, partições de 8 MB, tema Noir, licença AGPL-3.0.

A implementação de cada módulo segue as **fases do [ROADMAP](../ROADMAP.md)**.

## Público e pré-requisitos

- Confortável com **C++** básico (o Arduino framework é C++).
- Noções de **redes** (IP, portas, DNS) ajudam nos módulos de rede.
- Nenhuma experiência prévia com ESP32 é obrigatória — a doc de [ambiente](02-ambiente-dev.md) e [arquitetura](03-arquitetura.md) te levam do zero.

## Próximo passo
➡️ Entenda o alvo: **[01 — Hardware do Cardputer](01-hardware.md)**.
