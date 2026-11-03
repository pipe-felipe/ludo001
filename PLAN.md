# PLAN — Update the ludo001 firmware over the network (OTA)

Machine-readable execution plan. Audience: the LLM executing it. Follow it literally.
Progress lives in §12 Status. Start with §11.

## 1. Task

Add over-the-air firmware updates to ludo001, as the Notion card "Atualizar o firmware do ludo001 pelo navegador (OTA)" specifies. The flash becomes 4 MB, with ESP-IDF's ready-made "Two large size OTA partitions" table (`ota_0` and `ota_1`, 1700K each) and app rollback. All of these settings live in a new `sdkconfig.defaults`, which `idf.py reconfigure` applies to `sdkconfig`.

A new route, `POST /firmware`, streams the request body into the OTA partition the board is not running from. It answers:
- 413 for an empty image or one larger than the partition;
- 400 for an image whose app header is not `ludo001`;
- 400 for an image that `esp_ota_end()` rejects (truncated, corrupt, or not signed with the project's ECDSA key);
- 409 while an accepted update waits for its restart.

The route is protected by signed images: `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` with the ECDSA (V1) scheme, checked on update only. There is no Secure Boot and no eFuse write. The private key is `secure_boot_signing_key.pem` in the repository root, and it is gitignored.

A new firmware is confirmed only after Wi-Fi and the HTTP server are both up. If that hasn't happened 60 s after boot, the board rolls back to the previous firmware.

The status page gets a "Firmware" card: the running version, a `.bin` chooser, an indeterminate progress bar, then "Reiniciando..." and a reload. The OLED shows `ATUALIZANDO <n>%` during an update and `FW <version>` on its last line. For that, its stroke font gains the missing letters, `-` and `%`.

Decided during planning (settled, do not revisit):
- Route protection is signed images; there is no password and no button.
- The card's three analysis decisions stand: 1700K partitions, rollback on, and confirmation only after Wi-Fi plus HTTP within 60 s.
- The Notion card is not read or edited. Measurements go into the final report only.
- `esp_http_server` runs every handler on one task, so two requests never run at the same time. The 409 therefore covers the 1-second window between an accepted update and its restart, for both `POST /firmware` and `POST /upload/*`.
- An update is never refused for having the same version as the running firmware.

## 2. Definition of done

Every item is checkable by running a named command or reading a named file.

- [ ] `sdkconfig.defaults` holds exactly 4 `CONFIG_` lines, and `sdkconfig` contains the 10 lines listed in BLOCK 1 **Verify** (`PLAN/01-ota-config.md`).
- [ ] `idf.py partition-table` prints the rows `nvs,data,nvs,0x9000,24K,`, `otadata,data,ota,0xf000,8K,`, `phy_init,data,phy,0x11000,4K,`, `ota_0,app,ota_0,0x20000,1700K,` and `ota_1,app,ota_1,0x1d0000,1700K,`.
- [ ] `idf.py build` exits 0, prints no `warning:` line, and prints `Smallest app partition is 0x1a9000 bytes` with at least `0x90000` bytes free.
- [ ] The host test run prints `95 Tests 0 Failures 0 Ignored` and `OK`.
- [ ] `secure_boot_signing_key.pem` exists. `git check-ignore -q secure_boot_signing_key.pem` exits 0, and `git ls-files secure_boot_signing_key.pem` prints nothing.
- [ ] After the one cable flash, the board's boot log contains `Loaded app from partition at offset 0x20000` and `running from ota_0`. `/files` lists as many root items as it did before the flash (BLOCK 7).
- [ ] With the board on the HW-131 supply, `POST /firmware` returns 200 `OK`, and `GET /` then shows the new version (BLOCK 7 T1, BLOCK 9).
- [ ] An interrupted upload, another project's `.bin`, an unsigned `.bin`, a `.bin` signed with another key and a truncated `.bin` all leave the board running its previous version (BLOCK 7 T2–T6).
- [ ] An empty or oversized body gets 413, and a request that arrives while an accepted update waits for its restart gets 409 (BLOCK 7 T7–T8).
- [ ] A firmware built without Wi-Fi credentials rolls back to the previous version by itself (BLOCK 7 T9).
- [ ] The human confirmed that the OLED shows `ATUALIZANDO` with a rising percentage during an update and `FW <version>` on its last line (BLOCK 7).
- [ ] `README.md` documents the firmware route, the signing key, rollback and the new partition sizes (BLOCK 8 **Verify** greps).
- [ ] All gates in §5 pass at least as well as §6 Baseline.
- [ ] `PLAN.md` no longer exists, and neither does `PLAN/`.

## 3. Out of scope

- Hardware Secure Boot, flash encryption, anti-rollback (`CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK`), and any other eFuse write.
- Signature checks by the bootloader on every boot (`CONFIG_SECURE_SIGNED_ON_BOOT_NO_SECURE_BOOT` stays off).
- The RSA (V2) signing scheme; it needs `CONFIG_ESP32_REV_MIN` 3.0.
- HTTPS, and authentication for the file routes (`/files`, `/upload`, `/delete`, `/zip`).
- The board fetching updates by itself (`esp_https_ota`).
- Using the ~540 KB of flash left after `ota_1`.
- Real upload percentages in the browser. The bar is indeterminate, as for file uploads.
- Reading or editing the Notion card.

## 4. Branch

Branch: `feat/ota-firmware-update`
Base: `main` at `514d68f`
Created for this task.

All work happens on this branch. See §11 R4.

## 5. Project facts

Every command below was read from the file cited. A row marked `NONE FOUND` must never be run.
A row marked `write-only` rewrites files: it belongs in a block's Steps, never in a **Verify**
list and never in the final block's gate list.

`idf.py`, `python -m esptool` and `python -m espsecure` exist only after the activation script runs **in the same shell command** (`idf.py` is a shell function). Every command below already includes the activation. A full firmware build takes up to 6 minutes, so run every build with a 600000 ms timeout.

| Fact | Value | Source |
|---|---|---|
| Package manager | ESP-IDF component manager (`dependencies.lock`, `components/web_server/idf_component.yml`) | `dependencies.lock` |
| Toolchain activation | `source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1` | `host_tests/README.md:28` |
| Firmware build (gate) | command **C-BUILD** below | `README.md:127`, `README.md:267` |
| Host unit tests (gate) | command **C-HOST** below | `README.md:250-258`, `host_tests/README.md:25-37` |
| Apply `sdkconfig.defaults` | command **C-RECONF** below | ESP-IDF `idf.py`; tested in planning on a copy of `main@514d68f` |
| Partition table | command **C-PART** below | Notion card checklist, Fase 1 |
| Flash over USB | command **C-FLASH** below | `README.md:127` |
| Generate a signing key | command **C-KEYGEN** below | `~/.espressif/v6.1/esp-idf/components/bootloader/Kconfig.projbuild:732-746`, `espsecure generate-signing-key --help` |
| Sign an image | command **C-SIGN** below | `espsecure sign-data --help` |
| JS syntax check | command **C-JS** below | planner-added; `node` is `/home/linuxbrew/.linuxbrew/bin/node` |
| Board address | `http://ludo001.local` (answered 200 from 192.168.3.234 on 2026-11-03) | `README.md:140-147` |
| Board test commands (BLOCK 7 and BLOCK 9 only) | `curl` against `http://ludo001.local` (`/`, `/files`, `/firmware`, `/upload/...`); `idf.py -B <dir> -DPROJECT_VER=<ver> build` (a build with a forced version); `idf.py -C <dir> build` (a build of an exported copy); `python -m esptool -p /dev/ttyUSB0 chip-id` (board identity). Every one is written out in full in those blocks | `README.md:180-218`; `~/.espressif/v6.1/esp-idf/tools/cmake/project.cmake:813-858`; `embedded` skill, Workflow step 1 |
| Lint | NONE FOUND | — |
| Format check / write | NONE FOUND | — |
| E2E | NONE FOUND | — |
| CI gates, in order | NONE FOUND (no `.github/`). Local gates, in this order: **C-HOST**, **C-BUILD** | — |
| Commit style | one line, English, imperative, no trailing period, no co-author or AI attribution line | `README.md:269`, `~/.claude/CLAUDE.md` |
| Commit examples | "Add a search box that filters the files in the current folder" / "Number an upload instead of overwriting a file with the same name" | `git log` |

Commands, run from the repository root exactly as written:

**C-BUILD**: firmware build. Pass means `fw build exit=0`, no line containing `warning:`, and two `binary size` lines.
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py build > build/fw-build.log 2>&1; echo "fw build exit=$?"; grep -E 'warning:|binary size' build/fw-build.log
```

**C-HOST**: host unit tests. Pass means `host build exit=0`, then `<N> Tests 0 Failures 0 Ignored` and `OK`. When the build fails, read `build/host-tests-build.log`.
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; export PATH="/usr/bin:$PATH"; idf.py -C host_tests build > build/host-tests-build.log 2>&1; echo "host build exit=$?"; ./host_tests/build/ludo001_host_tests.elf | tail -3
```

**C-RECONF**: applies `sdkconfig.defaults` to `sdkconfig`.
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py reconfigure > build/reconfigure.log 2>&1; echo "reconfigure exit=$?"
```

**C-PART**: prints the partition rows.
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py partition-table 2>&1 | grep -E '^(nvs|otadata|phy_init|ota_0|ota_1),'
```

**C-FLASH**: the only cable flash, used in BLOCK 7 only.
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py -p /dev/ttyUSB0 flash > build/ota-test/flash.log 2>&1; echo "flash exit=$?"
```

**C-KEYGEN**: replace `<file>` as the block says.
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; python -m espsecure generate-signing-key --version 1 --scheme ecdsa256 <file>
```

**C-SIGN**: replace `<key>`, `<out>` and `<in>` as the block says.
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; python -m espsecure sign-data --version 1 --keyfile <key> --output <out> <in>
```

**C-JS**: pass means exit 0 with no output.
```
node --check components/web_server/web/app.js
```

## 6. Baseline — recorded 2026-11-03

| Gate | Command | Result |
|---|---|---|
| Host unit tests | §5 C-HOST | `host build exit=0`, `83 Tests 0 Failures 0 Ignored`, `OK` |
| Firmware build | §5 C-BUILD | `fw build exit=0`, no `warning:` lines, `Bootloader binary size 0x6640 bytes. 0x9c0 bytes (9%) free.`, `ludo001.bin binary size 0xf9040 bytes. Smallest app partition is 0x100000 bytes. 0x6fc0 bytes (3%) free.` |
| Board | `curl -s -m 6 -o /dev/null -w '%{http_code}' http://ludo001.local/` | `200`; the board runs the 2 MB, single-app firmware of `main@514d68f` |
| Lint / format / E2E | — | NONE FOUND |

Planning built a copy of the repository with every block of this plan applied. These are the expected figures. Signed images are padded to a 64 KB boundary, so the app size moves only in 64 KB steps:
- `Bootloader binary size 0x66a0 bytes. 0x960 bytes (8%) free.`
- `ludo001.bin binary size 0xffff4 bytes. Smallest app partition is 0x1a9000 bytes. 0xa900c bytes (40%) free.`
- `95 Tests 0 Failures 0 Ignored`

Known pre-existing failures: None.
A failure listed here is not caused by this plan: do not fix it, do not let it block a block (§11 R12).

Pre-existing uncommitted paths: None.
These paths are not yours: never stage them, never revert them. The final block expects to see
them in `git status`.

## 7. Architecture and style rules that bind this task

1. Pure logic with no `esp_*` header goes in `components/web_content`. Its test goes in `host_tests/main/test_*.cpp`, listed in `host_tests/main/CMakeLists.txt` and declared plus `RUN_TEST`-registered in `host_tests/main/test_main.cpp`. Source: `README.md:237`, `README.md:266`, `host_tests/README.md:39-44`.
2. Front-end code lives in files under `components/web_server/web/`. No HTML, CSS or JS is written inside C++ beyond the existing fragment builders. Source: `README.md:238`, project memory "Keep HTML/CSS/JS out of .cpp".
3. UI text is Portuguese without accents (`Versao`, `Reiniciando...`). HTTP error messages and log lines are English. Source: `README.md:265`.
4. Build settings change only through `sdkconfig.defaults` plus `idf.py reconfigure`. Never hand-edit `sdkconfig`, and never run `idf.py menuconfig` (it is interactive). Every entry in `sdkconfig` is marked `# default:`, which is why `sdkconfig.defaults` overrides it. Source: `sdkconfig:668-725`.
5. C++ follows the style of `components/web_server/web_server.cpp`:
   - helpers in the anonymous namespace;
   - `kCamelCase` constants and `snake_case` functions;
   - a `// POST /route -- ...` comment above each handler;
   - `httpd_resp_send_err` for error answers.
   Source: `components/web_server/web_server.cpp:285-309`.
6. Comments match the surrounding density: one comment block per function and a short inline comment on a non-obvious line. No placeholder or to-do comments. Source: `components/web_server/web_server.cpp`.
7. A handler returns `ESP_FAIL` after an error, except where this plan's code returns `ESP_OK` on purpose. That happens in `firmware_handler`'s rejections before the body is read and in `send_restart_pending` callers. It lets httpd discard the unread body, so the client receives the status instead of a connection reset. Source: `~/.espressif/v6.1/esp-idf/components/esp_http_server/src/httpd_parse.c:844-873`.
8. NOTE: the docs and the card are stale on two points, and the code wins.
   - `README.md:267-268` describe a 1 MB partition and an 8-route limit. BLOCK 8 rewrites both lines.
   - The Notion card assumes a file upload and a firmware update can run at the same time. They can't: `esp_http_server` runs every handler on one task.

## 8. Security invariants

1. Never burn an eFuse, never run any `espefuse` command, and never run `idf.py erase-flash`. Never enable `CONFIG_SECURE_BOOT`, `CONFIG_SECURE_FLASH_ENC_ENABLED`, `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK`, `CONFIG_SECURE_SIGNED_ON_BOOT_NO_SECURE_BOOT` or any `CONFIG_SECURE_BOOT_ALLOW_*` / insecure option. Source: Notion card "Fora do escopo"; `embedded` skill hard rule 7.
2. `secure_boot_signing_key.pem` is listed in `.gitignore` before it is created and is never staged. Never print, `cat`, `head`, `grep` or otherwise display its content, never overwrite it, and never copy it outside the repository. The one allowed copy is BLOCK 7's temporary copy inside `build/ota-test/nowifi/`, which BLOCK 7 deletes. If the file already exists when BLOCK 1 starts, stop (R12): it may be the human's own key. Source: `~/.espressif/v6.1/esp-idf/components/bootloader/Kconfig.projbuild:732-746`.
3. Never print, stage, edit or copy `.env`. It holds the Wi-Fi password. Log lines that contain the SSID are counted with `grep -c`, never printed. Source: `README.md:111`, `README.md:275`.
4. `firmware_handler` writes nothing to flash before both `firmware_size_fits` and `firmware_head_matches` pass. It calls `esp_ota_set_boot_partition` only after `esp_ota_end` returned `ESP_OK`. `CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT=y`, which makes `esp_ota_end` check the signature, must stay set. Source: `~/.espressif/v6.1/esp-idf/components/bootloader/Kconfig.projbuild:629-642`.
5. Flash is written only through `esp_ota_begin` / `esp_ota_write` / `esp_ota_end` on the partition from `esp_ota_get_next_update_partition`. Never call `esp_partition_write` or `esp_partition_erase_range`, and never touch the running partition. Source: `~/.espressif/v6.1/esp-idf/components/app_update/include/esp_ota_ops.h`.
6. Error answers and logs never contain request bodies, key material or Wi-Fi credentials. Source: `README.md:271-283`.
7. The existing `is_safe_filename` checks in `files_item_handler`, `upload_handler`, `delete_handler` and `zip_handler` stay exactly as they are. Source: `README.md:213`, `components/web_server/web_server.cpp:207,293,334,470`.
8. The board is never powered by USB and the HW-131 supply at the same time. Only the human moves it, when BLOCK 7 asks. Source: `embedded` skill "General wiring rules".

## 9. Quality bars

Violating any of these is a defect even when the tests pass.

- Do not write a second receive loop for the firmware body. `store_body` and `write_firmware` share `recv_body`. `recv_exact` exists only to read the first `kFirmwareHeadSize` bytes.
- Do not add an error struct, an error enum or an exception path for the firmware route. `write_firmware` answers the request itself and returns `bool`, the way `send_download` answers its own 404.
- Do not add a Kconfig option, an environment variable or a `.env` entry for the restart delay, the 60 s timeout, `max_uri_handlers` or the signing key path.
- Do not add a route or a JS file for the status page. `index.html` loads the existing `/app.js`, and the bottom IIFE in `app.js` returns early when `#f` is missing.
- Do not compare versions or refuse a same-version image. `native_ota_example` does that; this task does not.
- Name every byte offset and magic number in `components/web_content/web_server_firmware.cpp` as a `k...` constant, as the code in this plan does.
- Do not change what `store_body` does: same error strings, partial file still removed, same progress semantics.
- Do not use a bitmap font table for the new glyphs. Use segment lists, like the existing `kGlyph*` arrays.
- Do not change existing tests or their assertions; only add the tests this plan lists.
- When "avoid duplication" and "avoid over-engineering" conflict, choose the smaller diff: duplicate once, extract on the third occurrence.

## 10. Test obligations

| Kind | Required | Why | Location | Command |
|---|---|---|---|---|
| Unit (host, Unity) | yes | new pure logic: firmware image checks, new OLED glyphs | `host_tests/main/test_web_server_firmware.cpp` (NEW), `host_tests/main/test_framebuffer.cpp` | §5 C-HOST |
| Firmware build | yes | every change is compiled for the ESP32 target | — | §5 C-BUILD |
| JS syntax | yes | `app.js` changes; the project has no JS tests | — | §5 C-JS |
| Integration | no | the project has none | — | — |
| E2E | no | the project has none | — | — |
| Smoke on the board | yes | OTA, rollback and signature checks only run on the hardware | BLOCK 7, BLOCK 9 | commands inside those blocks |

Follow the existing test style at `host_tests/main/test_web_server_zip.cpp`.

## 11. EXECUTOR CONTRACT

You are executing a plan, not designing one. Every decision in it has already been made and
approved. Your job is to carry out one block at a time, exactly as written, and to stop when
reality disagrees with the plan.

**R1 — One block at a time.** Read §1 through §12 first: that is your whole brief. Then open
§12 Status, find the first row whose status is `TODO`, and read that one block. Read no other
block. Do not read ahead, do not batch two blocks, do not reorder.

**R2 — Do not redesign.** If a step looks suboptimal, do it anyway. If a step looks *wrong*
or impossible, stop (R12). Never substitute your own approach.

**R3 — Touch only the listed files.** A block's **Touches** list is exhaustive. If the change
seems to require editing any other file, stop (R12).

**R4 — One branch.** Before starting a block, run `git branch --show-current`. If the output
is not `feat/ota-firmware-update`, stop (R12). Never leave, hide or rewrite the branch's work: no
`git checkout feat/ota-firmware-update`, `git switch`, `git merge`, `git rebase`, `git reset`, `git stash`,
`git commit --amend`, `git push`, `git tag`, and no command that opens a pull request.
Never create a second branch.
Restoring a file with `git checkout -- <path>` is allowed; that is the only `git checkout`
you may run.

**R5 — Always verify.** Run the block's **Verify** commands exactly as written, from the
repository root, all of them, in the given order. A block is never done because the code
"looks right". Never edit a Verify command to make it easier.

**R6 — Never weaken a check.** Do not delete, rename, skip, `.skip`, `xit`, `@Ignore`, comment
out, or loosen the assertions of any test. Do not add `eslint-disable`, `@ts-ignore`,
`@ts-expect-error`, `as any`, `# type: ignore`, `# noqa`, or a cast to silence an error. Do not
lower a coverage threshold, relax a lint rule, or edit CI configuration. If a check fails,
either the code is wrong or the plan is wrong — fix the code, or stop (R12).

**R7 — Add nothing unrequested.** No new dependency, script, config file, environment
variable, feature flag, generated file, or lockfile change unless a step says so explicitly.

**R8 — Match the surrounding style.** Write code in the style of the file you are editing and
of the sibling file named in **Context to read first**: same naming, same import order, same
export style, same error handling, same test structure, same language for comments and
user-facing strings. When in doubt, copy the sibling.

**R9 — Keep it minimal.** Implement exactly what the steps say and nothing more. No extra
abstraction layer, no interface with one implementation, no "while I'm here" refactor, no
defensive branches for cases the steps do not mention, no new utility for a single caller, no
commented-out code, no `TODO` comments, no leftover `console.log` / `print` / debugger.

**R10 — Security is not optional.** Re-read §8 Security invariants before writing any code
that touches authentication, authorization, input parsing, output rendering, secrets,
headers, cookies, file paths, uploads, or external calls. After such a block, confirm each
listed invariant still holds before committing.

**R11 — Commit every block.** When Verify passes: (1) set the block's row in §12 Status to
`DONE`; (2) `git add` the exact paths from **Touches** plus `PLAN.md`; (3) commit with the
exact message given in the block. One commit per block, no more, no fewer. Never `git add -A`,
`git add .`, or `git commit -a`: they sweep in files this plan never touched. The last block
is the one exception to (1) — it deletes `PLAN.md` instead of marking a row `DONE`.

**R12 — Stop conditions.** When any of the following happens: set that block's row in §12 to
`BLOCKED`, append the failing command and its **exact, unedited output** under `## Blocked`
at the end of `PLAN.md`, commit only `PLAN.md` with the message `Block <N> blocked`, and stop.
Report to the human. Do not proceed to another block. Leave the block's half-finished changes
in the working tree: do not revert them and do not commit them — the human reads them.

- Verify still fails after 2 fix attempts.
- A step names a file, symbol, command, or option that does not exist.
- Two steps contradict each other, or a step can be read in more than one way.
- The change would require touching a file outside **Touches**.
- A gate fails that §6 Baseline recorded as passing, and your change does not explain it.
- Following a step would violate §7 Architecture, §8 Security, or §9 Quality bars.

A failure that §6 Baseline already recorded is **not yours**: do not fix it, do not let it
block you, and note in the block that you saw the known-failing gate.

**R13 — The plan deletes itself last.** Never delete or shorten `PLAN.md` or `PLAN/` except in
the final block, and only after every gate in it has passed.

## 12. Status

Single source of truth for progress. The next block is the first row marked `TODO`.

The **Where** column is a literal search string: the block's own heading inside §13, or the
path of its file. Searching for it must land on the block.

| # | Block | Where | Status |
|---|---|---|---|
| 1 | Switch to 4 MB flash, two OTA partitions, app rollback and signed images | `PLAN/01-ota-config.md` | DONE |
| 2 | Add the firmware image checks to web_content, with host tests | `PLAN/02-firmware-image-checks.md` | DONE |
| 3 | Complete the OLED font with every letter, '-' and '%' | `PLAN/03-oled-font.md` | DONE |
| 4 | Add POST /firmware to web_server | `PLAN/04-firmware-route.md` | DONE |
| 5 | Confirm a new firmware after Wi-Fi and HTTP come up, and show it on the OLED | `PLAN/05-boot-check-and-oled.md` | TODO |
| 6 | Add the Firmware card to the status page | `PLAN/06-status-page.md` | TODO |
| 7 | Flash once over USB and test OTA on the board | `PLAN/07-board-tests.md` | TODO |
| 8 | Document firmware updates in the READMEs | `PLAN/08-docs.md` | TODO |
| 9 | Verify everything and remove the plan | `PLAN/09-cleanup.md` | TODO |

Statuses: `TODO` → `DONE`, or `BLOCKED` (see §11 R12).

## 13. Blocks

PLAN/01-ota-config.md
PLAN/02-firmware-image-checks.md
PLAN/03-oled-font.md
PLAN/04-firmware-route.md
PLAN/05-boot-check-and-oled.md
PLAN/06-status-page.md
PLAN/07-board-tests.md
PLAN/08-docs.md
PLAN/09-cleanup.md

## Blocked
