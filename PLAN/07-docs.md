## BLOCK 7 — Document the new item count and dir_scan

**Depends on:** BLOCK 6 committed
**Touches:** `README.md` (MODIFY), `host_tests/README.md` (MODIFY)

### Goal
`README.md` says that `ITENS` counts files and folders on the whole card and is re-read only after a change. It lists `dir_scan` in the architecture and documents the new `500` cause. `host_tests/README.md` names `dir_scan` among the host-tested components.

### Context to read first
1. `README.md:45-52` (Recursos), `README.md:160-176` (Tela OLED), `README.md:208-216` (response codes), `README.md:218-236` (Como funciona).
2. `host_tests/README.md:1-16`.

Read exactly these. Do not open other files unless a step below names one.

### Steps
Every old and new line below is in a fenced block; copy it exactly, including backticks and box-drawing characters.

1. In `README.md`, replace the line:
   ```
   - **Tela OLED** opcional com IP, progresso de envio, quantidade de itens e espaço usado.
   ```
   with:
   ```
   - **Tela OLED** opcional com IP, progresso de envio, quantidade de itens no cartão (arquivos e pastas, contando as subpastas) e espaço usado.
   ```
2. In `README.md`, replace the line:
   ```
   - **Código testável**: toda a lógica sem hardware (URLs, HTML, formato ZIP, desenho no display) roda e é testada no seu PC, sem placa.
   ```
   with:
   ```
   - **Código testável**: toda a lógica sem hardware (URLs, HTML, formato ZIP, desenho no display, contagem de itens) roda e é testada no seu PC, sem placa.
   ```
3. In `README.md`, inside the OLED box, replace the line:
   ```
   │ ITENS 12                 │  ← itens na raiz do cartão
   ```
   with:
   ```
   │ ITENS 12                 │  ← arquivos + pastas no cartão todo
   ```
4. In `README.md`, immediately before the line:
   ```
   O progresso na tela cobre **envios**; o download de arquivos e de `.zip` não aparece no OLED.
   ```
   insert these 2 lines (one paragraph, then one blank line):
   ```
   **ITENS** conta os arquivos e as pastas do cartão inteiro, descendo em todas as subpastas; nomes que começam com `.` ficam de fora, junto com tudo que estiver dentro de uma pasta assim (como na lista da web). Percorrer o cartão pelo SPI é lento, então a contagem e o espaço usado/livre são lidos ao ligar e depois de cada envio ou exclusão, não a cada 0,4 s. Cada leitura aparece no log como `sd card: 12 items, 123 MB used, 14500 MB free`.

   ```
5. In `README.md`, replace the line:
   ```
   | `500` | Falha ao abrir o arquivo para escrita, ou envio interrompido (o arquivo parcial é removido) |
   ```
   with:
   ```
   | `500` | Falha ao abrir ou gravar o arquivo (por exemplo, cartão cheio), ou envio interrompido (o arquivo parcial é removido) |
   ```
6. In `README.md`, inside the "Como funciona" code block, immediately after the line:
   ```
   ├── sd_card/              microSD por SPI, montado como FAT em /sdcard
   ```
   insert this line (`dir_scan/` followed by exactly 13 spaces, so the description starts in the same column as the other rows):
   ```
   ├── dir_scan/             lista e conta o que há nas pastas (POSIX opendir/readdir) — lógica pura
   ```
7. In `README.md`, in the paragraph that starts with `- **Separação entre lógica pura e hardware.**`, replace this text:
   ```
   (codificação de URL, montagem da lista, registros do ZIP, desenho no display) fica em `web_content` e `framebuffer`,
   ```
   with:
   ```
   (codificação de URL, montagem da lista, registros do ZIP, desenho no display, leitura de pastas) fica em `web_content`, `framebuffer` e `dir_scan`,
   ```
8. In `host_tests/README.md`, replace these 6 lines:
   ```
   Unit tests for the parts of this project with no hardware/network
   dependency: `framebuffer` (OLED pixel/text drawing) and `web_content`
   (the web server's string-building logic -- URL encode/decode, filename
   safety, file-size formatting, HTML fragment rendering). They build as a
   native executable via ESP-IDF's `linux` target and run directly on this
   machine, no board or flashing involved.
   ```
   with:
   ```
   Unit tests for the parts of this project with no hardware/network
   dependency: `framebuffer` (OLED pixel/text drawing), `web_content`
   (the web server's string-building logic -- URL encode/decode, filename
   safety, file-size formatting, HTML fragment rendering) and `dir_scan`
   (listing and counting directory entries, tested against a scratch
   directory under the system temp dir). They build as a native executable
   via ESP-IDF's `linux` target and run directly on this machine, no board
   or flashing involved.
   ```
9. In `host_tests/README.md`, replace the line:
   ```
   points at `framebuffer` and `web_content`, not the whole `components/`
   ```
   with:
   ```
   points at `framebuffer`, `web_content` and `dir_scan`, not the whole `components/`
   ```

### Do not
- Do not add a new README section, a changelog entry, or screenshots.
- Do not edit any other README line (API table rows, Limitações, Solução de problemas).
- Do not translate the README; it stays Portuguese with accents (only the web interface text is accent-free).

### Verify
Run from the repository root, in this order:
```
grep -c 'itens na raiz' README.md
grep -c 'arquivos + pastas no cartão todo' README.md
grep -c '├── dir_scan/             lista e conta' README.md
grep -c 'Falha ao abrir ou gravar o arquivo' README.md
grep -c 'dir_scan' host_tests/README.md
git diff --stat -- README.md host_tests/README.md
```
Expected: `0`, `1`, `1`, `1`, `2`, and the `--stat` shows only `README.md` and `host_tests/README.md`.

### If verification fails
1. Read the failing output in full.
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set this block's row in §12 Status to `DONE`.
2. Run:
   ```
   git add README.md host_tests/README.md PLAN.md
   git commit -m "Document the OLED item count and the dir_scan component"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
