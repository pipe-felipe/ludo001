## BLOCK 4 — Add POST /firmware to web_server

**Depends on:** BLOCK 3 committed
**Touches:** `components/web_server/web_server.cpp` (MODIFY), `components/web_server/include/web_server.hpp` (MODIFY), `components/web_server/CMakeLists.txt` (MODIFY)

### Goal
The firmware registers `POST /firmware`, which writes a signed image to the inactive OTA partition and restarts into it. File uploads and firmware uploads share one body-receiving loop, and the progress snapshot says which kind is running.

### Context to read first
1. `components/web_server/web_server.cpp` — the whole file (647 lines). Look closely at `set_progress` (lines 31-42), `store_body` and `upload_handler` (lines 247-309), the route table (lines 516-570) and `start()` (lines 574-617).
2. `components/web_server/include/web_server.hpp` — `UploadProgress` (lines 21-29).
3. `components/web_content/include/web_server_firmware.hpp` — `kFirmwareHeadSize`, `firmware_size_fits`, `firmware_head_matches` (added in BLOCK 2).
4. §8 Security invariants 4, 5, 6 and 7, and §7 rule 7 (why some rejections return `ESP_OK`).

Read exactly these. Do not open other files unless a step below names one.

### Steps
1. In `components/web_server/CMakeLists.txt`, replace
   `    PRIV_REQUIRES esp_http_server wifi_manager sd_card web_content dir_scan`
   with
   `    PRIV_REQUIRES esp_http_server wifi_manager sd_card web_content dir_scan app_update esp_app_format esp_timer`
2. In `components/web_server/include/web_server.hpp`, replace
   ```cpp
   struct UploadProgress {
       bool active = false;
       uint8_t percent = 0;
   };

   // Snapshot of the in-progress upload (if any), so other tasks -- e.g. the
   // OLED display loop -- can show a progress bar without touching httpd
   // internals.
   ```
   with
   ```cpp
   struct UploadProgress {
       bool active = false;
       uint8_t percent = 0;
       bool firmware = false; // a firmware update (POST /firmware), not a file upload
   };

   // Snapshot of the upload or firmware update in progress (if any), so other
   // tasks -- e.g. the OLED display loop -- can show a progress bar without
   // touching httpd internals.
   ```
3. In `components/web_server/web_server.cpp`, replace the include lines
   ```cpp
   #include "dir_scan.hpp"
   #include "esp_http_server.h"
   #include "esp_log.h"
   #include "freertos/FreeRTOS.h"
   #include "mdns.h"
   #include "web_server_render.hpp"
   ```
   with
   ```cpp
   #include "dir_scan.hpp"
   #include "esp_app_desc.h"
   #include "esp_http_server.h"
   #include "esp_log.h"
   #include "esp_ota_ops.h"
   #include "esp_system.h"
   #include "esp_timer.h"
   #include "freertos/FreeRTOS.h"
   #include "mdns.h"
   #include "web_server_firmware.hpp"
   #include "web_server_render.hpp"
   ```
4. In the same file, replace
   ```cpp
   // Written by the httpd task inside upload_handler, read by the display
   // loop (a different task); a spinlock keeps the read/write atomic without
   // pulling in a full mutex for two small fields.
   portMUX_TYPE s_progress_lock = portMUX_INITIALIZER_UNLOCKED;
   UploadProgress s_progress;

   void set_progress(bool active, uint8_t percent) {
       portENTER_CRITICAL(&s_progress_lock);
       s_progress.active = active;
       s_progress.percent = percent;
       portEXIT_CRITICAL(&s_progress_lock);
   }
   ```
   with
   ```cpp
   // Written by the httpd task inside recv_body, read by the display loop (a
   // different task); a spinlock keeps the read/write atomic without pulling
   // in a full mutex for three small fields.
   portMUX_TYPE s_progress_lock = portMUX_INITIALIZER_UNLOCKED;
   UploadProgress s_progress;

   void set_progress(const UploadProgress& progress) {
       portENTER_CRITICAL(&s_progress_lock);
       s_progress = progress;
       portEXIT_CRITICAL(&s_progress_lock);
   }

   // Set once firmware_handler has written a new firmware and scheduled the
   // restart into it. From then on POST /firmware and POST /upload get a 409
   // instead of starting work the restart would cut short.
   std::atomic<bool> s_restart_pending{false};

   // Time for firmware_handler's "OK" to reach the client before the restart.
   constexpr uint64_t kRestartDelayUs = 1000 * 1000;
   ```
