# Documentação do Noir OS

> Firmware ("sistema operacional") para o **M5Stack Cardputer v1.1**, com estética **Spider-Man Noir**, focado em cyber-ferramentas, utilitários e monitoramento do seu servidor.

Esta pasta é o seu **mapa de construção**. Cada documento explica **o quê**, **por quê**, **com quais bibliotecas** e **como aprender** a fazer cada parte — na ordem em que faz sentido estudar.

## Por onde começar

1. **[00 — Visão geral](00-visao-geral.md)** — o que é o Noir, escopo dos módulos e filosofia do projeto.
2. **[01 — Hardware do Cardputer](01-hardware.md)** — o que a placa tem (e o que **não** tem), pinout e limites.
3. **[02 — Ambiente de desenvolvimento](02-ambiente-dev.md)** — setup do PlatformIO + **análise de prontidão da sua máquina**.
4. **[03 — Arquitetura do OS](03-arquitetura.md)** — boot, launcher, sistema de menus, memória e partições.
5. **[04 — Design system Noir](04-design-noir.md)** — paleta, tipografia, textura de filme e componentes de UI.
6. **[05 — Reaproveitando o Bruce](05-reaproveitar-bruce.md)** — implicações da licença AGPL e mapa dos módulos reutilizáveis.

## Guias por módulo

Cada guia usa o mesmo template: **Objetivo → APIs/libs → Reaproveitamento do Bruce → Passos → Recursos → Dificuldade**.

| Módulo | Conteúdo | Guia |
|---|---|---|
| 🏠 **Home** | Relógio (NTP), clima, bateria, WiFi | [modulos/1-home.md](modulos/1-home.md) |
| 🌐 **Rede** | Scanner WiFi/BLE, DNS, ping, port scanner, speed test + sniffer, evil portal, beacon spam, hotspot, clone (Bruce) | [modulos/2-rede.md](modulos/2-rede.md) |
| 🖥️ **Servidor** | Docker/Portainer, AdGuard, Uptime Kuma, reiniciar serviços, ver logs | [modulos/3-servidor.md](modulos/3-servidor.md) |
| 🔐 **Segurança** | Gerador de senha, cofre criptografado, TOTP, Base64, QR Code | [modulos/4-seguranca.md](modulos/4-seguranca.md) |
| 📁 **Arquivos** | Explorador (SD), editor de texto, notas rápidas, visualizador de imagens | [modulos/5-arquivos.md](modulos/5-arquivos.md) |
| ⏱️ **Produtividade** | Calendário, cronômetro, pomodoro, conversor de unidades | [modulos/6-produtividade.md](modulos/6-produtividade.md) |

## Referências transversais

- **[Legalidade e ética](legal-etica.md)** — leitura **obrigatória** antes de mexer nos módulos de rede ofensivos.
- **[Recursos de aprendizado](recursos-aprendizado.md)** — todos os links de estudo reunidos por tema.
- **[../ROADMAP.md](../ROADMAP.md)** — a ordem sugerida para construir tudo, em fases.

## Decisões de projeto (resumo)

| Decisão | Escolha | Motivo |
|---|---|---|
| Build system | **PlatformIO + Arduino** | Reaproveita o Bruce e as libs M5 diretamente; toolchain automática. |
| Licença | **AGPL-3.0** | O Bruce é AGPL-3.0; reusar código dele obriga o projeto todo a ser AGPL-3.0. |
| Base de UI | **M5GFX / M5Unified / M5Cardputer** (MIT) | Libs oficiais, combinam com qualquer licença. |
| Relógio | **NTP via WiFi** | O Cardputer v1.1 **não tem RTC** que sobrevive a reboot. |

---
*Toda a documentação está em português. Os termos técnicos e nomes de API ficam no original (inglês) para casar com o código e a documentação oficial.*
