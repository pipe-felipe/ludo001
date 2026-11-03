## BLOCK 8 — Document firmware updates in the READMEs

**Depends on:** BLOCK 7 committed
**Touches:** `README.md` (MODIFY), `host_tests/README.md` (MODIFY)

### Goal
`README.md` explains how to create the signing key, the one cable flash, updates over the network, rollback, the new route and its status codes, the OLED lines, and the new partition sizes. `host_tests/README.md` lists the firmware checks.

### Context to read first
1. `README.md`: the whole file (300 lines). It is Portuguese with accents. Keep that, since only the UI strings lack accents.
2. `host_tests/README.md:1-17`.

Read exactly these. Do not open other files unless a step below names one.

### Steps
Each step replaces one exact text with another. The old text occurs exactly once in the file. Apply the steps in order. The text inside each fence is literal: copy it byte for byte, including the box-drawing characters and the inner code fences.

1. In `README.md`, in the table of contents, add the new section after the OLED entry. Replace
   ```markdown
   - [Tela OLED](#tela-oled)
   ```
   with
   ```markdown
   - [Tela OLED](#tela-oled)
   - [Atualizando o firmware](#atualizando-o-firmware)
   ```
2. In `README.md`, in **Recursos**, replace the OLED bullet with two bullets. Replace
   ```markdown
   - **Tela OLED** opcional com IP, progresso de envio, quantidade de itens no cartão (arquivos e pastas, contando as subpastas) e espaço usado.
   ```
   with
   ```markdown
   - **Tela OLED** opcional com IP, progresso de envio e de atualização, quantidade de itens no cartão (arquivos e pastas, contando as subpastas), espaço usado e versão do firmware.
   - **Atualização do firmware pela rede** (OTA), direto na página de status: a placa só aceita firmware do ludo001 assinado com a sua chave, e um firmware novo que não sobe o Wi-Fi volta sozinho para o anterior.
   ```
3. In `README.md`, in **Recursos**, add the firmware header to the testable logic. Replace
   ```markdown
   (URLs, HTML, formato ZIP, desenho no display, contagem de itens)
   ```
   with
   ```markdown
   (URLs, HTML, formato ZIP, cabeçalho do firmware, desenho no display, contagem de itens)
   ```
4. In `README.md`, in **Começando**, insert the new step 4 and renumber the old step 4. Replace
   ```markdown
   ### 4. Compile, grave e acompanhe

   Com a placa conectada por USB:
   ```
   with
   ````markdown
   ### 4. Crie a chave de assinatura

   A placa só aceita atualizações pela rede assinadas com a sua chave. Crie a chave uma vez, na raiz do projeto, com o ambiente do ESP-IDF ativo:

   ```bash
   idf.py secure-generate-signing-key --version 1 --scheme ecdsa256 secure_boot_signing_key.pem
   ```

   O arquivo está no `.gitignore` e **nunca vai para o Git**. Guarde uma cópia num lugar seguro, fora deste computador: sem ela, só dá para atualizar pelo cabo, com uma chave nova. A compilação assina o `build/ludo001.bin` com essa chave; sem o arquivo, ela para com um erro.

   ### 5. Compile, grave e acompanhe

   A **primeira gravação é pelo cabo**: ela grava o bootloader e a tabela de partições, que nunca mudam pela rede. Faça isso antes de fechar a placa numa case. Com a placa conectada por USB:
   ````