5. In the same file, replace the 3-line comment that begins `// Writes the request body to a new file at` and the whole `store_body` function under it, up to and including its closing `}` (the line before the blank line that precedes the `// POST /upload/<path>` comment), with:
   ```cpp
   // Reads the rest of the request body, from byte `done` on, in chunks of up
   // to 1 KB, handing each one to `on_chunk`, which returns nullptr or the
   // reason to stop. Keeps the OLED's progress bar up to date meanwhile;
   // `firmware` only labels that progress. nullptr once the whole body is in,
   // otherwise the reason it stopped.
   template <typename OnChunk>
   const char* recv_body(httpd_req_t* req, int done, bool firmware, OnChunk on_chunk) {
       const int total = req->content_len;
       const char* error = nullptr;
       set_progress({true, 0, firmware});

       char buffer[1024];
       while (done < total && error == nullptr) {
           int remaining = total - done;
           int received = httpd_req_recv(req, buffer, remaining < static_cast<int>(sizeof(buffer)) ? remaining : sizeof(buffer));
           if (received == HTTPD_SOCK_ERR_TIMEOUT) {
               continue;
           }
           if (received <= 0) {
               error = "upload interrupted";
           } else {
               error = on_chunk(buffer, received);
               done += received;
               // 64-bit product: done * 100 overflows an int past 21 MB.
               set_progress({true, static_cast<uint8_t>(static_cast<uint64_t>(done) * 100 / total), firmware});
           }
       }
       set_progress({});
       return error;
   }

   // Writes the request body to a new file at `path`, keeping the upload
   // progress up to date for the OLED. A partly written file is removed.
   // nullptr on success, otherwise the reason for the 500 response.
   const char* store_body(httpd_req_t* req, const std::string& path) {
       FILE* file = fopen(path.c_str(), "wb");
       if (file == nullptr) {
           return "could not open file for writing";
       }

       const char* error = recv_body(req, 0, false, [file](const char* data, int len) -> const char* {
           return fwrite(data, 1, len, file) == static_cast<size_t>(len) ? nullptr : "could not write file";
       });
       fclose(file);
       if (error != nullptr) {
           remove(path.c_str());
       }
       return error;
   }

   // The answer to a request that would start writing while a firmware
   // update has the board about to restart.
   esp_err_t send_restart_pending(httpd_req_t* req) {
       httpd_resp_set_status(req, "409 Conflict");
       return httpd_resp_sendstr(req, "firmware update in progress, the board is restarting");
   }
   ```
6. In the same file, replace
   ```cpp
   esp_err_t upload_handler(httpd_req_t* req) {
       std::string name = request_path(req, "/upload/");
   ```
   with
   ```cpp
   esp_err_t upload_handler(httpd_req_t* req) {
       if (s_restart_pending) {
           return send_restart_pending(req);
       }

       std::string name = request_path(req, "/upload/");
   ```
