# Legalidade e ética

> **Leitura obrigatória antes de usar qualquer módulo de rede ofensivo.** Este documento não é aconselhamento jurídico — é um guia de bom senso e responsabilidade.

As ferramentas de rede do Noir se dividem em dois mundos com **peso legal muito diferente**:

## RX passivo × TX ativo

| Categoria | Exemplos no Noir | Natureza |
|---|---|---|
| **RX passivo** (escutar) | Scanner WiFi, scanner BLE, sniffer (só captura), speed test na sua rede | Recebe transmissões públicas/broadcast. Baixo risco, mas ainda sujeito a leis de privacidade. |
| **TX ativo** (transmitir/interferir) | Deauth, beacon spam, evil portal, clone de AP, injeção de pacotes | **Emite** sinais que afetam outros dispositivos/redes. **Alto peso legal.** |

**A regra prática:** escutar broadcasts públicos é muito diferente de **transmitir** para atrapalhar ou enganar. O segundo caso quase sempre exige **autorização escrita, específica e por escopo**.

## O que a lei tende a dizer

Isto varia por país; consulte a legislação local. Em linhas gerais:

- **Acesso/interferência não autorizada é crime.** Ex.: EUA — *Computer Fraud and Abuse Act* (18 U.S.C. § 1030); Reino Unido — *Computer Misuse Act*; Brasil — Lei 12.737/2012 ("Lei Carolina Dieckmann") e o Marco Civil da Internet. Deauth/spam sem autorização já foram tratados como intenção de disromper.
- **Jamming/interferência intencional é proibido.** Órgãos reguladores de espectro (FCC nos EUA, **Anatel** no Brasil) proíbem interferência proposital. O dispositivo opera em faixas ISM (equivalente à Part 15/homologação), mas **inundar/jamar não é permitido**.
- **Privacidade de dados.** Capturar tráfego que contenha dados pessoais pode violar leis de proteção de dados (LGPD no Brasil, GDPR na UE), mesmo que "só escutando".
- **Captive/evil portal** coletando credenciais de terceiros pode configurar **fraude/interceptação**.

## Regras de ouro do projeto

1. **Só opere em redes/dispositivos que você possui** ou que tem **autorização escrita** para testar.
2. **TX ativo = consentimento explícito por ação.** Nada de "autorização genérica".
3. **Prefira ambiente isolado** (uma rede de teste, idealmente com isolamento de RF) para deauth/spam/injeção.
4. **Não colete dados de terceiros.** Se um sniffer capturar dados alheios sem escopo, você tem um problema.
5. **Documente o escopo** de qualquer teste autorizado (o quê, quando, quais alvos).

## Como o Noir ajuda você a ser responsável

- **Acento vermelho = TX ativo** (ver [design](04-design-noir.md)). Quando o vermelho pisca, o dispositivo está **transmitindo** — você sempre sabe.
- **Telas de confirmação** antes de iniciar qualquer ferramenta de TX, com aviso de escopo.
- **Separação clara** no menu Rede entre "Reconhecimento" (RX) e "Ferramentas" (TX).

## Nota sobre o Bruce
O Bruce declara explicitamente ser **"para fins de teste de segurança legais e autorizados"**. Ao reusar seu código (AGPL-3.0), você herda essa responsabilidade. Use o Noir como uma ferramenta de **aprendizado e pentest autorizado** — não para causar dano.

## Recursos
- 📕 **CFAA (EUA):** <https://www.law.cornell.edu/uscode/text/18/1030>
- 📕 **Lei 12.737/2012 (Brasil):** <http://www.planalto.gov.br/ccivil_03/_ato2011-2014/2012/lei/l12737.htm>
- 📕 **LGPD (Brasil):** <https://www.gov.br/anpd/pt-br>
- 📕 **Anatel — uso de espectro:** <https://www.gov.br/anatel/pt-br>
- 📕 **OWASP — testing guide (metodologia ética):** <https://owasp.org/www-project-web-security-testing-guide/>
