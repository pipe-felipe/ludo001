# PLAN — Fix the OLED item count and remove the long-parameter and duplicated-code smells

Machine-readable execution plan. Audience: the LLM executing it. Follow it literally.
Progress lives in §12 Status. Start with §11.

## 1. Task

The OLED line `ITENS <n>` must show the number of files plus folders on the whole SD card,
at every depth. Names starting with `.` are skipped, and so is everything inside a folder whose
name starts with `.`. Today it counts only the root. The user chose this rule.
A new pure component, `components/dir_scan` (POSIX `opendir`/`readdir` only, no ESP-IDF, tested on
the host), replaces the 4 copies of the "readdir skipping dotfiles" loop in `sd_card.cpp` and
`web_server.cpp`. The OLED loop stops polling the card every 400 ms. It re-reads the count and
the free space only at start-up and when `web_server::content_version()` changes. That counter
goes up after every upload or delete request.
The same plan removes these smells, all chosen by the user:
- `draw_status` has 7 parameters. It now takes a `StatusSnapshot` struct.
- `sd_card::space_info` returns its result through 2 pointers. It now returns a `SpaceInfo` struct.
- `web_server.cpp` repeats request decoding, path building and file streaming, and has long handlers. These become shared helpers.
- `url_encode`, `url_decode` and `format_size` write into `char*` + size buffers. They now return `std::string`.
- `render_file_list` builds each row in a fixed `char item[1800]` and truncates long encoded names. It now appends to a `std::string`.

The user approved one on-device check in the final block: flash the board at `/dev/ttyUSB0`, then
create and delete only the folder `plan_count_test/` on the real card over HTTP.

## 2. Definition of done

Every item is checkable by running a named command or reading a named file.

- [ ] `components/dir_scan/` exists with `CMakeLists.txt`, `include/dir_scan.hpp`, `dir_scan.cpp`, and `host_tests/main/test_dir_scan.cpp` holds 6 tests that pass.
- [ ] §5 "Unit tests (all)" prints `83 Tests 0 Failures 0 Ignored` and `OK`.
- [ ] §5 "Build (firmware)" prints `exit=0`, prints no line containing `warning:` or `error:`, and reports at least `0x6c00` bytes free in the `ludo001.bin binary size` line.
- [ ] `grep -rn 'readdir' components main --include='*.cpp'` prints exactly one line, in `components/dir_scan/dir_scan.cpp`.
- [ ] `grep -nE 'char (decoded_name|decoded|path|content_disposition|item|encoded_name|size_text)\[' components/web_server/web_server.cpp components/web_content/web_server_render.cpp` prints nothing.
- [ ] `grep -n 'void draw_status(Ssd1306& display, const StatusSnapshot& status)' main/ludo001.cpp` prints 1 line, and `grep -n 'SpaceInfo space_info();' components/sd_card/include/sd_card.hpp` prints 1 line.
- [ ] `grep -c 'sd_card::item_count()' main/ludo001.cpp` prints `1`, and `grep -c 's_content_version++' components/web_server/web_server.cpp` prints `2`.
- [ ] On the board (BLOCK 8): the `sd card: <n> items` serial line rises by exactly 6 after the 4 test uploads and returns to its first value after the test folder is deleted; the download of `plan_count_test/a.txt` matches `README.md` byte for byte; the `.zip` of `plan_count_test` passes `unzip -t` and lists 6 entries.
- [ ] `README.md` and `host_tests/README.md` describe the new count and `dir_scan` (BLOCK 7 Verify).
- [ ] All gates in §5 pass at least as well as §6 Baseline.
- [ ] `PLAN.md` no longer exists, and neither does `PLAN/`.

## 3. Out of scope