7. In the same file, immediately before the line `constexpr httpd_uri_t kRootUri = {`, insert this block followed by one blank line:
   ```cpp
   // Fills `buffer` with the next `len` bytes of the request body. False if
   // the client went away first.
   bool recv_exact(httpd_req_t* req, char* buffer, int len) {
       for (int got = 0; got < len;) {
           int received = httpd_req_recv(req, buffer + got, len - got);
           if (received == HTTPD_SOCK_ERR_TIMEOUT) {
               continue;
           }
           if (received <= 0) {
               return false;
           }
           got += received;
       }
       return true;
   }

   // Writes the image whose first `head_len` bytes are `head` (the rest still
   // unread in the request body) to `partition`, then has esp_ota_end() check
   // the whole image, signature included. On a failure it answers the request
   // itself and returns false; the partition the board boots from is never
   // touched.
   bool write_firmware(httpd_req_t* req, const esp_partition_t* partition, const char* head, int head_len) {
       esp_ota_handle_t handle = 0;
       esp_err_t err = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle);
       if (err != ESP_OK) {
           ESP_LOGE(kTag, "esp_ota_begin failed: %s", esp_err_to_name(err));
           httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not start firmware update");
           return false;
       }

       auto write_chunk = [handle](const char* data, int len) -> const char* {
           return esp_ota_write(handle, data, len) == ESP_OK ? nullptr : "could not write firmware";
       };
       const char* error = write_chunk(head, head_len);
       if (error == nullptr) {
           error = recv_body(req, head_len, true, write_chunk);
       }
       if (error != nullptr) {
           esp_ota_abort(handle);
           httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, error);
           return false;
       }

       err = esp_ota_end(handle);
       if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
           httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "firmware image truncated, corrupt or not signed with this board's key");
           return false;
       }
       if (err != ESP_OK) {
           ESP_LOGE(kTag, "esp_ota_end failed: %s", esp_err_to_name(err));
           httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not finish firmware update");
           return false;
       }
       return true;
   }

   // Restarts the board kRestartDelayUs from now, from the esp_timer task, so
   // firmware_handler can still answer first. Restarts right away if the
   // timer can't be set up.
   void schedule_restart() {
       const esp_timer_create_args_t args = {
           .callback = [](void*) { esp_restart(); },
           .arg = nullptr,
           .dispatch_method = ESP_TIMER_TASK,
           .name = "fw_restart",
           .skip_unhandled_events = false,
       };
       esp_timer_handle_t timer = nullptr;
       if (esp_timer_create(&args, &timer) != ESP_OK || esp_timer_start_once(timer, kRestartDelayUs) != ESP_OK) {
           esp_restart();
       }
   }

   // POST /firmware -- writes the raw request body, a signed build/ludo001.bin,
   // to the OTA partition the board is not running from, makes that the boot
   // partition and restarts into it. Nothing reaches flash unless the size
   // fits the partition and the image was built for this project; then
   // esp_ota_end() checks the whole image and its signature
   // (CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT) before the boot partition
   // changes. The rejections before the body is read return ESP_OK, so httpd
   // reads and discards the rest of the body: the client gets the error
   // status instead of a connection reset.
   esp_err_t firmware_handler(httpd_req_t* req) {
       if (s_restart_pending) {
           return send_restart_pending(req);
       }

       const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
       if (partition == nullptr) {
           httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no OTA partition to update");
           return ESP_OK;
       }
       if (!firmware_size_fits(req->content_len, partition->size)) {
           httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "firmware image empty or larger than the update partition");
           return ESP_OK;
       }

       char head[kFirmwareHeadSize];
       const int head_len = static_cast<int>(req->content_len < kFirmwareHeadSize ? req->content_len : kFirmwareHeadSize);
       if (!recv_exact(req, head, head_len)) {
           return ESP_FAIL; // the client went away
       }
       if (!firmware_head_matches(std::string_view(head, head_len), esp_app_get_description()->project_name)) {
           httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "not a ludo001 firmware image");
           return ESP_OK;
       }

       if (!write_firmware(req, partition, head, head_len)) {
           return ESP_FAIL;
       }
       esp_err_t err = esp_ota_set_boot_partition(partition);
       if (err != ESP_OK) {
           ESP_LOGE(kTag, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
           httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not switch to the new firmware");
           return ESP_FAIL;
       }

       s_restart_pending = true;
       ESP_LOGI(kTag, "firmware written to %s (%u bytes), restarting", partition->label, static_cast<unsigned>(req->content_len));
       httpd_resp_sendstr(req, "OK");
       schedule_restart();
       return ESP_OK;
   }
   ```
