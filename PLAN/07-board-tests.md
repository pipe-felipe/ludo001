## BLOCK 7 — Flash once over USB and test OTA on the board

**Depends on:** BLOCK 6 committed
**Touches:** `PLAN.md` (status row only). Scratch output goes under `build/ota-test/`, which is gitignored. This block changes no source file.

### Goal
The board runs the BLOCK 6 firmware from `ota_0` after one cable flash, with its SD files intact. On the HW-131 supply it passes the update matrix T1–T9: normal update, interrupted upload, wrong project, unsigned image, wrong key, truncated image, 413, 409, and rollback.

### Context to read first
1. §8 Security invariants 1, 2, 3 and 8.
2. §5 C-BUILD and C-FLASH.
3. `README.md:122-147`: flashing, the expected log lines, and the board address.

Read exactly these. Do not open other files unless a step below names one.

How this block runs:
- It talks to the human three times (steps 4, 9 and 13). Send each message exactly as written, in Portuguese, then end your turn and wait for the reply.
- Every **Expected** under a step is a pass condition. A mismatch is a verification failure (see **If verification fails**).
- Run every command that takes longer than 2 minutes with a 600000 ms timeout. That covers builds, flashing, and every wait loop.
- `WAIT-UP` below is the wait loop. Wherever a step says "run WAIT-UP", run this exact command:
  ```
  for i in $(seq 1 45); do code=$(curl -s -m 3 -o /dev/null -w '%{http_code}' http://ludo001.local/); [ "$code" = 200 ] && break; sleep 2; done; echo "status=$code"; curl -s -m 10 http://ludo001.local/ | grep -o 'Versao: [^<]*'
  ```
  Its expected output is `status=200` followed by the line `Versao: <version>` that the step names.

### Steps
1. Run `mkdir -p build/ota-test`, then C-BUILD. Expected: `fw build exit=0`, no `warning:` line.
2. Run `python3 -c "import json; print(json.load(open('build/project_description.json'))['project_version'])"`. It prints the version under test; call it **V**. **V** is 7 or more hexadecimal characters with no `-dirty` suffix. If it ends in `-dirty`, the tree is not clean: stop (§11 R12).
3. With the board still on the HW-131 supply and running the old firmware, run:
   ```
   curl -s -m 10 -o /dev/null -w 'status=%{http_code}\n' http://ludo001.local/
   curl -s -m 30 http://ludo001.local/files | grep -o '<li>' | wc -l
   ```
   Expected: `status=200`, then a number. Call that number **N**: it is the root item count before the flash.
4. Send the human exactly this message and wait for the reply:
   > Preciso gravar o firmware pelo cabo uma vez (bootloader e tabela de partições novos). Desligue a placa da fonte HW-131 e ligue-a no computador pelo cabo USB — nunca as duas alimentações ao mesmo tempo. Responda "pronto" quando ela estiver ligada só pelo USB.

   Any reply other than "pronto" (in any letter case) means stop (§11 R12). Record the reply under `## Blocked`.
