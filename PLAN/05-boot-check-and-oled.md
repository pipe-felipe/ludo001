## BLOCK 5 — Confirm a new firmware after Wi-Fi and HTTP come up, and show it on the OLED

**Depends on:** BLOCK 4 committed
**Touches:** `main/ludo001.cpp` (MODIFY), `main/CMakeLists.txt` (MODIFY)

### Goal
A firmware fresh from an OTA update marks itself valid once Wi-Fi and the web server are up, or rolls back after 60 s. The log names the running partition, and the OLED shows `ATUALIZANDO <n>%` during a firmware update and `FW <version>` on its last line.

### Context to read first
1. `main/ludo001.cpp`: the whole file (140 lines). Note the `StatusSnapshot` struct, `draw_status`, and the loop in `app_main`.
2. `main/CMakeLists.txt:24-26`: the `idf_component_register` call.
3. `components/web_server/include/web_server.hpp`: `UploadProgress` now has `firmware` (BLOCK 4), and `is_running()` exists.

Read exactly these. Do not open other files unless a step below names one.

### Steps
1. In `main/CMakeLists.txt`, replace
   `                    PRIV_REQUIRES wifi_manager oled_display sd_card web_server)`
   with
   `                    PRIV_REQUIRES wifi_manager oled_display sd_card web_server app_update esp_app_format esp_timer)`
2. In `main/ludo001.cpp`, replace
   ```cpp
   #include "esp_err.h"
   #include "esp_log.h"
   #include "freertos/FreeRTOS.h"
   ```
   with
   ```cpp
   #include "esp_app_desc.h"
   #include "esp_err.h"
   #include "esp_log.h"
   #include "esp_ota_ops.h"
   #include "esp_timer.h"
   #include "freertos/FreeRTOS.h"
   ```
3. In the same file, replace
   ```cpp
   constexpr int kBarH = 8;

   // Everything the OLED shows, gathered by the loop in app_main.
   struct StatusSnapshot {
       bool connected = false;
       char ip[16] = "";
       uint32_t item_count = 0;
       sd_card::SpaceInfo space;
       web_server::UploadProgress progress;
   };

   // Yellow band (y 0-15): Wi-Fi status, as "IP <address>" or "SEM WIFI".
   // Blue area (y 16-63): upload progress bar, item count and SD space.
   ```
   with
   ```cpp
   constexpr int kBarH = 8;

   // A firmware fresh from an OTA update has this long after boot to bring up
   // Wi-Fi and the web server, or the bootloader goes back to the previous one.
   constexpr int64_t kNewFirmwareTimeoutUs = 60 * 1000 * 1000;

   // Everything the OLED shows, gathered by the loop in app_main.
   struct StatusSnapshot {
       bool connected = false;
       char ip[16] = "";
       uint32_t item_count = 0;
       sd_card::SpaceInfo space;
       web_server::UploadProgress progress;
       const char* version = ""; // the running firmware's, from esp_app_get_description()
   };

   // Yellow band (y 0-15): Wi-Fi status, as "IP <address>" or "SEM WIFI".
   // Blue area (y 16-63): progress bar, item count (or "ATUALIZANDO <n>%"
   // during a firmware update), SD space and firmware version.
   ```
4. In `draw_status`, replace
   ```cpp
       char items_line[24];
       snprintf(items_line, sizeof(items_line), "ITENS %lu", static_cast<unsigned long>(status.item_count));
       fb.draw_text(0, 32, items_line);
   ```
   with
   ```cpp
       char info_line[24];
       if (status.progress.active && status.progress.firmware) {
           snprintf(info_line, sizeof(info_line), "ATUALIZANDO %u%%", static_cast<unsigned>(status.progress.percent));
       } else {
           snprintf(info_line, sizeof(info_line), "ITENS %lu", static_cast<unsigned long>(status.item_count));
       }
       fb.draw_text(0, 32, info_line);
   ```
