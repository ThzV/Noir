# Roadmap do Noir OS

A ordem sugerida para construir o Noir, do zero ao completo. Cada fase entrega algo **funcional e testável** no dispositivo. Não pule fases — cada uma prepara a próxima.

> Legenda de dificuldade: 🟢 tranquilo · 🟡 exige estudo · 🔴 desafiador

> ✅ **Estado atual (v0.1):** o núcleo e **todos os módulos** das Fases 1–6 já estão **implementados e compilando** (`pio run` → `firmware.bin`). As fases abaixo permanecem como **mapa de aprendizado** e checklist de **endurecimento/validação em hardware** — muitos itens já têm código, faltando testar no Cardputer real e polir (Fase 7). Os guias do código que já existe estão em [docs/implementacao/](docs/implementacao/README.md).

---

## ✅ Fase 0 — Fundações (ESTE COMMIT)
O ponto de partida já entregue neste repositório.
- [x] Documentação completa (`docs/`).
- [x] Projeto PlatformIO configurado (`platformio.ini`, partições 8 MB).
- [x] Licença AGPL-3.0, README, `.vscode`.
- [x] Esqueleto que **compila e dá flash**: splash Noir + home + menu navegável (módulos = "em construção").
- [ ] **Você:** instalar a extensão PlatformIO e fazer o **primeiro flash** (ver [docs/02](docs/02-ambiente-dev.md)).

**Marco:** o Cardputer liga, mostra a splash Noir e navega pelo menu com o teclado. 🎉

---

## Fase 1 — MVP: o OS "vivo" 🟢🟡
Transformar o esqueleto num sistema utilizável.
- [ ] **Toolkit de UI** polido: barra de status, listas, modais, grão/vinheta (`ui/theme`, `ui/menu`).
- [ ] **Config de WiFi** (scan → escolher → senha → salvar na NVS).
- [ ] **Relógio NTP** na home (ver [modulos/1-home](docs/modulos/1-home.md)).
- [ ] **Sistema de `Screen`/Router** consolidado.
- [ ] **Config geral** (brilho, fuso, tema) persistida em NVS.

**Marco:** conecta no WiFi, mostra a hora certa, navega entre telas com identidade Noir.

---

## Fase 2 — Utilitários offline 🟢
Módulos que não dependem de rede — vitórias rápidas para firmar o toolkit.
- [ ] **Produtividade:** cronômetro → conversor → pomodoro → calendário ([modulos/6](docs/modulos/6-produtividade.md)).
- [ ] **Segurança (parte fácil):** gerador de senhas → Base64 → QR Code ([modulos/4](docs/modulos/4-seguranca.md)).
- [ ] **Arquivos:** explorador → visualizador de imagens → notas ([modulos/5](docs/modulos/5-arquivos.md)).

**Marco:** o Noir já é útil no dia a dia mesmo sem servidor/rede ofensiva.

---

## Fase 3 — Segurança avançada 🟡🔴
- [ ] **TOTP** (2FA) — precisa do NTP da Fase 1.
- [ ] **Cofre criptografado** (AES-256 + KDF + NVS) — o módulo mais delicado.
- [ ] **Editor de texto** completo (evolução das notas).

**Marco:** cofre desbloqueável por senha-mestra guardando segredos e TOTP.

---

## Fase 4 — Rede: reconhecimento 🟢🟡
Só RX passivo — baixo risco, muito aprendizado.
- [ ] **Scanner WiFi** (o primeiro e mais simples).
- [ ] **Ping** e **DNS lookup**.
- [ ] **Scanner BLE** (NimBLE).
- [ ] **Port scanner** e **speed test**.
- [ ] Indicador de "TX ativo" (vermelho) preparado na barra de status.

**Marco:** um scanner de rede de bolso, com estética Noir.

---

## Fase 5 — Rede: ferramentas ofensivas 🔴
⚠️ **Leia [docs/legal-etica.md](docs/legal-etica.md) antes.** Reaproveitar o Bruce ([docs/05](docs/05-reaproveitar-bruce.md)).
- [ ] **Sniffer** de pacotes (PCAP no SD) — primeiro contato com modo promíscuo.
- [ ] **Hotspot/AP**.
- [ ] **Evil portal** (AP + DNS + web server + HTML do Bruce).
- [ ] **Beacon spam**.
- [ ] **Clone / deauth**.
- [ ] Confirmações obrigatórias + indicador vermelho de TX em todas.

**Marco:** suíte ofensiva integrada, com salvaguardas visuais e de confirmação.

---

## Fase 6 — Painel do servidor 🟡
- [ ] Tela de **config** (URLs + tokens na NVS).
- [ ] **AdGuard** stats → **Portainer** containers → **Uptime Kuma** status.
- [ ] **Ações** (restart) + **visualizador de logs**.

**Marco:** monitorar e operar o homelab pelo Cardputer.

---

## Fase 7 — Polimento Noir 🎨
- [ ] Fontes art-déco custom, sprites de ícones, splash animada.
- [ ] Transições (obturador/ink wipe), grão/vinheta refinados.
- [ ] Sons (obturador, beeps).
- [ ] **OTA** (opcional — rever partição para dois slots de app).
- [ ] Otimização de memória e bateria.

**Marco:** o Noir parece um produto, não um protótipo.

---

## Dicas de execução
- **Uma feature por branch**, PR pequeno, teste no hardware antes de mesclar.
- Cada módulo novo = uma pasta em `src/modules/<nome>/` com sua `Screen`.
- Cabeçalho **AGPL-3.0** em todo arquivo; créditos ao Bruce nos trechos reusados.
- Comece sempre pelos itens 🟢 de cada fase — momentum importa.
- Consulte os [recursos de aprendizado](docs/recursos-aprendizado.md) 🟢 ao entrar em cada módulo.
