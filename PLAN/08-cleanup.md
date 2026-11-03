## BLOCK 8 — Verify everything and remove the plan

**Depends on:** BLOCK 7 committed
**Touches:** `PLAN.md` (deleted), `PLAN/` (deleted)

### Goal
Prove the task is complete against §2, on the host and on the board, then remove the plan from the repository.

### Context to read first
1. §2 Definition of done: the list you are about to check.
2. §6 Baseline: the numbers you compare against.
3. §8 Security invariant 7: on the card, only `plan_count_test/` may be written or deleted.

### Steps
1. Run both §5 gates (the project has no CI, lint or formatter):
   ```
   mkdir -p build && rm -f build/plan-host.log build/plan-tests.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && export PATH="/usr/bin:$PATH" && idf.py -C host_tests build > build/plan-host.log 2>&1 && ./host_tests/build/ludo001_host_tests.elf > build/plan-tests.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-host.log; tail -3 build/plan-tests.log
   mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log
   ```
   Expected:
   - First command: `exit=0`, no `warning:`/`error:` line, `83 Tests 0 Failures 0 Ignored`, `OK`.
   - Second command: `exit=0`, no `warning:`/`error:` line, and a `ludo001.bin binary size` line whose free-bytes value is at least `0x6c00`.
2. Flash the board (the user approved this while planning; §1):
   ```
   source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py -p /dev/ttyUSB0 flash > build/plan-flash.log 2>&1; echo "exit=$?"; grep -cE 'Hash of data verified' build/plan-flash.log
   ```
   Expected: `exit=0` and a count of `3` or more. If the port is busy or missing, the command fails. That is a stop condition (§11 R12).
3. Wait for the board to serve HTTP again (it reboots after flashing and reconnects to Wi-Fi):
   ```
   for i in $(seq 1 40); do curl -s -m 2 -o /dev/null http://ludo001.local/ && break; sleep 1; done; curl -sS -m 5 -o /dev/null -w '%{http_code}\n' http://ludo001.local/
   ```
   Expected: `200`.
4. Confirm the test folder does not exist yet:
   ```
   curl -s -m 5 -o /dev/null -w '%{http_code}\n' http://ludo001.local/files/plan_count_test/
   ```
   Expected: `404`. Any other code means `plan_count_test` already exists on the card: stop (§11 R12) and do not delete it.
5. Read the starting count. The command captures the serial log for 4 s while it triggers one refresh with a delete of a path that does not exist (it answers `404` and bumps `content_version()`):
   ```
   stty -F /dev/ttyUSB0 115200 raw -echo -hupcl
   (timeout 4 cat /dev/ttyUSB0 | tr -d '\000' > build/plan-serial.log) &
   sleep 1
   curl -s -m 5 -o /dev/null -w '%{http_code}\n' -X POST http://ludo001.local/delete/plan_count_test/missing.txt
   wait
   grep -a -oE 'sd card: [0-9]+ items' build/plan-serial.log | tail -1
   ```
   Expected: `404`, then one line `sd card: <N0> items`. Write N0 down; the next steps compare against it. If the `grep` prints nothing, run this whole step again once. If it still prints nothing, or `build/plan-serial.log` is larger than 100000 bytes (`wc -c build/plan-serial.log`), stop (§11 R12). The embedded skill warns that long serial reads on this machine can return garbage.
6. Upload 3 files, using `README.md` as content:
   ```
   curl -sS -m 30 -X POST --data-binary @README.md http://ludo001.local/upload/plan_count_test/a.txt; echo
   curl -sS -m 30 -X POST --data-binary @README.md http://ludo001.local/upload/plan_count_test/b.txt; echo
   curl -sS -m 30 -X POST --data-binary @README.md http://ludo001.local/upload/plan_count_test/sub/c.txt; echo
   ```
   Expected: three lines, each `OK`.
7. Upload `a.txt` a fourth time while capturing the log. The upload must not overwrite, so the server stores it as `1-a.txt`:
   ```
   stty -F /dev/ttyUSB0 115200 raw -echo -hupcl
   (timeout 4 cat /dev/ttyUSB0 | tr -d '\000' > build/plan-serial.log) &
   sleep 1
   curl -sS -m 30 -X POST --data-binary @README.md http://ludo001.local/upload/plan_count_test/a.txt; echo
   wait
   grep -a -oE 'sd card: [0-9]+ items' build/plan-serial.log | tail -1
   ```
   Expected: `OK`, then `sd card: <N0 + 6> items` (6 new items: `plan_count_test`, `a.txt`, `b.txt`, `sub`, `sub/c.txt`, `1-a.txt`). If the `grep` prints nothing, run the step 5 command block once more (its `404` delete triggers a refresh without changing the card) and expect `sd card: <N0 + 6> items` from it.
