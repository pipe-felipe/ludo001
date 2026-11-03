## BLOCK 3 — Split the upload and zip handlers and publish content_version()

**Depends on:** BLOCK 2 committed
**Touches:** `components/web_server/web_server.cpp` (MODIFY), `components/web_server/include/web_server.hpp` (MODIFY)

### Goal
`upload_handler` and `zip_handler` delegate to short helpers with at most 3 parameters, and `web_server::content_version()` goes up by one after every upload or delete request that passed validation. The firmware still builds.

### Context to read first
1. `components/web_server/web_server.cpp`: the whole file, as BLOCK 2 left it. Each step names the exact text it replaces.
2. `components/web_server/include/web_server.hpp`: the declaration style (`UploadProgress upload_progress();` with its comment).
3. `components/web_content/include/web_server_zip.hpp`: `ZipEntry` and the `zip_append_*` functions this block calls.
4. §8 Security invariants 1, 3, 4 and 5.

Read exactly these. Do not open other files unless a step below names one.

### Steps
Every fenced code block below is literal: type it exactly as written, including the comments it carries over from the current file and its double-brace initialisers.

1. In `components/web_server/web_server.cpp`, replace the line `#include <cstdio>` with these 2 lines:
   ```cpp
   #include <atomic>
   #include <cstdio>
   ```
2. Immediately before the line `// Static assets embedded from web/ (see this component's CMakeLists.txt,`, insert:
   ```cpp
   // Bumped by the httpd task after every upload or delete, read by the
   // display loop to know when the card's item count and free space are stale.
   std::atomic<uint32_t> s_content_version{0};

   ```
3. Replace the functions `ensure_parent_dirs` and `upload_handler`, starting at the comment line that begins with `// Creates every directory in` and ending at the closing `}` of `upload_handler` (the `}` right after the `    return httpd_resp_sendstr(req, "OK");` that follows `    ESP_LOGI(kTag, "uploaded %s", final_name.c_str());`), with:
   ```cpp
   // Creates every directory in `relative_path`'s prefix on the card (e.g.
   // "a/b/c.txt" -> "a", then "a/b"), so a folder upload's nested files land
   // in the right place. Failures are ignored here: if a directory genuinely
   // can't be created, the fopen() that follows will fail and report it.
   void ensure_parent_dirs(const std::string& relative_path) {
       for (size_t slash = relative_path.find('/'); slash != std::string::npos;
            slash = relative_path.find('/', slash + 1)) {
           mkdir(sd_path(relative_path.substr(0, slash)).c_str(), 0755);
       }
   }

   // `name` if nothing on the card has that name yet, otherwise the first
   // free "1-<name>", "2-<name>", ...: an upload never overwrites a file or a
   // folder.
   std::string unused_name(const std::string& name) {
       std::string candidate = name;
       struct stat existing {};
       for (unsigned n = 1; stat(sd_path(candidate).c_str(), &existing) == 0; ++n) {
           candidate = numbered_name(name, n);
       }
       return candidate;
   }

   // Writes the request body to a new file at `path`, keeping the upload
   // progress up to date for the OLED. A partly written file is removed.
   // nullptr on success, otherwise the reason for the 500 response.
   const char* store_body(httpd_req_t* req, const std::string& path) {
       FILE* file = fopen(path.c_str(), "wb");
       if (file == nullptr) {
           return "could not open file for writing";
       }

       const int total = req->content_len;
       int remaining = total;
       const char* error = nullptr;
       set_progress(true, 0);

       char buffer[1024];
       while (remaining > 0 && error == nullptr) {
           int received = httpd_req_recv(req, buffer, remaining < static_cast<int>(sizeof(buffer)) ? remaining : sizeof(buffer));
           if (received == HTTPD_SOCK_ERR_TIMEOUT) {
               continue;
           }
           if (received <= 0) {
               error = "upload interrupted";
           } else if (fwrite(buffer, 1, received, file) != static_cast<size_t>(received)) {
               error = "could not write file";
           } else {
               remaining -= received;
               // 64-bit product: (total - remaining) * 100 overflows an int past 21 MB.
               set_progress(true, static_cast<uint8_t>(static_cast<uint64_t>(total - remaining) * 100 / total));
           }
       }
       fclose(file);
       set_progress(false, 0);
       if (error != nullptr) {
           remove(path.c_str());
       }
       return error;
   }

   // POST /upload/<path> -- writes the raw request body to the SD card. The
   // page's JS (web/app.js) sends the File object directly as the body, so
   // there's no multipart boundary to parse. <path> may be several segments
   // deep ("folder/sub/file.txt") for a folder upload; ensure_parent_dirs()
   // creates whatever directories that needs first. A name that already
   // exists on the card gets a number prefix instead of being overwritten.
   esp_err_t upload_handler(httpd_req_t* req) {
       std::string name = request_path(req, "/upload/");
       if (!sd_card::is_mounted() || !is_safe_filename(name.c_str())) {
           httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
           return ESP_FAIL;
       }

       ensure_parent_dirs(name);
       std::string final_name = unused_name(name);
       const char* error = store_body(req, sd_path(final_name));
       s_content_version++; // even a failed upload may have created folders
       if (error != nullptr) {
           httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, error);
           return ESP_FAIL;
       }

       ESP_LOGI(kTag, "uploaded %s", final_name.c_str());
       return httpd_resp_sendstr(req, "OK");
   }
   ```
   This changes 2 behaviours on purpose. A short `fwrite` (card full) now answers 500 `could not write file` and removes the partial file; before, it answered `OK` with a truncated file. The progress percentage is now computed in 64 bits; before, it overflowed for uploads over 21 MB.
