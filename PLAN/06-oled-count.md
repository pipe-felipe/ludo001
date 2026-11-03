## BLOCK 6 — Count every file and folder on the OLED, only after a change

**Depends on:** BLOCK 5 committed
**Touches:** `components/sd_card/include/sd_card.hpp` (MODIFY), `components/sd_card/sd_card.cpp` (MODIFY), `components/sd_card/CMakeLists.txt` (MODIFY), `main/ludo001.cpp` (MODIFY)
4 files: `sd_card`'s public API changes (`file_count` → `item_count`, `space_info` returns a struct), and `main/ludo001.cpp` is its only caller (`grep -rn 'file_count\|space_info' main components` finds no other). Every edit is listed.

### Goal
`ITENS` on the OLED shows `dir_scan::count_entries` of the card's root. `draw_status` takes a `StatusSnapshot`. The card is read only on the first loop pass and when `web_server::content_version()` changes, and each read logs `sd card: <n> items, <u> MB used, <f> MB free`.

### Context to read first
1. `components/sd_card/include/sd_card.hpp` and `components/sd_card/sd_card.cpp`: `file_count` and `space_info` as they are today.
2. `main/ludo001.cpp`: the whole file (121 lines); step 4 replaces it.
3. `components/dir_scan/include/dir_scan.hpp` and `components/web_server/include/web_server.hpp`: `dir_scan::count_entries`, `web_server::content_version`, `web_server::UploadProgress`.

Read exactly these. Do not open other files unless a step below names one.

### Steps
1. In `components/sd_card/include/sd_card.hpp`, replace:
   ```cpp
   // Number of regular entries at the mount point's root (dotfiles
   // excluded), or 0 if not mounted.
   uint32_t file_count();

   // Used/free space on the mounted filesystem, in megabytes.
   // ESP_ERR_INVALID_STATE if not mounted.
   esp_err_t space_info(uint64_t* used_mb, uint64_t* free_mb);
   ```
   with:
   ```cpp
   // Number of files and folders on the card, at any depth (names starting
   // with '.' excluded, and hidden folders not entered), or 0 if not mounted.
   // Walks the whole card over SPI, so call it only when the content may have
   // changed.
   uint32_t item_count();

   struct SpaceInfo {
       uint64_t used_mb = 0;
       uint64_t free_mb = 0;
   };

   // Used/free space on the mounted filesystem, in megabytes; both 0 if not
   // mounted or if the filesystem can't be queried.
   SpaceInfo space_info();
   ```
2. In `components/sd_card/sd_card.cpp`, make 2 edits:
   - Replace:
     ```cpp
     #include <cstring>
     #include <dirent.h>

     #include "driver/sdspi_host.h"
     ```
     with:
     ```cpp
     #include <cstring>

     #include "dir_scan.hpp"
     #include "driver/sdspi_host.h"
     ```
   - Replace the 2 functions `uint32_t file_count() { ... }` and `esp_err_t space_info(uint64_t* used_mb, uint64_t* free_mb) { ... }` (from the line `uint32_t file_count() {` through the closing `}` of `space_info`, the `}` right after `    return ESP_OK;` that follows `    *free_mb = free_bytes / (1024 * 1024);`) with:
     ```cpp
     uint32_t item_count() {
         return s_mounted ? dir_scan::count_entries(s_mount_point) : 0;
     }

     SpaceInfo space_info() {
         SpaceInfo info;
         uint64_t total_bytes = 0;
         uint64_t free_bytes = 0;
         if (!s_mounted || esp_vfs_fat_info(s_mount_point, &total_bytes, &free_bytes) != ESP_OK) {
             return info;
         }
         info.used_mb = (total_bytes - free_bytes) / (1024 * 1024);
         info.free_mb = free_bytes / (1024 * 1024);
         return info;
     }
     ```
     The only caller ignored the old `esp_err_t` and drew zeros on failure, so returning a zeroed `SpaceInfo` keeps what the OLED shows.
3. In `components/sd_card/CMakeLists.txt`, replace:
   ```cmake
       REQUIRES fatfs sdmmc esp_driver_sdspi esp_driver_spi
   )
   ```
   with:
   ```cmake
       REQUIRES fatfs sdmmc esp_driver_sdspi esp_driver_spi
       PRIV_REQUIRES dir_scan
   )
   ```
