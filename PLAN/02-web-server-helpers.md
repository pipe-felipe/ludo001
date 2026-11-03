## BLOCK 2 — Route web_server's directory walks and downloads through shared helpers

**Depends on:** BLOCK 1 committed
**Touches:** `components/web_server/web_server.cpp` (MODIFY), `components/web_server/CMakeLists.txt` (MODIFY)

### Goal
`web_server.cpp` has no `opendir`/`readdir`/`closedir`. Directory listing, delete and zip collection use `dir_scan::visible_entries`. The file-streaming loop exists once (`send_file_chunks`). `send_embedded` is replaced by `send_text`. The firmware still builds.

### Context to read first
1. `components/web_server/web_server.cpp`: the whole file (645 lines). Each step below names the exact text it replaces.
2. `components/dir_scan/include/dir_scan.hpp`: `dir_scan::Entry`, `dir_scan::visible_entries`.
3. §8 Security invariants 1, 3 and 5. This block touches file paths and response headers.

Read exactly these. Do not open other files unless a step below names one.

### Steps
Every fenced code block below is literal: type it exactly as written, including the comments it carries over from the current file and its double-brace initialisers.

1. In `components/web_server/web_server.cpp`, replace these 3 lines:
   ```cpp
   #include <cstring>
   #include <dirent.h>
   #include <sys/stat.h>
   ```
   with:
   ```cpp
   #include <cstring>
   #include <sys/stat.h>
   ```
2. In the same file, replace the line `#include "esp_http_server.h"` with these 2 lines:
   ```cpp
   #include "dir_scan.hpp"
   #include "esp_http_server.h"
   ```
3. Replace this text (the functions `send_embedded`, `style_css_handler`, `app_js_handler`):
   ```cpp
   esp_err_t send_embedded(httpd_req_t* req, const char* content_type, const uint8_t* start, const uint8_t* end) {
       httpd_resp_set_type(req, content_type);
       return httpd_resp_send(req, reinterpret_cast<const char*>(start), end - start - 1);
   }

   esp_err_t style_css_handler(httpd_req_t* req) {
       return send_embedded(req, "text/css", style_css_start, style_css_end);
   }

   esp_err_t app_js_handler(httpd_req_t* req) {
       return send_embedded(req, "application/javascript", app_js_start, app_js_end);
   }
   ```
   with:
   ```cpp
   esp_err_t send_text(httpd_req_t* req, const char* content_type, std::string_view body) {
       httpd_resp_set_type(req, content_type);
       return httpd_resp_send(req, body.data(), body.size());
   }

   esp_err_t style_css_handler(httpd_req_t* req) {
       return send_text(req, "text/css", embedded_text(style_css_start, style_css_end));
   }

   esp_err_t app_js_handler(httpd_req_t* req) {
       return send_text(req, "application/javascript", embedded_text(app_js_start, app_js_end));
   }
   ```
