## BLOCK 6 — Add the Firmware card to the status page

**Depends on:** BLOCK 5 committed
**Touches:** `components/web_server/web/index.html` (MODIFY), `components/web_server/web/app.js` (MODIFY), `components/web_server/web_server.cpp` (MODIFY)

### Goal
`GET /` shows `Versao: <running version>` and a "Firmware" card. The card sends the chosen `.bin` to `POST /firmware`, shows "Reiniciando..." after the `OK`, and reloads the page once the board answers again.

### Context to read first
1. `components/web_server/web/index.html`: the whole file (20 lines).
2. `components/web_server/web/files.html:20-45`: the upload card markup to mirror (`.upload`, `.drop`, `.drop-icon`, `.drop-title`, `.drop-hint`, `.btn wide`, `#progress`/`#fill`, `#status`).
3. `components/web_server/web/app.js`: the whole file (163 lines). Reuse `setStatus`, `setProgress` and `picked`. Note that the bottom IIFE assumes the files page's `#f` and `#d` inputs exist.
4. `components/web_server/web_server.cpp`: `root_handler`, which starts at the line `esp_err_t root_handler(httpd_req_t* req) {`.
5. §7 rules 2 and 3: front-end stays in `web/`, and UI text is Portuguese without accents.

Read exactly these. Do not open other files unless a step below names one.

### Steps
1. In `components/web_server/web/index.html`, replace
   ```html
           <p><a class="btn" href="/files">Ver arquivos</a></p>
       </div>
   </body>
   ```
   with the block below. `{{FW_VERSION}}` is a literal template token: type it exactly.
   ```html
           <p><a class="btn" href="/files">Ver arquivos</a></p>
       </div>
       <div class="card">
           <h2>Firmware</h2>
           <p>Versao: {{FW_VERSION}}</p>
           <div class="upload">
               <label class="drop">
                   <input type="file" id="fw" class="visually-hidden" accept=".bin" onchange="picked(this)">
                   <span class="drop-icon"></span>
                   <span class="drop-title">Escolher firmware</span>
                   <span class="drop-hint">Toque para escolher o ludo001.bin assinado</span>
               </label>
               <button class="btn wide" onclick="upFirmware()" disabled>Atualizar firmware</button>
           </div>
           <div class="progress" id="progress" hidden>
               <div class="progress-fill" id="fill"></div>
           </div>
           <p id="status"></p>
       </div>
       <script src="/app.js"></script>
   </body>
   ```
2. In `components/web_server/web/app.js`, immediately before the line `// Lowercase and strip accents, so "relatorio" finds "Relatório".`, insert this block followed by one blank line:
   ```js
   // Sends the chosen .bin to POST /firmware. The board answers before it
   // restarts into the new firmware; waitForRestart() then reloads the page
   // once the board is back, showing the version that is running.
   function upFirmware() {
       var bin = document.getElementById('fw').files[0];
       if (!bin) { return; }
       var button = document.querySelector('.upload button');
       button.disabled = true;
       setProgress(null);
       setStatus('enviando ' + bin.name + '...');
       fetch('/firmware', { method: 'POST', body: bin })
           .then(function (r) {
               return r.text().then(function (text) {
                   if (!r.ok) { throw new Error('falhou (' + r.status + '): ' + text); }
                   setProgress(100);
                   setStatus('Reiniciando...');
                   waitForRestart(30);
               });
           })
           .catch(function (e) {
               document.getElementById('progress').hidden = true;
               setStatus('erro: ' + e.message, true);
               button.disabled = false;
           });
   }

   // Asks for the status page every 2 s and reloads once the board answers;
   // gives up after `tries` attempts.
   function waitForRestart(tries) {
       setTimeout(function () {
           fetch('/', { cache: 'no-store' })
               .then(function (r) {
                   if (!r.ok) { throw new Error('HTTP ' + r.status); }
                   location.reload();
               })
               .catch(function () {
                   if (tries > 1) {
                       waitForRestart(tries - 1);
                   } else {
                       setStatus('a placa nao voltou; recarregue a pagina', true);
                   }
               });
       }, 2000);
   }
   ```
3. In the same file, replace
   ```js
   (function () {
       var input = document.getElementById('f');
       var drop = input.parentNode;
   ```
   with
   ```js
   (function () {
       var input = document.getElementById('f');
       if (!input) { return; } // the status page has only the firmware box
       var drop = input.parentNode;
   ```
4. In `components/web_server/web_server.cpp`, inside `root_handler`, replace
   ```cpp
               {"SD_LINE", render_sd_line(sd_mounted, sd_card::capacity_mb())},
           });
   ```
   with
   ```cpp
               {"SD_LINE", render_sd_line(sd_mounted, sd_card::capacity_mb())},
               {"FW_VERSION", esp_app_get_description()->version},
           });
   ```

### Do not
- Do not add a new JS file, a new route or inline `<script>` code (§9). `index.html` loads `/app.js`.
- Do not reuse `fail()` or `setBusy()` from `app.js` for the firmware card: both call `picked()` on `#f`/`#d`, which do not exist on the status page.
- Do not add CSS; the card reuses the existing classes.
- Do not use accented letters in the new UI strings (§7 rule 3).
- Do not change `up()`, `upDir()`, `del()` or `filterList()`.

### Verify
Run from the repository root, in this order:
```
node --check components/web_server/web/app.js
grep -c 'FW_VERSION' components/web_server/web/index.html components/web_server/web_server.cpp
grep -n "if (!input) { return; }" components/web_server/web/app.js
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py build > build/fw-build.log 2>&1; echo "fw build exit=$?"; grep -E 'warning:|binary size' build/fw-build.log
```
Expected:
1. Exit 0 with no output.
2. `components/web_server/web/index.html:1` and `components/web_server/web_server.cpp:1`.
3. 1 line.
4. `fw build exit=0` and no `warning:` line. The `ludo001.bin binary size ...` line shows partition `0x1a9000` with at least `0x90000` bytes free.

### If verification fails
1. Read the failing output in full.
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set BLOCK 6's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/web_server/web/index.html components/web_server/web/app.js components/web_server/web_server.cpp PLAN.md
   git commit -m "Add a Firmware card to the status page"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