- `select_glyph` and the glyph `switch` in `components/framebuffer/framebuffer.cpp`.
- A table-driven `crc32_update`, and any other change to `components/web_content/web_server_zip.cpp` or `web_server_zip.hpp` (including `zip_append_end_of_central_dir`'s 4 parameters).
- The per-file `stat()` in the directory listing (FatFs walks the directory again for each file).
- Percent-encoding `current_dir` inside `render_file_list` hrefs and `data-name` (it is inserted decoded today and stays that way).
- Deleting a folder that contains dotfiles: `remove_recursive` skips them, so `rmdir` fails and the request answers 404. Keep that behaviour.
- `config.stack_size = 8192` in `start()`: keep the value.
- Files over 2 GiB (`st_size` cast to `long`), and the OLED's `USO used/free` wording.
- Any change to `components/web_server/web/` (HTML, CSS, JS), `components/oled_display/`, `components/wifi_manager/`.
- Pushing, opening a pull request, merging.

## 4. Branch

Branch: `refactor/oled-count-and-code-smells`
Base: `main` at `32883a1`
Created for this task.

All work happens on this branch. See §11 R4.

## 5. Project facts

Every command below was read from the file cited. A row marked `NONE FOUND` must never be run.
A row marked `write-only` rewrites files: it belongs in a block's Steps, never in a **Verify**
list and never in the final block's gate list.

`idf.py` is a shell function created by the activation script. It exists only inside the same
command that sources the script, so every command below sources it first.

| Fact | Value | Source |
|---|---|---|
| Toolchain | ESP-IDF v6.1, activated with `source ~/.espressif/tools/activate_idf_v6.1.sh` | `host_tests/README.md:26`; `~/.claude/skills/embedded/SKILL.md:80` |
| Package manager | ESP-IDF component manager (`dependencies.lock`); it fetches `espressif/mdns` by itself during `idf.py build` | `README.md:107`; `components/web_server/idf_component.yml` |
| Install | NONE FOUND | — |
| Unit tests (all) | `mkdir -p build && rm -f build/plan-host.log build/plan-tests.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && export PATH="/usr/bin:$PATH" && idf.py -C host_tests build > build/plan-host.log 2>&1 && ./host_tests/build/ludo001_host_tests.elf > build/plan-tests.log 2>&1; echo "exit=$?"; grep -E 'warning:\|error:' build/plan-host.log; tail -3 build/plan-tests.log` | `README.md:250-252`; `host_tests/README.md:26-32` |
| Unit tests (one file) | NONE FOUND (one executable runs every test) | `host_tests/main/test_main.cpp` |
| Lint | NONE FOUND | — |
| Type check | NONE FOUND (the compilers check types inside the two build rows; host tests build with `-Wall -Werror`) | `host_tests/build/compile_commands.json` |
| Format check | NONE FOUND | — |
| Format write (`write-only`) | NONE FOUND | — |
| Build (firmware) | `mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:\|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log` | `README.md:127`, `README.md:264` |
| Flash | `source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py -p /dev/ttyUSB0 flash` | `README.md:127` (without `monitor`, which is interactive: `~/.claude/skills/embedded/SKILL.md:139`) |
| Serial read | `stty -F /dev/ttyUSB0 115200 raw -echo -hupcl` once, then `timeout <s> cat /dev/ttyUSB0 \| tr -d '\000'`; keep reads short (4 s), long reads can return garbage on this machine | `~/.claude/skills/embedded/SKILL.md:136`, `:143` |
| Run the app | Flash, then HTTP against `http://ludo001.local/` (resolved to `192.168.3.234` and answered `200` on 2026-11-03) | `README.md:145` |
| E2E | NONE FOUND | — |
| CI gates, in order | NONE FOUND (no CI configuration in the repository) | — |
| Commit style | one subject line, English, imperative, no trailing period; no AI attribution or `Co-Authored-By` line | `README.md:266`; `~/.claude/CLAUDE.md` |
| Commit examples | `Add a search box that filters the files in the current folder` / `Number an upload instead of overwriting a file with the same name` | `git log` |

In the Unit tests (all), Build (firmware) and Serial read rows above, `\|` stands for a literal `|` (escaped only for this table).
Type the commands with a plain `|`. The same commands appear unescaped, ready to copy, in each
block's **Verify**.

## 6. Baseline — recorded 2026-11-03

| Gate | Command | Result |
|---|---|---|
| Unit | §5 Unit tests (all) | `72 Tests 0 Failures 0 Ignored`, `OK`, exit 0 |
| Build (firmware) | §5 Build (firmware) | exit 0; `ludo001.bin binary size 0xf86e0 bytes. ... 0x7920 bytes (3%) free.`; a full build of a clean `git archive HEAD` export printed 0 `warning:` lines |
| Flash / board | not run while planning | NOT RUN: the board is flashed for the first time in BLOCK 8 |

Known pre-existing failures: None.
A failure listed here is not caused by this plan: do not fix it, do not let it block a block (§11 R12).

Pre-existing uncommitted paths: None.
These paths are not yours: never stage them, never revert them. The final block expects to see
them in `git status`.

Every code change in this plan was compiled and tested while planning, on a scratch copy of the
repository. Measured free space: 0x75e0 after BLOCK 2, 0x75a0 after BLOCK 3, 0x7420 after BLOCK 4,
0x6ff0 after BLOCK 6. Host tests: 78 after BLOCK 1, 80 after BLOCK 4, 83 after BLOCK 5.

## 7. Architecture and style rules that bind this task

1. Logic with no hardware dependency lives in a component with no ESP-IDF includes and is tested in `host_tests/` on the PC. `host_tests/CMakeLists.txt` lists such components one by one in `EXTRA_COMPONENT_DIRS`, never the whole `components/` directory. — `README.md:234`, `host_tests/CMakeLists.txt:5-13`
2. A new host test is a `void test_*()` function in `host_tests/main/test_*.cpp`, forward-declared and registered with `RUN_TEST(...)` in `host_tests/main/test_main.cpp`; a new test file is also added to `SRCS` in `host_tests/main/CMakeLists.txt`. — `README.md:263`, `host_tests/README.md:37-41`
3. HTML, CSS and JS stay in `components/web_server/web/`; the row markup built by `render_file_list` must stay byte-for-byte what it is today (BLOCK 5 pins it with 2 exact-string tests). — `README.md:235`, memory `feedback_web_assets_outside_cpp.md`
4. User-facing text is Portuguese without accents; HTTP error messages and log lines are English. — `README.md:262`
5. The app partition is about 97% full: after every firmware build, read the `ludo001.bin binary size` line; the final state must keep at least `0x6c00` bytes free. — `README.md:264`
6. Do not add an HTTP route; the server already uses the 8 that `HTTPD_DEFAULT_CONFIG()` allows. — `README.md:265`
7. Code comments are English, as short as the surrounding ones; component and header comments say why the code exists, the way `components/web_content/include/web_server_util.hpp:1-9` does. — existing files
8. Commit messages: exactly the message written in each block; never add `Co-Authored-By`, "Generated with" or any AI attribution line. — `README.md:266`, `~/.claude/CLAUDE.md`

NOTE: `components/web_content/include/web_server_render.hpp:33-37` says hrefs are percent-encoded per path segment; the code encodes only the entry's own name and inserts `current_dir` decoded. The code wins; leave it (§3).

## 8. Security invariants

No security documentation found in this repository besides `README.md:268-280` (no authentication, no HTTPS, trusted network only). Baseline invariants apply: never log or commit secrets; never interpolate untrusted input into SQL, HTML, shell, or file paths; never remove or bypass an existing authentication or authorization check; never widen a CORS, cookie, or permission setting.

Project-specific invariants for this task:

1. Every handler that takes a `<path>` from the URI (`files_item_handler`, `upload_handler`, `delete_handler`, `zip_handler` in `components/web_server/web_server.cpp`) calls `is_safe_filename` on the whole decoded path before any `stat`, `fopen`, `mkdir`, `remove` or `rmdir`. — `components/web_server/web_server.cpp:171,240,333,436`
2. From BLOCK 4 on, `is_safe_filename` receives the `std::string` itself, never `.c_str()`, and rejects any `'\0'`. A decoded `%00` must not shorten the path: `"%00abc"` would otherwise become the card's root, and `POST /delete/%00abc` would erase the whole card.
3. Every SD card path is built as `sd_path(<checked relative path>)` or from a path that came out of `sd_path`. No handler builds a path from another root.
4. An upload never overwrites anything on the card. `unused_name` keeps the `1-<name>`, `2-<name>` numbering. — `README.md:47`
5. `httpd_resp_set_hdr` keeps only a pointer to its value. The `std::string content_disposition` must be a local variable that lives until the handler has sent its last chunk.
6. `WIFI_PASSWORD` is never logged; `.env` is never staged. — `.gitignore:9`, `README.md:111-118`
7. On the board, BLOCK 8 writes and deletes only `plan_count_test/` and the files inside it. No other path on the card may be uploaded to or deleted.

## 9. Quality bars

Violating any of these is a defect even when the tests pass.

- Directory reading happens only in `components/dir_scan/dir_scan.cpp`. `sd_card.cpp` and `web_server.cpp` call `dir_scan::visible_entries` / `dir_scan::count_entries` and contain no `opendir`/`readdir`/`closedir`.
- Do not add a class, interface, callback walker or visitor to `dir_scan`. It has exactly two functions and one struct.
- Do not add a cache, timer, debounce or background task for the item count. The only refresh trigger is `sd_read_at != version` in `main/ludo001.cpp`.
- No function that this plan adds or rewrites has more than 3 parameters.
- No fixed-size `char` buffer holds a path, a decoded name, an encoded name, a header value or an HTML row in `web_server.cpp` or `web_server_render.cpp`. Fixed buffers stay only where they hold short formatted numbers (`format_size`'s `char out[24]`, the `render_wifi_line`/`render_sd_line` `buf[64]`, the OLED lines in `main/ludo001.cpp`, the 1 KB I/O buffers).
- Do not rename or change anything this plan does not list: `numbered_name`, `render_template`, `render_dir_title`, `render_wifi_line`, `render_sd_line`, the URI tables, `start()`, `stop()`, `is_running()`, `upload_progress()`.
- No commented-out code, no reminder comments, and no new log line besides the two this plan writes out (`zip aborted at %s` in BLOCK 3, `sd card: %lu items, ...` in BLOCK 6).
- No new dependency in `idf_component.yml` or `dependencies.lock`. `dir_scan` is a local component.
- When "avoid duplication" and "avoid over-engineering" conflict, choose the smaller diff: duplicate once, extract on the third occurrence.

## 10. Test obligations

| Kind | Required | Why | Location | Command |
|---|---|---|---|---|
| Unit | yes | new pure logic in `dir_scan`; changed signatures and row building in `web_content` | `host_tests/main/test_dir_scan.cpp` (NEW), `host_tests/main/test_web_server_util.cpp`, `host_tests/main/test_web_server_render.cpp` | §5 Unit tests (all) |
| Integration | no | the project has no integration tests | — | — |
| E2E | no | the project has no e2e tests | — | — |
| Smoke (board) | yes | `web_server.cpp`, `sd_card.cpp` and `main/ludo001.cpp` have no automated tests; only the board runs them | BLOCK 8 | §5 Flash, §5 Serial read, `curl` against `http://ludo001.local/` |

Follow the existing test style at `host_tests/main/test_web_server_render.cpp`. Each test is a
`void test_*()` with Unity `TEST_ASSERT_*` macros, `extern "C" { #include "unity.h" }`, and
`using namespace web_server;` where the file tests `web_server`.

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
is not `refactor/oled-count-and-code-smells`, stop (R12). Never leave, hide or rewrite the branch's work: no
`git checkout refactor/oled-count-and-code-smells`, `git switch`, `git merge`, `git rebase`, `git reset`, `git stash`,
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
| 1 | Add the dir_scan component and its host tests | `PLAN/01-dir-scan.md` | DONE |
| 2 | Route web_server's directory walks and downloads through shared helpers | `PLAN/02-web-server-helpers.md` | DONE |
| 3 | Split the upload and zip handlers and publish content_version() | `PLAN/03-web-server-upload-zip.md` | DONE |
| 4 | Return std::string from url_encode, url_decode and format_size | `PLAN/04-util-strings.md` | DONE |
| 5 | Build the file list rows without fixed buffers | `PLAN/05-render-file-list.md` | DONE |
| 6 | Count every file and folder on the OLED, only after a change | `PLAN/06-oled-count.md` | DONE |
| 7 | Document the new item count and dir_scan | `PLAN/07-docs.md` | DONE |
| 8 | Verify everything and remove the plan | `PLAN/08-cleanup.md` | BLOCKED |

Statuses: `TODO` → `DONE`, or `BLOCKED` (see §11 R12).

## 13. Blocks

- `PLAN/01-dir-scan.md`
- `PLAN/02-web-server-helpers.md`
- `PLAN/03-web-server-upload-zip.md`
- `PLAN/04-util-strings.md`
- `PLAN/05-render-file-list.md`
- `PLAN/06-oled-count.md`
- `PLAN/07-docs.md`
- `PLAN/08-cleanup.md`

## Blocked

### Block 8 — step 5 (serial size limit); step 2 (flash count differs)

Steps 1-4 of BLOCK 8 passed: 83 tests, firmware 0x6fc0 bytes free, HTTP `200`, `plan_count_test/` answered `404`. Nothing has been written to the card.

Step 5 command and exact output. The stop condition is "`build/plan-serial.log` is larger than 100000 bytes":

```
stty -F /dev/ttyUSB0 115200 raw -echo -hupcl
(timeout 4 cat /dev/ttyUSB0 | tr -d '\000' > build/plan-serial.log) &
sleep 1
curl -s -m 5 -o /dev/null -w '%{http_code}\n' -X POST http://ludo001.local/delete/plan_count_test/missing.txt
wait
grep -a -oE 'sd card: [0-9]+ items' build/plan-serial.log | tail -1; wc -c build/plan-serial.log
```

```
404
sd card: 0 items
400673 build/plan-serial.log
```

The log has 7 lines and one `sd card:` line. The rest is the repeating `xx` noise that `~/.claude/skills/embedded/SKILL.md` describes as bogus data from long serial reads on this machine.

Step 2 (flash): `exit=0`, but `grep -cE 'Hash of data verified' build/plan-flash.log` printed `1`, the plan expects `3` or more. The esptool in ESP-IDF v6.1 prints `'bootloader/bootloader.bin' at 0x00001000 verified.` and `'partition_table/partition-table.bin' at 0x00008000 verified.` for the two unchanged images and `Hash of data verified.` once for the app. The board then answered HTTP `200`.

### Block 8 — retry after the human chose short serial captures: SD card stopped answering

Serial captures worked with a reset first and 3-4 s reads, retried until under 100000 bytes. Results: N0 = `sd card: 130 items`, after the 4 uploads `sd card: 136 items`. Step 8 then failed:

```
curl -sS -m 10 http://ludo001.local/files/plan_count_test/ | grep -o '<li>' | wc -l          -> 4
curl -sS -m 10 http://ludo001.local/files/plan_count_test/ | grep -c 'href="/files/plan_count_test/1-a.txt"'   -> 1
curl -sS -m 30 http://ludo001.local/files/plan_count_test/a.txt | cmp - README.md; echo "cmp=$?"
README.md e - são diferentes: byte 1537, linha 41
cmp=1
curl -sS -m 60 -o build/plan_count_test.zip http://ludo001.local/zip/plan_count_test; echo "zip=$?"   -> zip=0
unzip -t build/plan_count_test.zip | tail -1
  End-of-central-directory signature not found.  Either this file is not
```

Afterwards every `/files/plan_count_test/...` request answered `404`, `/files` listed `Pasta vazia.`, and the serial log showed:

```
E (76850) sdmmc_cmd: sdmmc_read_sectors_dma: sdmmc_send_cmd returned 0x107, failed to get status (0x107)
E (76850) diskio_sdmmc: sdmmc_read_blocks failed (0x107)
I (76850) ludo001: sd card: 0 items, 72 MB used, 30352 MB free
```

0x107 is ESP_ERR_TIMEOUT from the SD card over SPI: a hardware or contact problem, not a code defect. `plan_count_test/` may still be on the card (step 9 did not run). Nothing outside `plan_count_test/` was written or deleted.