4. Replace the whole function `render_directory`, starting at its comment line `// Shared by the root listing ("/files") and directory browsing under` and ending at its closing `}` (the `}` right after `    return httpd_resp_send(req, body.c_str(), body.size());` inside `render_directory`), with:
   ```cpp
   // The decoded <path> of a request to `route` + <path>: "a/b.txt" for
   // "/files/a/b.txt" with route "/files/".
   std::string request_path(httpd_req_t* req, const char* route) {
       char decoded[256];
       url_decode(req->uri + strlen(route), decoded, sizeof(decoded));
       return decoded;
   }

   // SD card path of `relative_path`, which has no leading slash ("" is the
   // card's root).
   std::string sd_path(const std::string& relative_path) {
       std::string path = sd_card::mount_point();
       if (!relative_path.empty()) {
           path += "/" + relative_path;
       }
       return path;
   }

   // The listing's entries for the SD card folder `full_dir`, with file sizes
   // already formatted. A file stat() can't read is skipped rather than
   // failing the whole listing.
   std::vector<FileEntry> list_entries(const std::string& full_dir) {
       std::vector<FileEntry> entries;
       for (const dir_scan::Entry& child : dir_scan::visible_entries(full_dir)) {
           FileEntry fe;
           fe.name = child.name;
           fe.is_dir = child.is_dir;
           if (!child.is_dir) {
               struct stat st {};
               if (stat((full_dir + "/" + child.name).c_str(), &st) != 0) {
                   continue;
               }
               char size_text[24];
               format_size(static_cast<long>(st.st_size), size_text, sizeof(size_text));
               fe.size_text = size_text;
           }
           entries.push_back(std::move(fe));
       }
       return entries;
   }

   // Shared by the root listing ("/files") and directory browsing under
   // "/files/*" (see files_item_handler) once a target turns out to be a
   // directory rather than a file. `relative_dir` has no leading/trailing
   // slash ("" for the root).
   esp_err_t render_directory(httpd_req_t* req, const std::string& relative_dir) {
       std::string list_html = sd_card::is_mounted()
           ? render_file_list(list_entries(sd_path(relative_dir)), relative_dir)
           : "<p class=\"empty\">SD card nao montado.</p>";

       std::string body = render_template(embedded_text(files_html_start, files_html_end),
           {{"DIR_TITLE", render_dir_title(relative_dir)}, {"FILE_LIST", list_html}});

       return send_text(req, "text/html", body);
   }
   ```
   The `char size_text[24]` buffer stays until BLOCK 4 changes `format_size`.
5. In `root_handler`, replace:
   ```cpp
               {"SD_LINE", render_sd_line(sd_mounted, sd_card::capacity_mb())},
           });

       httpd_resp_set_type(req, "text/html");
       return httpd_resp_send(req, body.c_str(), body.size());
   ```
   with:
   ```cpp
               {"SD_LINE", render_sd_line(sd_mounted, sd_card::capacity_mb())},
           });

       return send_text(req, "text/html", body);
   ```
6. Replace the whole function `files_item_handler`, starting at its comment line `// GET /files/<path> -- either streams a file back (with a` and ending at its closing `}` (the `}` right after `    return err;`), with:
   ```cpp
   // Sends what's left of `file` as response chunks of up to 1 KB, handing
   // each chunk to `on_chunk` first. False if the client went away.
   template <typename OnChunk>
   bool send_file_chunks(httpd_req_t* req, FILE* file, OnChunk on_chunk) {
       char buffer[1024];
       size_t read_bytes;
       while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
           on_chunk(reinterpret_cast<const uint8_t*>(buffer), read_bytes);
           if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
               return false;
           }
       }
       return true;
   }

   // Streams the file at `path` back with a Content-Disposition that makes the
   // browser save it as `name` instead of trying to render it.
   esp_err_t send_download(httpd_req_t* req, const std::string& path, const std::string& name) {
       FILE* file = fopen(path.c_str(), "rb");
       if (file == nullptr) {
           httpd_resp_send_404(req);
           return ESP_FAIL;
       }

       httpd_resp_set_type(req, "application/octet-stream");
       // httpd keeps only a pointer to the header value, so it must live until
       // the last chunk is out.
       std::string content_disposition = "attachment; filename=\"" + name + "\"";
       httpd_resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());

       bool sent = send_file_chunks(req, file, [](const uint8_t*, size_t) {});
       fclose(file);
       httpd_resp_send_chunk(req, nullptr, 0);
       return sent ? ESP_OK : ESP_FAIL;
   }

   // GET /files/<path> -- either downloads a file (send_download) or, if
   // <path> names a directory, browses into it via render_directory().
   // <path> may be several segments deep ("folder/sub/file.txt"), matching
   // how a folder upload lays files out (see upload_handler).
   esp_err_t files_item_handler(httpd_req_t* req) {
       std::string name = request_path(req, "/files/");
       while (!name.empty() && name.back() == '/') {
           name.pop_back(); // a directory link's trailing slash
       }

       if (!sd_card::is_mounted()) {
           httpd_resp_send_404(req);
           return ESP_FAIL;
       }
       if (name.empty()) {
           return render_directory(req, ""); // "/files/" with nothing after it
       }
       if (!is_safe_filename(name.c_str())) {
           httpd_resp_send_404(req);
           return ESP_FAIL;
       }

       std::string path = sd_path(name);
       struct stat st {};
       if (stat(path.c_str(), &st) != 0) {
           httpd_resp_send_404(req);
           return ESP_FAIL;
       }
       if (S_ISDIR(st.st_mode)) {
           return render_directory(req, name);
       }
       return send_download(req, path, name);
   }
   ```