5. In `draw_status`, replace
   ```cpp
       fb.draw_text(0, 44, mem_line);

       display.flush();
   }
   ```
   with the block below. It adds the version line and two new functions after `draw_status`, still inside the anonymous namespace.
   ```cpp
       fb.draw_text(0, 44, mem_line);

       char version_line[40];
       snprintf(version_line, sizeof(version_line), "FW %s", status.version);
       fb.draw_text(0, 56, version_line);

       display.flush();
   }

   // True while the running firmware came from an OTA update and hasn't been
   // confirmed yet: unless check_new_firmware() confirms it, the next restart
   // goes back to the previous one.
   bool new_firmware_pending() {
       esp_ota_img_states_t state;
       return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
              state == ESP_OTA_IMG_PENDING_VERIFY;
   }

   // Confirms a new firmware once Wi-Fi and the web server are both up, since
   // only then can the next update reach the board. Rolls back to the previous
   // firmware (restarting) when kNewFirmwareTimeoutUs passes first. Returns
   // whether the check is still pending.
   bool check_new_firmware(bool wifi_connected) {
       if (wifi_connected && web_server::is_running()) {
           esp_ota_mark_app_valid_cancel_rollback();
           ESP_LOGI(kTag, "new firmware confirmed");
           return false;
       }
       if (esp_timer_get_time() >= kNewFirmwareTimeoutUs) {
           ESP_LOGE(kTag, "new firmware brought up no Wi-Fi and HTTP in 60 s, rolling back");
           esp_ota_mark_app_invalid_rollback_and_reboot();
       }
       return true;
   }
   ```
6. In `app_main`, replace
   ```cpp
   extern "C" void app_main(void) {
       Ssd1306 display;
   ```
   with
   ```cpp
   extern "C" void app_main(void) {
       const esp_app_desc_t* app = esp_app_get_description();
       ESP_LOGI(kTag, "firmware %s running from %s", app->version, esp_ota_get_running_partition()->label);
       bool firmware_pending = new_firmware_pending();

       Ssd1306 display;
   ```
7. In `app_main`, replace
   ```cpp
       StatusSnapshot status;
       std::optional<uint32_t> sd_read_at; // content_version() when the SD figures were last read
   ```
   with
   ```cpp
       StatusSnapshot status;
       status.version = app->version;
       std::optional<uint32_t> sd_read_at; // content_version() when the SD figures were last read
   ```
8. In the loop in `app_main`, replace
   ```cpp
               last_connected = status.connected;
           }
   ```
   with
   ```cpp
               last_connected = status.connected;
           }

           if (firmware_pending) {
               firmware_pending = check_new_firmware(status.connected);
           }
   ```

### Do not
- Do not call `esp_ota_mark_app_valid_cancel_rollback()` anywhere else, for example right after `web_server::start()`. Confirmation happens only in `check_new_firmware`.
- Do not make the 60 s configurable, and do not add a Kconfig option (§9).
- Do not move the `if (firmware_pending)` check inside `if (display_ready)`. It must run without a display.
- Do not touch `components/`.

### Verify
Run from the repository root, in this order:
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py build > build/fw-build.log 2>&1; echo "fw build exit=$?"; grep -E 'warning:|binary size' build/fw-build.log
grep -c 'esp_ota_mark_app_valid_cancel_rollback\|esp_ota_mark_app_invalid_rollback_and_reboot' main/ludo001.cpp
grep -n 'ATUALIZANDO %u%%\|"FW %s"\|running from %s' main/ludo001.cpp
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; export PATH="/usr/bin:$PATH"; idf.py -C host_tests build > build/host-tests-build.log 2>&1; echo "host build exit=$?"; ./host_tests/build/ludo001_host_tests.elf | tail -3
```
Expected:
1. `fw build exit=0` and no `warning:` line. The `ludo001.bin binary size ...` line shows partition `0x1a9000` with at least `0x90000` bytes free.
2. `2`.
3. 3 lines, one for each pattern.
4. `host build exit=0`, `95 Tests 0 Failures 0 Ignored`, `OK`.

### If verification fails
1. Read the failing output in full.
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set BLOCK 5's row in §12 Status to `DONE`.
2. Run:
   ```
   git add main/ludo001.cpp main/CMakeLists.txt PLAN.md
   git commit -m "Confirm a new firmware once Wi-Fi and HTTP are up and show its version on the OLED"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