5. In `README.md`, in the expected log of the same step, add the firmware line, then point to the OTA section and renumber step 5. Replace
   ````markdown
   ```
   ludo001: wifi connected, ssid=NomeDaSuaRede ip=192.168.0.42
   ludo001: sd card mounted, capacity=... MB
   ludo001: web server started
   ```

   ### 5. Acesse
   ````
   with
   ````markdown
   ```
   ludo001: firmware 514d68f running from ota_0
   ludo001: wifi connected, ssid=NomeDaSuaRede ip=192.168.0.42
   ludo001: sd card mounted, capacity=... MB
   ludo001: web server started
   ```

   Daí em diante, as atualizações podem ser feitas pela rede, sem o cabo: veja [Atualizando o firmware](#atualizando-o-firmware).

   ### 6. Acesse
   ````
6. In `README.md`, in the OLED diagram, relabel the progress bar. Replace
   ```markdown
   │ [██████░░░░░░░░░░░░░░░░] │  ← progresso do envio em andamento
   ```
   with
   ```markdown
   │ [██████░░░░░░░░░░░░░░░░] │  ← progresso do envio ou da atualização
   ```
7. In `README.md`, in the OLED diagram, add the version line. Replace
   ```markdown
   │ USO 123/14500MB          │  ← espaço usado / livre
   ```
   with
   ```markdown
   │ USO 123/14500MB          │  ← espaço usado / livre
   │ FW 514D68F               │  ← versão do firmware
   ```
8. In `README.md`: after the OLED diagram, describe the update line and the version line, then add the new section **Atualizando o firmware**. Replace
   ```markdown
   O progresso na tela cobre **envios**; o download de arquivos e de `.zip` não aparece no OLED.
   ```
   with
   ````markdown
   Durante uma atualização do firmware, a linha **ITENS** dá lugar a **ATUALIZANDO** e o percentual já gravado. A última linha mostra a versão do firmware em execução (a fonte da tela só tem maiúsculas).

   O progresso na tela cobre **envios** e **atualizações do firmware**; o download de arquivos e de `.zip` não aparece no OLED.

   ## Atualizando o firmware

   Depois da primeira gravação pelo cabo, o firmware é atualizado pela rede, com a placa longe do computador:

   1. Compile com `idf.py build`. O arquivo assinado é `build/ludo001.bin`.
   2. Abra `http://ludo001.local/`, toque em *Escolher firmware*, escolha o `build/ludo001.bin` e toque em **Atualizar firmware**.
   3. A placa grava a imagem na partição que não está em uso, confere o tamanho, o nome do projeto e a assinatura, e reinicia. A página mostra **Reiniciando...** e recarrega sozinha quando a placa volta, com a versão nova em *Versao*.

   Pelo terminal:

   ```bash
   curl -X POST --data-binary @build/ludo001.bin http://ludo001.local/firmware
   ```

   - **O firmware anterior fica guardado.** Um envio interrompido ou recusado não muda nada na placa.
   - **Volta automática (rollback).** Um firmware novo tem 60 s, a partir do boot, para conectar no Wi-Fi e subir o servidor. Se não conseguir, a placa volta sozinha para o anterior. Se o roteador estiver desligado nesse primeiro boot, ela também volta: é só enviar de novo.
   - **A versão** vem do `git describe` na compilação (por exemplo `514d68f`, ou `514d68f-dirty` com mudanças sem commit) e aparece na página de status, na última linha do OLED e no log (`firmware ... running from ota_0`).
   - **Uma atualização por vez.** Enquanto a placa espera para reiniciar depois de uma atualização aceita, `POST /firmware` e `POST /upload/` respondem `409`.
   ````
9. In `README.md`, in the **API HTTP** route table, add the firmware route after the delete route. Replace
   ```markdown
   | `POST` | `/delete/<caminho>` | Apaga o arquivo, ou a pasta com tudo dentro |
   ```
   with
   ```markdown
   | `POST` | `/delete/<caminho>` | Apaga o arquivo, ou a pasta com tudo dentro |
   | `POST` | `/firmware` | Grava o corpo da requisição (um `ludo001.bin` assinado) como firmware novo e reinicia nele. Veja [Atualizando o firmware](#atualizando-o-firmware) |
   ```
10. In `README.md`, in the **API HTTP** examples, add the firmware update. Replace
   ```markdown
   # apagar
   curl -X POST http://ludo001.local/delete/foto.jpg
   ```
   with
   ```markdown
   # apagar
   curl -X POST http://ludo001.local/delete/foto.jpg

   # atualizar o firmware (o .bin assinado que o idf.py build gera)
   curl -X POST --data-binary @build/ludo001.bin http://ludo001.local/firmware
   ```
11. In `README.md`, in the response code table, extend the 400 row. Replace
   ```markdown
   | `400` | Nome inválido (vazio, começando com `/`, ou contendo `..`), cartão não montado, ou `/zip/` sem pasta (raiz) |
   ```
   with
   ```markdown
   | `400` | Nome inválido (vazio, começando com `/`, ou contendo `..`), cartão não montado, ou `/zip/` sem pasta (raiz). Em `/firmware`: imagem que não é do ludo001, truncada, corrompida, ou sem a assinatura da sua chave |
   | `409` | `/firmware` ou `/upload/` enquanto uma atualização já aceita espera a placa reiniciar |
   ```
12. In `README.md`, in the response code table, extend the 413 row. Replace
   ```markdown
   | `413` | A pasta pediu um `.zip` maior que 4 GiB ou com mais de 65535 itens |
   ```
   with
   ```markdown
   | `413` | A pasta pediu um `.zip` maior que 4 GiB ou com mais de 65535 itens. Em `/firmware`: imagem vazia ou maior que a partição (1700 KB) |
   ```
13. In `README.md`, in the response code table, extend the 500 row. Replace
   ```markdown
   | `500` | Falha ao abrir ou gravar o arquivo (por exemplo, cartão cheio), ou envio interrompido (o arquivo parcial é removido) |
   ```
   with
   ```markdown
   | `500` | Falha ao abrir ou gravar o arquivo (por exemplo, cartão cheio), ou envio interrompido (o arquivo parcial é removido). Em `/firmware`: falha ao gravar na flash, ou envio interrompido (o firmware atual continua) |
   ```
14. In `README.md`, in **Como funciona**, describe the update in the web_server line of the tree. Replace
   ```markdown
   ├── web_server/           servidor esp_http_server + web/ (index.html, files.html, style.css, app.js)
   ```
   with
   ```markdown
   ├── web_server/           servidor esp_http_server + web/ (index.html, files.html, style.css, app.js) + atualização OTA
   ```
15. In `README.md`, in the same tree, add the firmware header to web_content. Replace
   ```markdown
   └── web_content/          lógica pura do servidor: URL, nomes numerados, HTML dos itens, formato ZIP
   ```
   with
   ```markdown
   └── web_content/          lógica pura do servidor: URL, nomes numerados, HTML dos itens, formato ZIP, cabeçalho do firmware
   ```
16. In `README.md`, in **Como funciona**, add a decision bullet after the browser search bullet. Replace
   ```markdown
   - **Busca no navegador.** A pasta aberta já está inteira na página, então o filtro roda no JavaScript do cliente, sem pedir nada ao ESP32.
   ```
   with
   ```markdown
   - **Busca no navegador.** A pasta aberta já está inteira na página, então o filtro roda no JavaScript do cliente, sem pedir nada ao ESP32.
   - **Atualização em duas partições.** A flash de 4 MB tem duas partições de aplicação de 1700 KB (`ota_0` e `ota_1`, a tabela pronta "Two large size OTA partitions" do ESP-IDF). A atualização grava a partição que não está em uso, aos poucos, enquanto recebe. O `esp_ota_end()` confere a imagem inteira e a assinatura (ECDSA, sem Secure Boot e sem queimar eFuses) antes de a placa trocar de partição, e o bootloader com rollback volta para a anterior se a nova não for confirmada.
   ```
17. In `README.md`, in **Testes**, mention the update among what is checked on the board. Replace
   ```markdown
   O HTML/CSS/JS da interface e o servidor em si (`httpd`, cartão SD) não têm testes automatizados; eles são verificados na placa.
   ```
   with
   ```markdown
   O HTML/CSS/JS da interface e o servidor em si (`httpd`, cartão SD, atualização pela rede) não têm testes automatizados; eles são verificados na placa.
   ```
18. In `README.md`, in **Desenvolvimento**, replace the firmware-space and route bullets, and add the configuration bullet. Replace
   ```markdown
   - **Espaço no firmware:** a partição de aplicação tem 1 MB e o firmware já ocupa cerca de 97% dela. Antes de acrescentar algo grande, confira a folga no fim do `idf.py build` ou ajuste a tabela de partições e o tamanho da flash (`idf.py menuconfig`).
   - **Rotas HTTP:** o servidor usa as 8 rotas que o `HTTPD_DEFAULT_CONFIG()` permite. Para adicionar uma, aumente `config.max_uri_handlers` em `web_server.cpp`.
   ```
   with
   ```markdown
   - **Espaço no firmware:** cada partição de aplicação tem 1700 KB, e o firmware assinado ocupa pouco mais de 1 MB, então sobram perto de 700 KB. A folga aparece no fim do `idf.py build`.
   - **Rotas HTTP:** o servidor registra 9 rotas e aceita até 12 (`config.max_uri_handlers` em `web_server.cpp`). Para passar disso, aumente esse número.
   - **Configuração:** as opções de que o projeto depende (flash de 4 MB, partições de OTA, rollback, imagens assinadas) ficam em `sdkconfig.defaults`. Depois de mudar esse arquivo, rode `idf.py reconfigure`.
   ```
19. In `README.md`, in **Limitações e segurança**, add the signing bullets after the Wi-Fi password bullet. Replace
   ```markdown
   - A senha do Wi-Fi fica **embutida no binário do firmware** (gerado a partir do `.env`). Não compartilhe os arquivos `.bin` que você compilar.
   ```
   with
   ```markdown
   - A senha do Wi-Fi fica **embutida no binário do firmware** (gerado a partir do `.env`). Não compartilhe os arquivos `.bin` que você compilar.
   - **Atualização pela rede só com a sua chave.** A placa recusa qualquer `.bin` que não seja do ludo001 ou que não esteja assinado com a `secure_boot_signing_key.pem`. Quem tem a placa na mão ainda pode gravar qualquer coisa pelo cabo: não há Secure Boot.
   - **Guarde a chave de assinatura.** Sem ela, não dá mais para atualizar pela rede: é preciso gerar uma chave nova e gravar pelo cabo. Nunca a coloque no Git nem a compartilhe. O firmware guarda só a parte pública, para conferir a assinatura.
   ```
20. In `README.md`, in **Solução de problemas**, add 3 rows after the last row. Replace
   ```markdown
   | O download de uma pasta falha no meio | Normal se a conexão cair: o servidor encerra a conexão em vez de entregar um `.zip` truncado. Tente de novo |
   ```
   with
   ```markdown
   | O download de uma pasta falha no meio | Normal se a conexão cair: o servidor encerra a conexão em vez de entregar um `.zip` truncado. Tente de novo |
   | Atualização recusada com **400** | O `.bin` não é do ludo001, está incompleto, ou foi assinado com outra chave (ou não foi assinado). Envie o `build/ludo001.bin` compilado com a sua `secure_boot_signing_key.pem` |
   | A placa voltou para a versão anterior depois de atualizar | O firmware novo não conectou no Wi-Fi e não subiu o servidor em 60 s, e o bootloader voltou para o anterior (rollback). Confira o `.env` usado na compilação e se o roteador estava ligado; depois envie de novo |
   | A compilação para dizendo que `secure_boot_signing_key.pem` não existe | Crie a chave (veja [Começando](#4-crie-a-chave-de-assinatura)) ou copie a sua para a raiz do projeto, e rode `idf.py reconfigure` |
   ```
21. In `host_tests/README.md`, add the firmware checks to the list of what `web_content` covers. Replace
   ```markdown
   safety, file-size formatting, HTML fragment rendering) and `dir_scan`
   ```
   with
   ```markdown
   safety, file-size formatting, HTML fragment rendering, firmware image
   header checks) and `dir_scan`
   ```

### Do not
- Do not rewrite sections the steps don't name, and do not reflow paragraphs.
- Do not add screenshots, diagrams or badges.
- Do not translate the README or remove its accents. The no-accents rule applies to UI strings only (§7 rule 3).
- Do not document the private key's content or any Wi-Fi value.

### Verify
Run from the repository root, in this order:
```
grep -c 'Atualizando o firmware' README.md
grep -n '^### [1-6]\. ' README.md
grep -c '/firmware' README.md
grep -n '97%\|as 8 rotas' README.md
grep -c 'secure_boot_signing_key.pem' README.md
grep -c 'firmware image' host_tests/README.md
git diff --stat
```
Expected:
1. `4`: the table-of-contents entry, the section heading, and 2 links to it.
2. 6 lines, in order: `### 1. Instale o ESP-IDF 6.1`, `### 2. Baixe o projeto`, `### 3. Configure o Wi-Fi`, `### 4. Crie a chave de assinatura`, `### 5. Compile, grave e acompanhe`, `### 6. Acesse`.
3. `8`.
4. No output: both stale statements are gone.
5. `4`.
6. `1`.
7. Only `README.md` and `host_tests/README.md` are listed.

### If verification fails
1. Read the failing output in full.
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set BLOCK 8's row in §12 Status to `DONE`.
2. Run:
   ```
   git add README.md host_tests/README.md PLAN.md
   git commit -m "Document firmware updates over the network"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