7. Replace the functions `remove_recursive` and `delete_handler`, starting at the line `// Deletes a file, or a directory and everything under it.` and ending at the closing `}` of `delete_handler` (the `}` right after `    return httpd_resp_sendstr(req, "OK");` that follows `ESP_LOGI(kTag, "deleted %s", decoded_name);`), with:
   ```cpp
   // Deletes a file, or a directory and everything under it.
   bool remove_recursive(const std::string& path) {
       struct stat st {};
       if (stat(path.c_str(), &st) != 0) {
           return false;
       }
       if (!S_ISDIR(st.st_mode)) {
           return remove(path.c_str()) == 0;
       }

       bool ok = true;
       for (const dir_scan::Entry& child : dir_scan::visible_entries(path)) {
           if (!remove_recursive(path + "/" + child.name)) {
               ok = false;
           }
       }
       return rmdir(path.c_str()) == 0 && ok;
   }

   // POST /delete/<path> -- removes a file, or a whole folder (recursively),
   // from the SD card.
   esp_err_t delete_handler(httpd_req_t* req) {
       std::string name = request_path(req, "/delete/");
       if (!sd_card::is_mounted() || !is_safe_filename(name.c_str())) {
           httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
           return ESP_FAIL;
       }

       if (!remove_recursive(sd_path(name))) {
           httpd_resp_send_404(req);
           return ESP_FAIL;
       }

       ESP_LOGI(kTag, "deleted %s", name.c_str());
       return httpd_resp_sendstr(req, "OK");
   }
   ```
8. Replace the whole function `collect_zip_entries`, starting at its comment line that begins with `// Appends an entry for everything under` and ending at its closing `}` (the `}` right after `    closedir(dir);`), with:
   ```cpp
   // Appends an entry for everything under `full_dir` (an SD card path) to
   // `entries`, recursively, each named `prefix` + its path below `full_dir`.
   // Folders get their own "<name>/" entry so empty ones survive the round
   // trip. Dotfiles and unreadable files are skipped, matching what
   // render_directory() lists.
   void collect_zip_entries(const std::string& full_dir, const std::string& prefix, std::vector<ZipEntry>& entries) {
       for (const dir_scan::Entry& child : dir_scan::visible_entries(full_dir)) {
           std::string child_path = full_dir + "/" + child.name;
           ZipEntry zip_entry;
           zip_entry.name = prefix + child.name;
           if (child.is_dir) {
               zip_entry.name += '/';
               entries.push_back(zip_entry);
               collect_zip_entries(child_path, zip_entry.name, entries);
               continue;
           }
           struct stat st {};
           if (stat(child_path.c_str(), &st) != 0) {
               continue;
           }
           zip_entry.size = static_cast<uint32_t>(st.st_size);
           entries.push_back(std::move(zip_entry));
       }
   }
   ```