8. Check the listing, a download and the `.zip`:
   ```
   curl -sS -m 10 http://ludo001.local/files/plan_count_test/ | grep -o '<li>' | wc -l
   curl -sS -m 10 http://ludo001.local/files/plan_count_test/ | grep -c 'href="/files/plan_count_test/1-a.txt"'
   curl -sS -m 30 http://ludo001.local/files/plan_count_test/a.txt | cmp - README.md; echo "cmp=$?"
   curl -sS -m 60 -o build/plan_count_test.zip http://ludo001.local/zip/plan_count_test; echo "zip=$?"
   unzip -t build/plan_count_test.zip | tail -1
   unzip -Z1 build/plan_count_test.zip | sort
   ```
   Expected, in order: `4`; `1`; `cmp=0`; `zip=0`; `No errors detected in compressed data of build/plan_count_test.zip.`; these 6 lines:
   ```
   plan_count_test/
   plan_count_test/1-a.txt
   plan_count_test/a.txt
   plan_count_test/b.txt
   plan_count_test/sub/
   plan_count_test/sub/c.txt
   ```
9. Delete the test folder while capturing the log:
   ```
   stty -F /dev/ttyUSB0 115200 raw -echo -hupcl
   (timeout 4 cat /dev/ttyUSB0 | tr -d '\000' > build/plan-serial.log) &
   sleep 1
   curl -sS -m 30 -X POST http://ludo001.local/delete/plan_count_test; echo
   wait
   grep -a -oE 'sd card: [0-9]+ items' build/plan-serial.log | tail -1
   curl -s -m 5 -o /dev/null -w '%{http_code}\n' http://ludo001.local/files/plan_count_test/
   ```
   Expected: `OK`, then `sd card: <N0> items`, then `404`. If the `grep` prints nothing, run the step 5 command block once more and expect `404` and `sd card: <N0> items` from it.
10. Remove the local test artifacts (all under the git-ignored `build/`):
    ```
    rm -f build/plan_count_test.zip build/plan-serial.log build/plan-flash.log build/plan-fw.log build/plan-host.log build/plan-tests.log
    ```
11. Check each `- [ ]` item in §2 by running the command named in it. The on-board item is covered by steps 5-9 above. Every item must pass, except the last (`PLAN.md` no longer exists), which step 12 produces.
12. Only if steps 1-11 all passed:
    ```
    git rm -q PLAN.md
    git rm -rq PLAN
    ```

### Do not
- Do not delete the plan if any check failed. That is a stop condition (§11 R12).
- Do not upload to or delete any path on the card other than `plan_count_test/...` (§8 invariant 7). If step 9 fails, leave the folder and report it; do not try other delete paths.
- Do not run `idf.py monitor`. It needs an interactive terminal.
- Do not fix a failure that §6 Baseline already recorded.
- Do not push, tag, or open a pull request.

### Verify
```
git status --porcelain
ls PLAN.md PLAN 2>&1
```
Expected: `git status --porcelain` lists staged deletions (lines starting with `D `) for
`PLAN.md` and every `PLAN/` file, and nothing else; `ls` reports "No such file or directory"
for both paths.

A modified file that is neither of those means a gate rewrote it, or a block left work
behind. That is a stop condition.

### If verification fails
Follow §11 R12. Restore the plan with `git checkout -- PLAN.md PLAN` if it was already removed.

### Commit
`git rm` already staged both deletions, so nothing needs adding:
```
git commit -m "Complete the OLED item count fix and remove PLAN.md"
```
Do not run `git add -A` or `git commit -a` here.

### Next
Nothing. The plan is finished. Report to the human:
- The branch name `refactor/oled-count-and-code-smells` and the list of commits (`git log --oneline 32883a1..HEAD`).
- Every gate result from step 1, including the free-bytes value.
- N0, N0 + 6 and the final count from steps 5, 7 and 9.
- This request: look at the OLED and confirm that it shows `ITENS <N0>` now that the test folder is gone. Nobody has seen the display yet; the serial log is the only automated check of it.