4. In `delete_handler`, replace:
   ```cpp
       if (!remove_recursive(sd_path(name))) {
           httpd_resp_send_404(req);
   ```
   with:
   ```cpp
       bool removed = remove_recursive(sd_path(name));
       s_content_version++; // a delete that failed half way may still have removed items
       if (!removed) {
           httpd_resp_send_404(req);
   ```
5. Replace the functions `send_zip_record`, `send_zip_file_data` and `zip_handler`, starting at the 2-line comment directly above `bool send_zip_record(httpd_req_t* req, const std::string& record, uint32_t& offset) {` and ending at the closing `}` of `zip_handler` (the `}` right after the `    return ESP_OK;` that follows `    ESP_LOGI(kTag, "zipped %s (%u entries)", decoded_name, static_cast<unsigned>(entries.size()));`), with:
   ```cpp
   // The .zip being streamed as one response, with the archive's running
   // byte count that every record's offset is taken from.
   struct ZipStream {
       httpd_req_t* req;
       uint32_t offset = 0;

       // Sends `record` as one chunk and moves `offset` past it.
       bool send(const std::string& record) {
           offset += static_cast<uint32_t>(record.size());
           return httpd_resp_send_chunk(req, record.data(), record.size()) == ESP_OK;
       }
   };

   // Sends one file's bytes as the data of `entry`, computing its CRC-32 on
   // the way. False if the file can't be read in full or the client went
   // away, so the caller can abort instead of finishing a corrupt archive.
   bool send_zip_file_data(ZipStream& zip, const std::string& path, ZipEntry& entry) {
       FILE* file = fopen(path.c_str(), "rb");
       if (file == nullptr) {
           return false;
       }
       uint32_t crc = 0;
       uint32_t sent = 0;
       bool ok = send_file_chunks(zip.req, file, [&](const uint8_t* data, size_t len) {
           crc = crc32_update(crc, data, len);
           sent += static_cast<uint32_t>(len);
       });
       fclose(file);
       entry.crc32 = crc;
       zip.offset += sent;
       return ok && sent == entry.size;
   }

   // Sends every entry's local header and, for a file, its data and data
   // descriptor, filling in the offsets and CRC-32s the central directory
   // needs. False as soon as a file can't be read in full or the client goes
   // away. `parent_dir` + an entry's name is its SD card path.
   bool send_zip_entries(ZipStream& zip, const std::string& parent_dir, std::vector<ZipEntry>& entries) {
       std::string record;
       for (ZipEntry& entry : entries) {
           entry.local_header_offset = zip.offset;
           record.clear();
           zip_append_local_header(record, entry);
           if (!zip.send(record)) {
               return false;
           }
           if (entry.name.back() == '/') {
               continue;
           }
           if (!send_zip_file_data(zip, parent_dir + entry.name, entry)) {
               ESP_LOGE(kTag, "zip aborted at %s", entry.name.c_str());
               return false;
           }
           record.clear();
           zip_append_data_descriptor(record, entry);
           if (!zip.send(record)) {
               return false;
           }
       }
       return true;
   }

   // Sends the central directory and the record that ends the archive.
   bool send_zip_central_dir(ZipStream& zip, const std::vector<ZipEntry>& entries) {
       const uint32_t central_dir_offset = zip.offset;
       std::string record;
       for (const ZipEntry& entry : entries) {
           record.clear();
           zip_append_central_header(record, entry);
           if (!zip.send(record)) {
               return false;
           }
       }
       record.clear();
       zip_append_end_of_central_dir(
           record, static_cast<uint16_t>(entries.size()), zip.offset - central_dir_offset, central_dir_offset);
       return zip.send(record);
   }

   // GET /zip/<path> -- downloads the folder at <path>, and everything under
   // it, as one .zip built while it streams (records from web_server_zip.hpp),
   // so no file is ever held in memory. Entry names start with the folder's
   // own name, so the .zip unpacks into a folder of the same name. Once the
   // first chunk is out the status code can't change anymore: on a failure
   // the handler returns ESP_FAIL without the final empty chunk, httpd drops
   // the connection, and the browser marks the download as failed instead
   // of saving a truncated archive.
   esp_err_t zip_handler(httpd_req_t* req) {
       std::string name = request_path(req, "/zip/");
       // Strip every trailing slash: "/zip/pasta//" must not leave an empty
       // folder name, which would put "/..." names in the archive.
       while (!name.empty() && name.back() == '/') {
           name.pop_back();
       }

       if (!sd_card::is_mounted() || !is_safe_filename(name.c_str())) {
           httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid folder name or no SD card");
           return ESP_FAIL;
       }

       std::string full_dir = sd_path(name);
       struct stat st {};
       if (stat(full_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
           httpd_resp_send_404(req);
           return ESP_FAIL;
       }

       // Entry names are relative to the folder's parent ("fotos/a.jpg" for
       // /zip/viagem/fotos), so parent_dir + an entry's name is its SD path.
       std::string folder_name = name.substr(name.find_last_of('/') + 1); // no '/': npos + 1 wraps to 0
       std::string parent_dir = full_dir.substr(0, full_dir.size() - folder_name.size());

       std::vector<ZipEntry> entries;
       ZipEntry root;
       root.name = folder_name + "/";
       entries.push_back(root);
       collect_zip_entries(full_dir, root.name, entries);

       if (!zip_within_limits(entries)) {
           httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "folder too large for .zip (max 4 GB, 65535 items)");
           return ESP_FAIL;
       }

       // folder_name comes from a path stat() just found on the FAT card, and
       // FAT names can't contain '"' or control characters, so it can't break
       // out of the quoted header value. httpd keeps only a pointer to the
       // value, so it must live until the last chunk is out.
       httpd_resp_set_type(req, "application/zip");
       std::string content_disposition = "attachment; filename=\"" + folder_name + ".zip\"";
       httpd_resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());

       ZipStream zip{req};
       if (!send_zip_entries(zip, parent_dir, entries) || !send_zip_central_dir(zip, entries)) {
           return ESP_FAIL;
       }
       httpd_resp_send_chunk(req, nullptr, 0);

       ESP_LOGI(kTag, "zipped %s (%u entries)", name.c_str(), static_cast<unsigned>(entries.size()));
       return ESP_OK;
   }
   ```