9. Replace the whole function `send_zip_file_data`, starting at its comment line that begins with `// Sends one file's bytes as the data of` and ending at its closing `}` (the `}` right after `    return ok && sent == entry.size;`), with:
   ```cpp
   // Sends one file's bytes as the data of `entry`, computing its CRC-32 on
   // the way. False if the file can't be read in full or the client went
   // away, so the caller can abort instead of finishing a corrupt archive.
   bool send_zip_file_data(httpd_req_t* req, const std::string& path, ZipEntry& entry) {
       FILE* file = fopen(path.c_str(), "rb");
       if (file == nullptr) {
           return false;
       }
       uint32_t crc = 0;
       uint32_t sent = 0;
       bool ok = send_file_chunks(req, file, [&](const uint8_t* data, size_t len) {
           crc = crc32_update(crc, data, len);
           sent += static_cast<uint32_t>(len);
       });
       fclose(file);
       entry.crc32 = crc;
       return ok && sent == entry.size;
   }
   ```
10. In `start()`, replace these 4 comment lines:
    ```cpp
        // Default 4096 isn't enough for render_directory()'s call chain (a
        // FileEntry vector, several std::strings, and render_file_list()'s
        // char item[1800]/encoded_name[200] buffers) -- confirmed via a
        // captured StoreProhibited panic while browsing a subdirectory.
    ```
    with:
    ```cpp
        // Default 4096 isn't enough for render_directory()'s call chain --
        // confirmed via a captured StoreProhibited panic while browsing a
        // subdirectory.
    ```
    Keep `    config.stack_size = 8192;` unchanged.
11. In `components/web_server/CMakeLists.txt`, replace `PRIV_REQUIRES esp_http_server wifi_manager sd_card web_content` with `PRIV_REQUIRES esp_http_server wifi_manager sd_card web_content dir_scan`.

After these steps, `upload_handler`, `ensure_parent_dirs`, `send_zip_record` and `zip_handler` are unchanged. BLOCK 3 rewrites them.

### Do not
- Do not change `upload_handler`, `ensure_parent_dirs`, `send_zip_record` or `zip_handler` in this block. They are BLOCK 3.
- Do not change `url_decode`, `format_size` or `is_safe_filename` calls beyond the steps above. Their signatures change in BLOCK 4.
- Do not add a `strip_trailing_slashes` helper. The `while (... name.back() == '/')` loop appears in `files_item_handler` now and in `zip_handler` in BLOCK 3, and §9 allows one duplicate.
- Do not compute a CRC-32 for plain downloads. The empty lambda in `send_download` is intentional.
- Do not change the "SD card nao montado." HTML text or `config.stack_size`.

### Verify
Run from the repository root, in this order:
```
mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log
grep -cE 'opendir|readdir|closedir|send_embedded|char decoded_name|char content_disposition\[300\]' components/web_server/web_server.cpp
grep -c 'dir_scan::visible_entries' components/web_server/web_server.cpp
mkdir -p build && rm -f build/plan-host.log build/plan-tests.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && export PATH="/usr/bin:$PATH" && idf.py -C host_tests build > build/plan-host.log 2>&1 && ./host_tests/build/ludo001_host_tests.elf > build/plan-tests.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-host.log; tail -3 build/plan-tests.log
```
Expected:
- First command: `exit=0`, no `warning:`/`error:` line, and `ludo001.bin binary size ... bytes (3%) free.` (measured while planning: `0x75e0 bytes` free, give or take 0x100 for the `.env` credentials).
- Second command: `3`. The 3 remaining matches are `char decoded_name[256];` in `upload_handler` and in `zip_handler`, plus `char content_disposition[300];` in `zip_handler`. BLOCK 3 removes all 3.
- Third command: `3`.
- Fourth command: `exit=0`, no `warning:`/`error:` line, `78 Tests 0 Failures 0 Ignored`, `OK`.

### If verification fails
1. Read the failing output in full (`build/plan-fw.log`).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set this block's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/web_server/web_server.cpp components/web_server/CMakeLists.txt PLAN.md
   git commit -m "Share directory walks and file streaming in web_server through dir_scan"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
