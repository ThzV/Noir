<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Modulo Seguranca (offline)

Ferramentas de seguranca que rodam **sem rede** no Cardputer. O unico app que
depende de algo externo e o TOTP, que precisa da **hora certa** (obtida antes
por NTP). Nenhum app aqui transmite RF nem apaga dados de terceiros, entao todos
sao `danger=false`.

Arquivos:

- `src/apps/seguranca/seguranca.h` — contrato de exportacao (`SEGURANCA_APPS[]`).
- `src/apps/seguranca/seguranca.cpp` — helpers de cripto + os 5 apps.

Toda a criptografia usa o **mbedtls** que ja vem no core do ESP32 (nao ha
dependencia PlatformIO nova). O QR e desenho nativo da M5GFX.

## Como um "app" funciona aqui

No Noir, um app e so uma `void()` bloqueante que roda ate o usuario sair. Cada
app desenha nos widgets prontos (`ui::listView`, `ui::textInput`, ...) ou, quando
precisa de tela propria, faz o ciclo:

```cpp
ui::clearNoir();          // fundo Noir
ui::statusBar("Titulo");  // barra superior
/* desenha em ui::gfx() ... */
ui::present();            // envia o canvas ao display
ui::KeyEvent e = ui::waitKey();
```

As funcoes `run()` ficam num **namespace anonimo** (file-local): so o array
`SEGURANCA_APPS[]`, no fim do `.cpp`, as enxerga.

---

## 1) Gerador de senhas

Menu de opcoes montado com `ui::listView`, refletindo o estado atual como
checkboxes `[x]/[ ]`. Ao selecionar um item, alterna a opcao e o loop redesenha.

**Entropia sem vies.** `esp_random()` devolve `uint32_t` do TRNG de hardware.
Se pegarmos um byte e fizermos `byte % N` (N = tamanho do alfabeto), os primeiros
valores do alfabeto ficam ligeiramente mais provaveis quando 256 nao e multiplo
de N — o **vies de modulo**. Corrigimos com *rejection sampling*: descartamos
qualquer byte `>= (256/N)*N` antes do `%`. Assim cada caractere e equiprovavel.

```cpp
const int limite = (256 / m) * m;
uint8_t b; fillRandom(&b, 1);
if (b >= limite) continue;   // descarta
out += alfabeto[b % m];
```

A senha e mostrada em **fonte fixa** (`Font0` ampliada 2x com `setTextSize(2)`,
a unica monospace da M5GFX) para nao confundir `O`/`0`, `l`/`1`. `ENTER` gera
outra, `Q` mostra a senha como QR, crase sai.

## 2) Base64

`mbedtls/base64.h`. Cuidado com os tamanhos de buffer:

- encode: saida = `4*ceil(n/3)` + 1 (terminador). `mbedtls_base64_encode` ja
  finaliza com `\0`, entao da para construir `String` direto do buffer.
- decode: a saida cabe em `s.length()` bytes (decodificado e ~3/4 da entrada).
  A funcao retorna `!= 0` se a string nao for Base64 valida — tratamos com
  `ui::redStripe`.

## 3) QR Code

Desenho **nativo** da M5GFX:

```cpp
ui::gfx().qrcode(texto, x, y, tamanho, /*versao=*/1, /*margin=*/true);
```

A M5GFX comeca na versao 1 e **sobe a versao automaticamente** ate o texto caber
(ate a 40). `margin=true` desenha a *quiet zone* (borda branca de 4 modulos,
exigida pela norma para o QR ser lido). A funcao ja pinta o fundo branco dentro
do quadrado `tamanho x tamanho`, entao basta centralizar o quadrado na area util
(abaixo da statusbar) e o contraste com o fundo Noir preto fica perfeito.

## 4) TOTP (RFC 6238)

Codigo de 6 digitos que muda a cada 30s, igual ao Google Authenticator.

Passo a passo:

1. **Segredo em Base32** (o que o servico mostra ao ativar 2FA). Decodificamos
   com `base32Decode` (RFC 4648: `A-Z` = 0..25, `2-7` = 26..31, ignora espacos
   e `-`, para no `=`). Cinco bits por caractere, acumulados em bytes.
2. **Contador de tempo** `T = epoch / 30`. Usamos `time(nullptr)`, que e a epoch
   **UTC** depois do NTP — por isso o TOTP independe do fuso. Se `!have()`, o app
   avisa que precisa sincronizar o relogio antes.
3. **HMAC-SHA1** do contador (8 bytes big-endian) com o segredo como chave, via
   `mbedtls_md_hmac(SHA1, ...)`.
4. **Truncation dinamica** (RFC 4226): `offset = hash[19] & 0x0f`; lemos 4 bytes
   a partir do offset, zeramos o bit mais alto e fazemos `% 1000000`.