6. At the end of the file, immediately after the closing `}` of `UploadProgress upload_progress()` and before the line `} // namespace web_server`, insert:
   ```cpp

   uint32_t content_version() {
       return s_content_version.load();
   }
   ```
7. In `components/web_server/include/web_server.hpp`, immediately after the line `UploadProgress upload_progress();`, insert:
   ```cpp

   // Goes up by one after every upload or delete, so other tasks -- e.g. the
   // OLED display loop -- re-read the card's item count and free space only
   // when it may have changed, instead of walking the card on every refresh.
   uint32_t content_version();
   ```

### Do not
- Do not protect `s_content_version` with `s_progress_lock` or a mutex. `std::atomic<uint32_t>` is the whole synchronisation.
- Do not bump `s_content_version` anywhere else: exactly 2 places, in `upload_handler` and in `delete_handler`. A 400 rejection returns before either bump.
- Do not turn `ZipStream` into a class with a constructor or private members, and do not give it more members than `req`, `offset` and `send`.
- Do not change the `413` or `400` messages, `zip_within_limits`, or any `zip_append_*` function.
- Do not touch `main/ludo001.cpp`. It starts calling `content_version()` in BLOCK 6.

### Verify
Run from the repository root, in this order:
```
mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log
grep -cE 'opendir|readdir|closedir|send_embedded|send_zip_record|char decoded_name|char path\[|char content_disposition|strchr|strrchr|snprintf' components/web_server/web_server.cpp
grep -c 's_content_version++' components/web_server/web_server.cpp
grep -c 'uint32_t content_version();' components/web_server/include/web_server.hpp
```
Expected:
- First command: `exit=0`, no `warning:`/`error:` line, and `ludo001.bin binary size ... bytes (3%) free.` (measured while planning: `0x75a0 bytes` free, give or take 0x100).
- Second command: `0`.
- Third command: `2`.
- Fourth command: `1`.

### If verification fails
1. Read the failing output in full (`build/plan-fw.log`).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set this block's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/web_server/web_server.cpp components/web_server/include/web_server.hpp PLAN.md
   git commit -m "Split the upload and zip handlers and count content changes"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