5. Run:
   ```
   ls -l /dev/ttyUSB0
   source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; python -m esptool -p /dev/ttyUSB0 chip-id 2>&1 | grep -c '68:09:47:9d:48:bc'
   ```
   Expected: the device exists, and the count is at least `1` (the MAC of the user's ESP32-WROOM-32E board). A count of `0` means another board is connected: stop (§11 R12).
6. Run C-FLASH, then `grep -c '0x00020000' build/ota-test/flash.log`. Expected: `flash exit=0`, and a count of at least `1`.
7. Capture the boot log. The script resets the board through the USB-serial lines and reads 12 seconds of output:
   ```
   source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; python - <<'EOF'
   import serial, time
   port = serial.Serial()
   port.port = '/dev/ttyUSB0'
   port.baudrate = 115200
   port.timeout = 0.2
   port.dtr = False  # GPIO0 stays high: normal boot, not download mode
   port.rts = False
   port.open()
   port.rts = True   # EN low: hold the chip in reset
   time.sleep(0.1)
   port.reset_input_buffer()
   port.rts = False  # release EN: the board boots
   data = b''
   end = time.time() + 12
   while time.time() < end:
       data += port.read(4096)
   port.close()
   open('build/ota-test/boot.log', 'wb').write(data.replace(b'\x00', b''))
   print(len(data), 'bytes')
   EOF
   grep -ac 'Loaded app from partition at offset 0x20000' build/ota-test/boot.log
   grep -a 'running from ota_0' build/ota-test/boot.log
   ```
   Expected: fewer than 100000 bytes; `1`; one line that ends with `firmware V running from ota_0` (V from step 2). Wi-Fi is checked in step 10, over HTTP.
   A capture of 100000 bytes or more is a known artifact of this machine's serial port (megabytes of `x` characters), not a board fault. Re-run the whole command, up to 3 attempts in total. Only after 3 such captures, stop (§11 R12).
8. Run `rm -f build/ota-test/boot.log`.
9. Send the human exactly this message and wait for the reply:
   > Gravação pelo cabo feita. Desligue o cabo USB e ligue a placa de volta na fonte HW-131. Responda "pronto" quando ela estiver ligada só na fonte.

   Any reply other than "pronto" means stop (§11 R12).
10. Run WAIT-UP. Expected: `status=200`, `Versao: V`. Then run `curl -s -m 30 http://ludo001.local/files | grep -o '<li>' | wc -l`. Expected: exactly **N** (step 3).
11. Prepare the test images:
    ```
    source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py -B build/ota-test/v2 -DPROJECT_VER=ota-test-2 build > build/ota-test/v2-build.log 2>&1; echo "v2 build exit=$?"; ls -l build/ota-test/v2/ludo001.bin
    head -c 500000 build/ludo001.bin > build/ota-test/truncated.bin
    head -c 1800000 /dev/zero > build/ota-test/too-large.bin
    source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; python -m espsecure generate-signing-key --version 1 --scheme ecdsa256 build/ota-test/other_key.pem
    source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; python -m espsecure sign-data --version 1 --keyfile build/ota-test/other_key.pem --output build/ota-test/other-key.bin build/ludo001-unsigned.bin
    ls -l /home/ludo/Projects/esp-idf-hello/build/esp-idf-hello.bin build/ludo001-unsigned.bin build/ota-test/other-key.bin
    ```
    Expected: `v2 build exit=0` (a full build in a new directory), and every listed file exists.
12. Build the firmware with no Wi-Fi credentials. It comes from a `git archive` export, which has no `.env`. The signing key is copied in only for this build and deleted right after:
    ```
    rm -rf build/ota-test/nowifi && mkdir -p build/ota-test/nowifi && git archive HEAD | tar -x -C build/ota-test/nowifi && cp secure_boot_signing_key.pem build/ota-test/nowifi/ && test ! -e build/ota-test/nowifi/.env && echo "no .env"
    source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py -C build/ota-test/nowifi build > build/ota-test/nowifi-build.log 2>&1; echo "nowifi build exit=$?"; rm -f build/ota-test/nowifi/secure_boot_signing_key.pem; ls -l build/ota-test/nowifi/build/ludo001.bin
    ```
    Expected: `no .env`, `nowifi build exit=0`, and the `.bin` exists.
13. Send the human exactly this message and wait for the reply:
    > Vou mandar um firmware de teste pela rede agora. Olhe o OLED durante o envio (leva até um minuto): a linha do meio deve mostrar ATUALIZANDO e o percentual subindo. Responda "ok" para eu começar.

    Then run T1:
    ```
    curl -s -m 180 -o build/ota-test/t1.out -w 'code=%{http_code} time=%{time_total}\n' -X POST --data-binary @build/ota-test/v2/ludo001.bin http://ludo001.local/firmware; cat build/ota-test/t1.out; echo
    ```
    Expected: `code=200`, then `OK`. Note the `time=` value for the final report.
    Run WAIT-UP. Expected: `status=200`, `Versao: ota-test-2`.
    Then send the human exactly this message and wait for the reply:
    > O OLED mostrou ATUALIZANDO com o percentual subindo durante o envio? E a última linha da tela agora mostra FW OTA-TEST-2? Responda "sim" ou "não" para cada pergunta.

    Two "sim" answers are the pass condition. Any "não" means stop (§11 R12). Record the human's answer under `## Blocked`.
14. Go back to V:
    ```
    curl -s -m 180 -o build/ota-test/back.out -w 'code=%{http_code}\n' -X POST --data-binary @build/ludo001.bin http://ludo001.local/firmware; cat build/ota-test/back.out; echo
    ```
    Expected: `code=200`, `OK`. Run WAIT-UP. Expected: `Versao: V`.
15. T2, interrupted upload. curl gives up after 5 s, mid-body:
    ```
    curl -s -m 5 --limit-rate 50k -o /dev/null -w 'code=%{http_code}\n' -X POST --data-binary @build/ota-test/v2/ludo001.bin http://ludo001.local/firmware; echo "curl exit=$?"; sleep 8
    ```
    Expected: `code=000`, `curl exit=28`. Run WAIT-UP. Expected: `Versao: V`.
16. T3–T7, rejections. Run each line. Each prints its HTTP code and then the response body:
    ```
    curl -s -m 120 -o build/ota-test/r.out -w 'T3 code=%{http_code} ' -X POST --data-binary @/home/ludo/Projects/esp-idf-hello/build/esp-idf-hello.bin http://ludo001.local/firmware; cat build/ota-test/r.out; echo
    curl -s -m 180 -o build/ota-test/r.out -w 'T4 code=%{http_code} ' -X POST --data-binary @build/ludo001-unsigned.bin http://ludo001.local/firmware; cat build/ota-test/r.out; echo
    curl -s -m 180 -o build/ota-test/r.out -w 'T5 code=%{http_code} ' -X POST --data-binary @build/ota-test/other-key.bin http://ludo001.local/firmware; cat build/ota-test/r.out; echo
    curl -s -m 180 -o build/ota-test/r.out -w 'T6 code=%{http_code} ' -X POST --data-binary @build/ota-test/truncated.bin http://ludo001.local/firmware; cat build/ota-test/r.out; echo
    curl -s -m 120 -o build/ota-test/r.out -w 'T7a code=%{http_code} ' -X POST --data-binary @build/ota-test/too-large.bin http://ludo001.local/firmware; cat build/ota-test/r.out; echo
    curl -s -m 30 -o build/ota-test/r.out -w 'T7b code=%{http_code} ' -X POST --data-binary '' http://ludo001.local/firmware; cat build/ota-test/r.out; echo
    ```
    Expected, one line each:
    - `T3 code=400 not a ludo001 firmware image`
    - `T4 code=400 firmware image truncated, corrupt or not signed with this board's key`
    - `T5 code=400 firmware image truncated, corrupt or not signed with this board's key`
    - `T6 code=400 firmware image truncated, corrupt or not signed with this board's key`
    - `T7a code=413 firmware image empty or larger than the update partition`
    - `T7b code=413 firmware image empty or larger than the update partition`

    Then run WAIT-UP. Expected: `Versao: V`. None of these may restart the board.
17. T8, requests that arrive while an accepted update waits for its restart. Request A uploads slowly. B1 (firmware, empty body) and B2 (a file upload) queue behind it, and the board answers them after A, inside the 1 s before the restart:
    ```
    (curl -s -m 180 --limit-rate 100k -o build/ota-test/a.out -w 'A code=%{http_code}\n' -X POST --data-binary @build/ota-test/v2/ludo001.bin http://ludo001.local/firmware > build/ota-test/a.txt) &
    sleep 3
    (curl -s -m 180 -o build/ota-test/b1.out -w 'B1 code=%{http_code}\n' -X POST --data-binary '' http://ludo001.local/firmware > build/ota-test/b1.txt) &
    (curl -s -m 180 -o build/ota-test/b2.out -w 'B2 code=%{http_code}\n' -X POST --data-binary 'x' http://ludo001.local/upload/plan_ota_test.txt > build/ota-test/b2.txt) &
    wait
    cat build/ota-test/a.txt build/ota-test/a.out; echo; cat build/ota-test/b1.txt build/ota-test/b1.out; echo; cat build/ota-test/b2.txt build/ota-test/b2.out; echo
    ```
    Expected: `A code=200` with `OK`; `B1 code=409` and `B2 code=409`, each with `firmware update in progress, the board is restarting`.
    If B1 or B2 shows `code=000`, first run WAIT-UP and step 14, then re-run this step once. If it fails a second time, that is a verification failure.
    Run WAIT-UP. Expected: `Versao: ota-test-2`. Then run `curl -s -m 10 -o /dev/null -w 'code=%{http_code}\n' http://ludo001.local/files/plan_ota_test.txt`. Expected: `code=404`, meaning B2 created nothing. If it prints `code=200`, stop (§11 R12) and leave the file on the card for the human.
    Run step 14 again to go back to V.
18. T9, rollback. Send the firmware that has no Wi-Fi credentials:
    ```
    curl -s -m 180 -o build/ota-test/r.out -w 'T9 code=%{http_code} ' -X POST --data-binary @build/ota-test/nowifi/build/ludo001.bin http://ludo001.local/firmware; cat build/ota-test/r.out; echo; sleep 30
    ```
    Expected: `T9 code=200 OK`. The board restarts into a firmware that cannot join Wi-Fi. After 60 s it marks itself invalid, and the bootloader goes back to V.
    Run `curl -s -m 5 -o /dev/null -w 'status=%{http_code}\n' http://ludo001.local/`. Expected: `status=000`. This proves the board really is running the firmware with no Wi-Fi.
    Run WAIT-UP twice in a row, so the wait covers about 7 minutes in total. The first run may end with a `status=` other than 200. That is fine as long as the second run ends with `status=200`.
    Expected after the second run: `status=200`, `Versao: V`.
19. Run `curl -s -m 30 http://ludo001.local/files | grep -o '<li>' | wc -l`. Expected: exactly **N**.
20. Delete the scratch files that hold key material and the export:
    ```
    rm -rf build/ota-test/nowifi build/ota-test/other_key.pem
    test ! -e build/ota-test/nowifi && test ! -e build/ota-test/other_key.pem && echo "scratch removed"
    ```
    Expected: `scratch removed`.

### Do not
- Do not edit any source file in this block. A failing test is a stop condition (§11 R12), never a fix. Every fix would touch files outside **Touches**.
- Do not run C-FLASH more than once, and do not run `idf.py erase-flash`, `espefuse` or `monitor` (§8 invariant 1).
- Do not print `.env`, `secure_boot_signing_key.pem`, `build/ota-test/other_key.pem` or any boot-log line other than the two greps in step 7 (§8 invariants 2 and 3).
- Do not move on while the human hasn't answered a message from steps 4, 9 or 13.
- Do not delete anything on the SD card. The only card path this block may create, `plan_ota_test.txt`, must not get created (T8).

### Verify
Run from the repository root, in this order:
```
for i in $(seq 1 45); do code=$(curl -s -m 3 -o /dev/null -w '%{http_code}' http://ludo001.local/); [ "$code" = 200 ] && break; sleep 2; done; echo "status=$code"; curl -s -m 10 http://ludo001.local/ | grep -o 'Versao: [^<]*'
curl -s -m 30 http://ludo001.local/files | grep -o '<li>' | wc -l
git status --porcelain
```
Expected: `status=200` and `Versao: V`; the count **N**; empty `git status` output (everything this block wrote is under the ignored `build/`). Every **Expected** in steps 1–20 must also have passed.

### If verification fails
1. Read the failing output in full. For a build failure, read `build/fw-build.log`, `build/ota-test/v2-build.log` or `build/ota-test/nowifi-build.log`. For a flash failure, read `build/ota-test/flash.log`.
2. Retry only what this block allows: the serial capture (step 7, up to 3 attempts) and T8 (step 17, once). Everything else is not retried.
3. Fix nothing: this block's **Touches** has no source file.
4. Follow §11 R12: mark this block `BLOCKED` in §12, append the failing command and its exact output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop. Before stopping, run `rm -rf build/ota-test/nowifi build/ota-test/other_key.pem` so no copy of key material stays behind. Tell the human which firmware the board is running: the last `Versao:` line you saw.

### Commit
1. Set BLOCK 7's row in §12 Status to `DONE`.
2. Run:
   ```
   git add PLAN.md
   git commit -m "Verify OTA updates, signature checks and rollback on the board"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