A tela ao vivo mostra os digitos em `Font7` (display 7-segmentos, cara de token)
e uma barra do tempo restante que fica **vermelha** nos ultimos 5s. O loop usa
`ui::readKey()` (nao bloqueante) 20x a cada 50ms para redesenhar ~1x/s e ainda
responder rapido ao "voltar".

Os segredos TOTP **ficam no cofre cifrado** (entradas do tipo `'T'`), nunca em
texto plano na NVS.

## 5) Cofre criptografado

O app mais denso. Guarda uma lista de entradas `{kind, title, secret}` cifrada
com **AES-256-GCM**; a chave vem da senha-mestra via **PBKDF2**.

### Derivacao da chave (PBKDF2-HMAC-SHA256)

Nao guardamos a senha-mestra em lugar nenhum. Dela derivamos a chave AES:

```
key(32B) = PBKDF2-HMAC-SHA256(senha, salt, iteracoes=20000)
```

- O **salt** (16 bytes aleatorios) e publico e fica na NVS (`sec_salt`). Ele
  impede tabelas rainbow: a mesma senha gera chaves diferentes em cofres
  diferentes.
- As **20000 iteracoes** deixam o *brute force* caro. Como so precisamos de 32
  bytes (= tamanho do SHA-256), basta o "bloco 1" do PBKDF2. Reaproveitamos um
  unico contexto HMAC entre as iteracoes (`hmac_starts`/`update`/`finish`) para
  nao pagar `md_setup`/alloc a cada volta — importante num MCU.

### Cifra autenticada (AES-256-GCM)

GCM nao e so confidencialidade: produz uma **TAG** de 16 bytes que autentica o
dado. Guardamos o blob como `IV(12) || TAG(16) || CIPHERTEXT`, em Base64, na NVS
(`sec_vault`).

O truque elegante: **nao precisamos de hash-verificador da senha**. Se a
senha-mestra estiver errada, a chave derivada e diferente e
`mbedtls_gcm_auth_decrypt` **falha na TAG** — devolvemos "senha incorreta" sem
nunca ter revelado nada e sem comparar hashes manualmente. A mesma TAG detecta
adulteracao do blob.

### Fluxo

`vaultEnsureUnlocked()` centraliza tudo (o TOTP tambem chama):

- **1o uso** (sem `sec_salt`): pede a senha-mestra duas vezes, gera salt, deriva
  a chave, cria um cofre vazio e salva.
- **Desbloquear**: pede a senha, deriva a chave, tenta decifrar. Sucesso =
  destravado; falha = zera `g_key` da RAM e mostra erro.
- **Bloquear**: `memset(g_key, 0, 32)` + limpa as entradas da RAM. Fazemos isso
  ao sair do app (tecla voltar) e no item "Bloquear cofre".

### Serializacao

Entradas viram texto `K\tTITULO\tSEGREDO\n` (por isso `sanitize()` troca `\t`,
`\n`, `\r` por espaco na entrada). O tipo `K` separa senhas (`'P'`, app Cofre) de
sementes TOTP (`'T'`, app TOTP) no mesmo cofre.

### Riscos (honestidade didatica)

Isto e um cofre **pessoal, para estudo**, nao grau militar:

- A chave existe em RAM enquanto destravado; um atacante fisico com JTAG poderia
  le-la. O texto plano tambem existe por instantes durante cifra/decifra.
- Sem protecao contra keylogger de hardware nem contra forca bruta *offline* se
  alguem extrair o flash (a defesa e a senha forte + as 20000 iteracoes).
- GCM **nunca deve reusar (chave, IV)**. Geramos um IV novo e aleatorio a cada
  gravacao, o que torna a colisao improvavel para o volume de escritas de uso
  pessoal.

## Armadilhas que valem lembrar

- **Chave NVS <= 15 chars.** Usamos `sec_salt` e `sec_vault`.
- **Sem PSRAM.** Nada de buffers gigantes; os blobs do cofre sao pequenos.
- **`Font7` so tem digitos** — perfeito para o TOTP, mas nao imprima letras nela.
- **Setas = teclas `;` `.` `,` `/`.** No gerador, a tecla `Q` (QR) nao conflita
  porque nao e uma das setas. Sempre trate `Key::Back` (crase) para sair.
- **`setTextSize`/`setFont`** afetam o canvas globalmente: sempre volte a
  `setTextSize(1)` depois de ampliar, senao a proxima tela sai gigante.

## Integracao

Nada a editar aqui — a INTEGRACAO inclui `apps/seguranca/seguranca.h` no
`app_registry.cpp` e monta a categoria a partir de:

```cpp
apps::seguranca::SEGURANCA_APPS      // array
apps::seguranca::SEGURANCA_APPS_COUNT // contagem
```

Bibliotecas extras: **nenhuma** (mbedtls e QR ja vem no core).