4. Replace the whole content of `main/ludo001.cpp` with:
   ```cpp
   #include <cstdio>
   #include <optional>

   #include "esp_err.h"
   #include "esp_log.h"
   #include "freertos/FreeRTOS.h"
   #include "freertos/task.h"

   #include "oled_display.hpp"
   #include "sd_card.hpp"
   #include "web_server.hpp"
   #include "wifi_manager.hpp"
   #include "wifi_credentials.h" // generated at build time from .env, see main/CMakeLists.txt

   namespace {

   constexpr char kTag[] = "ludo001";

   using oled_display::Ssd1306;

   constexpr int kBarX = 4;
   constexpr int kBarY = 20;
   constexpr int kBarW = 120;
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
   void draw_status(Ssd1306& display, const StatusSnapshot& status) {
       auto& fb = display.framebuffer();
       fb.clear();

       if (status.connected) {
           char line[24];
           snprintf(line, sizeof(line), "IP %s", status.ip);
           fb.draw_text(2, 4, line);
       } else {
           fb.draw_text(2, 4, "SEM WIFI");
       }

       fb.draw_rect(kBarX, kBarY, kBarW, kBarH);
       if (status.progress.active && status.progress.percent > 0) {
           int fill_w = (kBarW - 2) * status.progress.percent / 100;
           fb.fill_rect(kBarX + 1, kBarY + 1, fill_w, kBarH - 2);
       }

       char items_line[24];
       snprintf(items_line, sizeof(items_line), "ITENS %lu", static_cast<unsigned long>(status.item_count));
       fb.draw_text(0, 32, items_line);

       char mem_line[32];
       snprintf(mem_line, sizeof(mem_line), "USO %llu/%lluMB",
           static_cast<unsigned long long>(status.space.used_mb), static_cast<unsigned long long>(status.space.free_mb));
       fb.draw_text(0, 44, mem_line);

       display.flush();
   }

   } // namespace

   extern "C" void app_main(void) {
       Ssd1306 display;
       esp_err_t err = display.init();
       bool display_ready = (err == ESP_OK);
       if (!display_ready) {
           ESP_LOGE(kTag, "display init failed: %s", esp_err_to_name(err));
       } else {
           draw_status(display, StatusSnapshot{});
       }

       wifi_manager::StaConfig wifi_config{WIFI_SSID, WIFI_PASSWORD};
       err = wifi_manager::connect(wifi_config);
       if (err == ESP_OK) {
           char ip[16];
           wifi_manager::get_ip(ip, sizeof(ip));
           ESP_LOGI(kTag, "wifi connected, ssid=%s ip=%s", WIFI_SSID, ip);
       } else {
           ESP_LOGE(kTag, "wifi connect failed: ssid=%s error=%s", WIFI_SSID, esp_err_to_name(err));
       }

       err = sd_card::mount();
       if (err == ESP_OK) {
           ESP_LOGI(kTag, "sd card mounted, capacity=%llu MB", static_cast<unsigned long long>(sd_card::capacity_mb()));
       } else {
           ESP_LOGE(kTag, "sd card mount failed: %s", esp_err_to_name(err));
       }

       err = web_server::start();
       if (err == ESP_OK) {
           ESP_LOGI(kTag, "web server started");
       } else {
           ESP_LOGE(kTag, "web server start failed: %s", esp_err_to_name(err));
       }

       StatusSnapshot status;
       std::optional<uint32_t> sd_read_at; // content_version() when the SD figures were last read
       bool last_connected = false;
       while (true) {
           status.connected = wifi_manager::is_connected();
           status.ip[0] = '\0';
           if (status.connected) {
               wifi_manager::get_ip(status.ip, sizeof(status.ip));
           }

           if (status.connected != last_connected) {
               if (status.connected) {
                   ESP_LOGI(kTag, "wifi status: CONNECTED ip=%s", status.ip);
               } else {
                   ESP_LOGW(kTag, "wifi status: DISCONNECTED");
               }
               last_connected = status.connected;
           }

           if (display_ready) {
               // Walking the card is slow over SPI: re-read it only at start-up
               // and after an upload or delete.
               uint32_t version = web_server::content_version();
               if (sd_read_at != version) {
                   status.item_count = sd_card::item_count();
                   status.space = sd_card::space_info();
                   sd_read_at = version;
                   ESP_LOGI(kTag, "sd card: %lu items, %llu MB used, %llu MB free",
                       static_cast<unsigned long>(status.item_count), static_cast<unsigned long long>(status.space.used_mb),
                       static_cast<unsigned long long>(status.space.free_mb));
               }
               status.progress = web_server::upload_progress();
               draw_status(display, status);
           }

           vTaskDelay(pdMS_TO_TICKS(400));
       }
   }
   ```
   Lines 1-25 and the start-up code in `app_main` (display init through `web_server::start()`) match today's file except the added `#include <optional>`, the new struct, the new `draw_status` and its first call.

### Do not
- Do not move the count refresh out of `if (display_ready)`. Today the card is read only when the OLED works, and BLOCK 8 checks the log line on a board whose display is attached.
- Do not add a refresh timer, a minimum interval, or a second task. `sd_read_at != version` is the only trigger (§9).
- Do not log the Wi-Fi password, and do not change the existing log lines (§8 invariant 6).
- Do not rename `kBarX`/`kBarY`/`kBarW`/`kBarH` or change the drawing coordinates.

### Verify
Run from the repository root, in this order:
```
mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log
grep -rn 'file_count\|space_info(&' main components
grep -rn 'readdir' components main --include='*.cpp'
grep -c 'sd_card::item_count()' main/ludo001.cpp
grep -n 'void draw_status(Ssd1306& display, const StatusSnapshot& status)' main/ludo001.cpp
mkdir -p build && rm -f build/plan-host.log build/plan-tests.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && export PATH="/usr/bin:$PATH" && idf.py -C host_tests build > build/plan-host.log 2>&1 && ./host_tests/build/ludo001_host_tests.elf > build/plan-tests.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-host.log; tail -3 build/plan-tests.log
```
Expected:
- First command: `exit=0`, no `warning:`/`error:` line, and `ludo001.bin binary size ... bytes (3%) free.` with at least `0x6c00` bytes free (measured while planning: `0x6ff0`).
- Second command: no output.
- Third command: exactly one line, `components/dir_scan/dir_scan.cpp:<line>:    while ((entry = readdir(handle)) != nullptr) {`.
- Fourth command: `1`.
- Fifth command: one line.
- Sixth command: `exit=0`, no `warning:`/`error:` line, `83 Tests 0 Failures 0 Ignored`, `OK`.

### If verification fails
1. Read the failing output in full (`build/plan-fw.log`).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set this block's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/sd_card/include/sd_card.hpp components/sd_card/sd_card.cpp components/sd_card/CMakeLists.txt main/ludo001.cpp PLAN.md
   git commit -m "Count every file and folder on the OLED and read the card only after a change"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
