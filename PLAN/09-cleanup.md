## BLOCK 9 — Verify the whole task and remove the plan

**Depends on:** BLOCK 8 committed
**Touches:** `PLAN.md` (deleted), `PLAN/` (deleted)

### Goal
Prove the task is complete against §2, including one last update over the network with the board away from the computer, then remove the plan from the repository.

### Context to read first
1. §2 Definition of done: the list you are about to check.
2. §6 Baseline: the numbers you compare against.
3. §8 Security invariants 2 and 3.

### Steps
1. Run every local gate in §5, in this order. Run each build with a 600000 ms timeout.
   ```
   source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; export PATH="/usr/bin:$PATH"; idf.py -C host_tests build > build/host-tests-build.log 2>&1; echo "host build exit=$?"; ./host_tests/build/ludo001_host_tests.elf | tail -3
   source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py build > build/fw-build.log 2>&1; echo "fw build exit=$?"; grep -E 'warning:|binary size' build/fw-build.log
   node --check components/web_server/web/app.js
   ```
   Expected:
   - `host build exit=0`, `95 Tests 0 Failures 0 Ignored`, `OK`.
   - `fw build exit=0` and no `warning:` line. The `ludo001.bin binary size ...` line shows `Smallest app partition is 0x1a9000 bytes` and at least `0x90000` bytes free. Keep both `binary size` lines for the report.
   - `node --check` exits 0 with no output.
2. Run the configuration and key checks from §2:
   ```
   grep -cE '^CONFIG_' sdkconfig.defaults
   source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py partition-table 2>&1 | grep -E '^(nvs|otadata|phy_init|ota_0|ota_1),'
   test -e secure_boot_signing_key.pem && echo "key present"
   git check-ignore -q secure_boot_signing_key.pem && echo IGNORED
   git ls-files secure_boot_signing_key.pem | wc -l
   ```
   Expected:
   - `4`.
   - The 5 partition rows: `nvs,data,nvs,0x9000,24K,`, `otadata,data,ota,0xf000,8K,`, `phy_init,data,phy,0x11000,4K,`, `ota_0,app,ota_0,0x20000,1700K,`, `ota_1,app,ota_1,0x1d0000,1700K,`.
   - `key present`, `IGNORED`, `0`.
3. Run the app: update the board over the network with the build from step 1. The board is on the HW-131 supply, where BLOCK 7 left it. Run `python3 -c "import json; print(json.load(open('build/project_description.json'))['project_version'])"` and call the printed version **F**. Then:
   ```
   mkdir -p build/ota-test
   curl -s -m 10 -o /dev/null -w 'status=%{http_code}\n' http://ludo001.local/
   curl -s -m 180 -o build/ota-test/final.out -w 'code=%{http_code} time=%{time_total}\n' -X POST --data-binary @build/ludo001.bin http://ludo001.local/firmware; cat build/ota-test/final.out; echo
   for i in $(seq 1 45); do code=$(curl -s -m 3 -o /dev/null -w '%{http_code}' http://ludo001.local/); [ "$code" = 200 ] && break; sleep 2; done; echo "status=$code"; curl -s -m 10 http://ludo001.local/ | grep -o 'Versao: [^<]*'
   ```
   Expected:
   - `status=200`.
   - `code=200` and `OK`. Keep the `time=` value for the report.
   - `status=200` and `Versao: F`.
4. Check each `- [ ]` item in §2. The items about the board (cable flash, T1–T9, OLED) passed in BLOCK 7, which is `DONE` in §12. Every item must pass.
5. Only if steps 1–4 all passed:
   ```
   git rm -q PLAN.md
   git rm -rq PLAN
   ```

### Do not
- Do not delete the plan if any check failed — that is a stop condition (§11 R12).
- Do not fix a failure that §6 Baseline already recorded.
- Do not push, tag, or open a pull request.
- Do not run C-FLASH; this block updates the board over the network only.
- Do not read or edit the Notion card (§3).

### Verify
```
git status --porcelain
ls PLAN.md PLAN 2>&1
```
Expected: `git status --porcelain` lists staged deletions (lines starting with `D `) for `PLAN.md` and the 9 `PLAN/` files, and nothing else. `ls` reports, for both paths, that they do not exist. On this machine the message is in Portuguese: `Arquivo ou diretório inexistente`.

A modified file that is neither of those means a gate rewrote it or a block left work behind: that is a stop condition.

### If verification fails
Follow §11 R12. Restore the plan with `git checkout -- PLAN.md PLAN` if it was already removed.

### Commit
`git rm` already staged both deletions, so nothing needs adding:
```
git commit -m "Complete OTA firmware updates and remove PLAN.md"
```
Do not run `git add -A` or `git commit -a` here.

### Next
Nothing. The plan is finished. Report to the human, in Portuguese:
- the branch name `feat/ota-firmware-update`;
- the list of commits (`git log --oneline 514d68f..HEAD`);
- every gate result from step 1;
- the measurements the Notion card asks for:
  - before: `0xf9040` bytes, `0x6fc0` free, from §6;
  - after: both `binary size` lines from step 1;
  - update time: the `time=` from step 3 and from BLOCK 7 T1, if you still have it;
- this reminder: "Guarde uma cópia de `secure_boot_signing_key.pem` fora deste computador. Sem ela, só dá para atualizar pelo cabo, com uma chave nova. O card do Notion não foi alterado."