8. In the same file, immediately after the closing `};` of `kZipUri` (the line before `} // namespace`), insert one blank line and:
   ```cpp
   constexpr httpd_uri_t kFirmwareUri = {
       .uri = "/firmware",
       .method = HTTP_POST,
       .handler = firmware_handler,
       .user_ctx = nullptr,
   };
   ```
9. In `start()`, replace
   ```cpp
       config.stack_size = 8192;
       esp_err_t err = httpd_start(&s_server, &config);
   ```
   with
   ```cpp
       config.stack_size = 8192;
       config.max_uri_handlers = 12; // the 9 routes registered below, plus room for 3 more
       esp_err_t err = httpd_start(&s_server, &config);
   ```
10. In `start()`, replace
    ```cpp
            &kRootUri, &kStyleCssUri, &kAppJsUri, &kFilesPageUri, &kFilesItemUri, &kUploadUri, &kDeleteUri, &kZipUri};
    ```
    with
    ```cpp
            &kRootUri, &kStyleCssUri, &kAppJsUri, &kFilesPageUri, &kFilesItemUri, &kUploadUri, &kDeleteUri, &kZipUri,
            &kFirmwareUri};
    ```

### Do not
- Do not keep a second receive loop inside `store_body` or `write_firmware`; both go through `recv_body` (§9).
- Do not add an error struct or enum; `write_firmware` answers the request itself (§9).
- Do not call `esp_restart()` inside `firmware_handler` and do not `vTaskDelay` there. The restart goes through `schedule_restart()`, so the 409 window exists.
- Do not compare the new image's version with the running one (§9).
- Do not change `is_safe_filename` calls, `files_item_handler`, `delete_handler` or `zip_handler` (§8 invariant 7).
- Do not touch `root_handler` or `components/web_server/web/`; the page is BLOCK 6. Do not touch `main/`; that is BLOCK 5.

### Verify
Run from the repository root, in this order:
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py build > build/fw-build.log 2>&1; echo "fw build exit=$?"; grep -E 'warning:|binary size' build/fw-build.log
grep -c 'set_progress(true\|set_progress(false' components/web_server/web_server.cpp
grep -c 'httpd_req_recv(' components/web_server/web_server.cpp
grep -n 'max_uri_handlers = 12\|&kFirmwareUri};\|s_restart_pending = true;' components/web_server/web_server.cpp
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; export PATH="/usr/bin:$PATH"; idf.py -C host_tests build > build/host-tests-build.log 2>&1; echo "host build exit=$?"; ./host_tests/build/ludo001_host_tests.elf | tail -3
```
Expected:
1. `fw build exit=0`, no `warning:` line, `ludo001.bin binary size 0xffff4 bytes. Smallest app partition is 0x1a9000 bytes. 0xa900c bytes (40%) free.` Other size figures also pass, as long as the partition is `0x1a9000` and the free figure is at least `0x90000`.
2. `0`: the old two-argument `set_progress` calls are gone.
3. `2`: one call in `recv_body`, one in `recv_exact`.
4. 3 lines, one for each pattern.
5. `host build exit=0`, `95 Tests 0 Failures 0 Ignored`, `OK`.

Then confirm §8 invariants 4–7 by reading the new `firmware_handler` and `write_firmware`:
- no flash call comes before `firmware_size_fits` and `firmware_head_matches`;
- `esp_ota_set_boot_partition` comes only after `write_firmware` returned true;
- no error message or log line prints request data.

### If verification fails
1. Read the failing output in full (`build/fw-build.log` for a build failure).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set BLOCK 4's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/web_server/web_server.cpp components/web_server/include/web_server.hpp components/web_server/CMakeLists.txt PLAN.md
   git commit -m "Add POST /firmware to update the firmware over the network"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
