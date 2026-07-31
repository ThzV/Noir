<div align="center">

# NOIR

**Sistema operacional de bolso para o M5Stack Cardputer v1.1**
*Cyber-ferramentas, utilitários e painel de homelab — com estética Spider-Man Noir.*

`ESP32-S3` · `PlatformIO + Arduino` · `AGPL-3.0`

</div>

---

> ⚠️ **Projeto em construção.** Este repositório contém, hoje, a **documentação completa** de como construir o Noir e um **esqueleto de firmware que já compila e dá flash** (splash + home + menu navegável). Os módulos são implementados por fases — veja o **[ROADMAP](ROADMAP.md)**.

## O que é

O **Noir** transforma o Cardputer num pequeno "sistema operacional": uma *home* com relógio/clima/status e um *launcher* com apps organizados em seis áreas — tudo vestido de **preto e branco de alto contraste, textura de filme e um único acento de cor** (vermelho = transmissão ativa / perigo).

## Recursos planejados

| Área | Módulos |
|---|---|
| 🏠 **Home** | Relógio (NTP), clima, bateria, WiFi |
| 🌐 **Rede** | Scanner WiFi/BLE · DNS · ping · port scanner · speed test · sniffer · evil portal · beacon spam · hotspot · clone/deauth |
| 🖥️ **Servidor** | Docker/Portainer · AdGuard · Uptime Kuma · reiniciar serviços · logs |
| 🔐 **Segurança** | Gerador de senhas · cofre AES-256 · TOTP · Base64 · QR Code |
| 📁 **Arquivos** | Explorador SD · editor de texto · notas · visualizador de imagens |
| ⏱️ **Produtividade** | Calendário · cronômetro · pomodoro · conversor de unidades |

## Hardware alvo

**M5Stack Cardputer v1.1** — ESP32-S3FN8 · **8 MB flash, sem PSRAM** · tela ST7789V2 240×135 · teclado 56 teclas · microSD · IR · Grove I2C. **Sem RTC** (relógio via NTP). Detalhes em [docs/01-hardware.md](docs/01-hardware.md).

## Começando

Pré-requisito: **extensão PlatformIO IDE** no VS Code (ver [docs/02-ambiente-dev.md](docs/02-ambiente-dev.md)).

```bash
# compilar
pio run -e m5stack-cardputer

# compilar + gravar no Cardputer (conectado via USB-C)
pio run -e m5stack-cardputer -t upload

# monitor serial
pio device monitor -b 115200
```

No primeiro build, o PlatformIO baixa sozinho a plataforma (pioarduino) e a toolchain. O firmware sobe mostrando a splash **NOIR**, a home com relógio e um menu navegável pelo teclado (`;` cima, `.` baixo, `ENTER` seleciona, `` ` `` volta).

## Documentação

O guia completo de construção está em **[`docs/`](docs/README.md)**:

- [Visão geral](docs/00-visao-geral.md) · [Hardware](docs/01-hardware.md) · [Ambiente](docs/02-ambiente-dev.md) · [Arquitetura](docs/03-arquitetura.md)
- [Design Noir](docs/04-design-noir.md) · [Reaproveitar o Bruce](docs/05-reaproveitar-bruce.md)
- Módulos: [Home](docs/modulos/1-home.md) · [Rede](docs/modulos/2-rede.md) · [Servidor](docs/modulos/3-servidor.md) · [Segurança](docs/modulos/4-seguranca.md) · [Arquivos](docs/modulos/5-arquivos.md) · [Produtividade](docs/modulos/6-produtividade.md)
- [Roadmap](ROADMAP.md) · [Recursos de aprendizado](docs/recursos-aprendizado.md) · [Legalidade e ética](docs/legal-etica.md)

## Estrutura do repositório

```
Noir/
├── docs/               # documentação (o mapa de construção)
├── src/                # firmware: main + ui/ + screens/
├── include/            # headers compartilhados (tema Noir)
├── partitions/         # tabela de particoes 8 MB
├── platformio.ini      # configuracao de build
└── ROADMAP.md          # fases de implementacao
```

## Licença

**GNU AGPL-3.0** (ver [LICENSE](LICENSE)). O Noir reutiliza e se inspira no firmware **[Bruce](https://github.com/pr3y/Bruce)** (também AGPL-3.0) para os módulos de rede ofensivos; por isso o projeto inteiro é AGPL-3.0. As bibliotecas M5 (M5Cardputer/M5Unified/M5GFX) são MIT. Detalhes em [docs/05-reaproveitar-bruce.md](docs/05-reaproveitar-bruce.md).

## ⚠️ Aviso legal

Os módulos de rede incluem ferramentas ofensivas (deauth, beacon spam, evil portal, etc.). **Use apenas em redes e dispositivos que você possui ou tem autorização escrita para testar.** Transmissão não autorizada pode ser crime. Leia **[docs/legal-etica.md](docs/legal-etica.md)** antes de usar qualquer ferramenta de TX ativo.

---
<div align="center"><sub>Feito para aprender construindo. 🕷️</sub></div>
