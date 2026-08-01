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
key(32B) = PBKDF2-HMAC-SHA256(senha, salt, iteracoes=100000)
```

- O **salt** (16 bytes aleatorios) e publico e fica na NVS (`sec_salt`). Ele
  impede tabelas rainbow: a mesma senha gera chaves diferentes em cofres
  diferentes. O salt so e **gravado apos** o primeiro `vaultSave` ter sucesso
  (ver "Fluxo") — assim nunca sobra um `sec_salt` sem blob decifravel.
- As **100000 iteracoes** deixam o *brute force* offline caro. E um
  **trade-off**: mais iteracoes = ataque mais caro, porem cada desbloqueio
  demora mais (~1-2 s no ESP32-S3). 100k e um piso razoavel sem PSRAM; subir
  demais so faria o unlock legitimo travar. Como so precisamos de 32 bytes (=
  tamanho do SHA-256), basta o "bloco 1" do PBKDF2. Reaproveitamos um unico
  contexto HMAC entre as iteracoes (`hmac_starts`/`update`/`finish`) para nao
  pagar `md_setup`/alloc a cada volta — importante num MCU.
- A **senha-mestra minima** e de **8 caracteres** (na criacao). Combinada com as
  iteracoes, e a unica defesa real contra forca bruta se alguem extrair o flash.
- **Cuidado de migracao:** alterar o numero de iteracoes muda a chave derivada e
  a GCM deixa de decifrar cofres antigos. Nesse caso use **"Resetar cofre"** para
  recriar do zero (os dados antigos sao perdidos).

### Cifra autenticada (AES-256-GCM)

GCM nao e so confidencialidade: produz uma **TAG** de 16 bytes que autentica o
dado. Guardamos o blob como `IV(12) || TAG(16) || CIPHERTEXT`, em Base64, na NVS
(`sec_vault`).

#### Nonce unico por CONSTRUCAO (nao confie so no RNG)

No GCM, **reusar o par (chave, nonce) e catastrofico**: vaza o keystream (XOR de
dois textos) e permite forjar a TAG. A chave do cofre e fixa durante a sessao,
entao a unica coisa que muda entre gravacoes e o nonce (IV de 96 bits) — ele
**precisa** ser unico.

Um IV puramente aleatorio dependeria da qualidade do `esp_random()`. Mas este e
um app **offline**: o TRNG do ESP32 so tem entropia forte garantida com o RF
(WiFi/BLE) ativo, e aqui ele pode estar desligado. Um IV aleatorio fraco poderia
**repetir** — quebra total do GCM.

Por isso o nonce e unico **por construcao**, sem depender do RNG:

```
IV[12] = contador_monotonico_BE[8] || esp_random()[4]
```

Os 8 primeiros bytes vem de um **contador monotonico** guardado na NVS
(`sec_ctr`, decimal), incrementado e **persistido a cada cifragem** (a cada
`vaultSave`). Mesmo que o RNG devolva sempre o mesmo valor, o contador nunca
repete, logo o nonce nunca repete. Os 4 bytes finais sao aleatorios apenas como
margem extra. O contador e persistido *antes* de ser usado, entao nem um reset
no meio da gravacao reaproveita um valor.

O truque elegante: **nao precisamos de hash-verificador da senha**. Se a
senha-mestra estiver errada, a chave derivada e diferente e
`mbedtls_gcm_auth_decrypt` **falha na TAG** — devolvemos "senha incorreta" sem
nunca ter revelado nada e sem comparar hashes manualmente. A mesma TAG detecta
adulteracao do blob.

### Fluxo

`vaultEnsureUnlocked()` centraliza tudo (o TOTP tambem chama):

- **1o uso** (sem `sec_salt`): pede a senha-mestra duas vezes (min 8), gera salt,
  deriva a chave, cria um cofre vazio e salva. O `sec_salt` so e gravado **depois**
  que o `vaultSave` deu certo; se o save falhar, removemos `sec_vault`/`sec_ctr` e
  nada fica meio-criado. (Antes o salt era gravado primeiro: se o save falhasse,
  o cofre ficava inacessivel para sempre.)
- **Desbloquear**: uma tela oferece **"Desbloquear"** ou **"Resetar cofre"**. No
  desbloqueio pede a senha, deriva a chave, tenta decifrar. Sucesso = destravado;
  falha = zera `g_key` da RAM e mostra erro.
- **Resetar cofre** (recuperacao): apaga `sec_salt` + `sec_vault` + `sec_ctr` e
  permite recriar um cofre novo. E a unica saida para senha esquecida, blob
  corrompido ou cofre incompativel apos mudanca de parametros do KDF. **Apaga
  todos os dados** — sempre passa por `ui::confirm(..., danger=true)`.
- **Bloquear**: `memset(g_key, 0, 32)`, zera (`wipe`) os segredos de cada entrada
  e limpa a RAM. Fazemos isso ao sair do app (tecla voltar) e no item "Bloquear
  cofre".

### Serializacao

Entradas viram texto `K\tTITULO\tSEGREDO\n` (por isso `sanitize()` troca `\t`,
`\n`, `\r` por espaco na entrada). O tipo `K` separa senhas (`'P'`, app Cofre) de
sementes TOTP (`'T'`, app TOTP) no mesmo cofre.

### Riscos (honestidade didatica)

Isto e um cofre **pessoal, para estudo**, nao grau militar:

- A chave existe em RAM enquanto destravado; um atacante fisico com JTAG poderia
  le-la. O texto plano tambem existe por instantes durante cifra/decifra.
- **Higiene de memoria:** senhas e segredos digitados (`p1`/`p2`/`pw`/`seg`/
  `segredo`) e os `secret` das entradas sao **zerados com `memset`** (`wipe`)
  assim que deixam de ser necessarios / ao bloquear. Isso **reduz** a janela de
  exposicao, mas *nao* a elimina: a Arduino `String` pode ter feito copias
  internas (realloc/`+=`/`substring`) fora do nosso alcance, e o plaintext
  serializado existe durante a (de)cifra. Nao ha limpeza perfeita sem um tipo de
  buffer dedicado.
- Sem protecao contra keylogger de hardware nem contra forca bruta *offline* se
  alguem extrair o flash (a defesa e a senha forte de 8+ chars + as 100000
  iteracoes do PBKDF2).
- GCM **nunca deve reusar (chave, IV)**. Aqui o nonce e unico **por construcao**
  (contador monotonico `sec_ctr` nos 8 primeiros bytes + 4 bytes aleatorios),
  entao a colisao nao depende da qualidade do RNG — importante num app offline em
  que o TRNG pode nao ter entropia de RF. Ver "Nonce unico por CONSTRUCAO".

## Armadilhas que valem lembrar

- **Chave NVS <= 15 chars.** Usamos `sec_salt`, `sec_vault` e `sec_ctr`.
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
