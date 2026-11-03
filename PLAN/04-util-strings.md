## BLOCK 4 — Return std::string from url_encode, url_decode and format_size

**Depends on:** BLOCK 3 committed
**Touches:** `components/web_content/include/web_server_util.hpp` (MODIFY), `components/web_content/web_server_util.cpp` (MODIFY), `host_tests/main/test_web_server_util.cpp` (MODIFY), `host_tests/main/test_main.cpp` (MODIFY), `components/web_content/web_server_render.cpp` (MODIFY), `components/web_server/web_server.cpp` (MODIFY)
6 files: a signature change of 4 functions in `web_server_util.hpp`, with their tests and their only 3 call sites (`web_server_render.cpp` once, `web_server.cpp` in 2 functions plus 4 `is_safe_filename` calls). Every edit is listed.

### Goal
`url_decode`, `url_encode` and `format_size` return `std::string` and never truncate. `is_safe_filename` takes a `std::string_view` and rejects embedded NUL bytes. The host suite runs 80 passing tests.

### Context to read first
1. `components/web_content/include/web_server_util.hpp` and `components/web_content/web_server_util.cpp`: the current C-buffer versions.
2. `host_tests/main/test_web_server_util.cpp`: the tests being rewritten.
3. §8 Security invariant 2. Read it before writing `is_safe_filename`.

Read exactly these. Do not open other files unless a step below names one.

### Steps
Every fenced code block below is literal: type it exactly as written, including the comments it carries over from the current file and its double-brace initialisers.

1. Replace the whole content of `components/web_content/include/web_server_util.hpp` with:
   ```cpp
   #pragma once

   #include <string>
   #include <string_view>

   // Small string/path helpers for web_server's HTTP handlers. No ESP-IDF or
   // network dependency, so this builds and runs on the host (IDF_TARGET=linux)
   // for unit testing -- see host_tests/.
   namespace web_server {

   // Decodes %XX escapes (as produced by encodeURIComponent on the client).
   std::string url_decode(std::string_view in);

   // Percent-encodes everything outside the URL-safe unreserved set, so file
   // names with spaces or accents still work as href targets.
   std::string url_encode(std::string_view in);

   // Rejects anything that could escape the SD card's mount point, and any
   // NUL byte (a decoded "%00"), which would cut the path short.
   bool is_safe_filename(std::string_view name);

   // Puts "<n>-" in front of the file name at the end of `path`
   // ("a/b/foto.jpg", 2 -> "a/b/2-foto.jpg"): the name an upload gets when
   // `path` is already taken on the card.
   std::string numbered_name(const std::string& path, unsigned n);

   // Formats a byte count as "123 B", "4.5 KB" or "1.2 MB".
   std::string format_size(long size);

   } // namespace web_server
   ```
2. Replace the whole content of `components/web_content/web_server_util.cpp` with:
   ```cpp
   #include "web_server_util.hpp"

   #include <cstdio>

   namespace web_server {
   namespace {

   int hex_value(char c) {
       if (c >= '0' && c <= '9') return c - '0';
       if (c >= 'a' && c <= 'f') return c - 'a' + 10;
       if (c >= 'A' && c <= 'F') return c - 'A' + 10;
       return -1;
   }

   } // namespace

   std::string url_decode(std::string_view in) {
       std::string out;
       out.reserve(in.size());
       for (size_t i = 0; i < in.size(); ++i) {
           int hi = (in[i] == '%' && i + 2 < in.size()) ? hex_value(in[i + 1]) : -1;
           int lo = (hi >= 0) ? hex_value(in[i + 2]) : -1;
           if (hi >= 0 && lo >= 0) {
               out += static_cast<char>((hi << 4) | lo);
               i += 2;
           } else {
               out += in[i];
           }
       }
       return out;
   }

   std::string url_encode(std::string_view in) {
       constexpr char kHex[] = "0123456789ABCDEF";
       std::string out;
       out.reserve(in.size());
       for (char ch : in) {
           unsigned char c = static_cast<unsigned char>(ch);
           bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
           if (unreserved) {
               out += ch;
           } else {
               out += '%';
               out += kHex[c >> 4];
               out += kHex[c & 0xF];
           }
       }
       return out;
   }

   bool is_safe_filename(std::string_view name) {
       return !name.empty() && name[0] != '/' && name.find("..") == std::string_view::npos &&
              name.find('\0') == std::string_view::npos;
   }

   std::string numbered_name(const std::string& path, unsigned n) {
       size_t name_start = path.find_last_of('/') + 1; // no '/': npos + 1 wraps to 0, the whole path is the name
       return path.substr(0, name_start) + std::to_string(n) + "-" + path.substr(name_start);
   }

   std::string format_size(long size) {
       char out[24];
       if (size < 1024) {
           snprintf(out, sizeof(out), "%ld B", size);
       } else if (size < 1024 * 1024) {
           snprintf(out, sizeof(out), "%.1f KB", size / 1024.0);
       } else {
           snprintf(out, sizeof(out), "%.1f MB", size / (1024.0 * 1024.0));
       }
       return out;
   }

   } // namespace web_server
   ```
3. Replace the whole content of `host_tests/main/test_web_server_util.cpp` with:
   ```cpp
   #include "web_server_util.hpp"

   #include <string>

   extern "C" {
   #include "unity.h"
   }

   using namespace web_server;

   void test_url_encode_leaves_unreserved_untouched() {
       TEST_ASSERT_EQUAL_STRING("abcXYZ019-_.~", url_encode("abcXYZ019-_.~").c_str());
   }

   void test_url_encode_percent_encodes_space() {
       TEST_ASSERT_EQUAL_STRING("a%20b", url_encode("a b").c_str());
   }

   void test_url_encode_percent_encodes_multibyte_utf8() {
       TEST_ASSERT_EQUAL_STRING("caf%C3%A9", url_encode("caf\xc3\xa9").c_str()); // "café", e-acute as UTF-8
   }

   void test_url_encode_keeps_long_name_whole() {
       std::string name;
       std::string expected;
       for (int i = 0; i < 120; ++i) { // 240 bytes of "é": 720 once encoded
           name += "\xc3\xa9";
           expected += "%C3%A9";
       }
       TEST_ASSERT_EQUAL_STRING(expected.c_str(), url_encode(name).c_str());
   }

   void test_url_decode_plain_roundtrip() {
       TEST_ASSERT_EQUAL_STRING("abc", url_decode("abc").c_str());
   }

   void test_url_decode_percent_sequences() {
       TEST_ASSERT_EQUAL_STRING("a b/c", url_decode("a%20b%2Fc").c_str());
   }

   void test_url_decode_invalid_percent_sequence_passed_through() {
       TEST_ASSERT_EQUAL_STRING("100%", url_decode("100%").c_str());
   }

   void test_url_decode_percent_with_one_digit_at_end_passed_through() {
       TEST_ASSERT_EQUAL_STRING("a%4", url_decode("a%4").c_str());
   }

   void test_url_encode_decode_roundtrip() {
       const char* original = "arquivo com espa\xc3\xa7o.txt"; // has a space and a UTF-8 char
       TEST_ASSERT_EQUAL_STRING(original, url_decode(url_encode(original)).c_str());
   }

   void test_is_safe_filename_accepts_plain_name() {
       TEST_ASSERT_TRUE(is_safe_filename("file.txt"));
   }

   void test_is_safe_filename_rejects_empty() {
       TEST_ASSERT_FALSE(is_safe_filename(""));
   }

   void test_is_safe_filename_rejects_leading_slash() {
       TEST_ASSERT_FALSE(is_safe_filename("/etc/passwd"));
   }

   void test_is_safe_filename_rejects_dotdot() {
       TEST_ASSERT_FALSE(is_safe_filename("../secret.txt"));
       TEST_ASSERT_FALSE(is_safe_filename("a/../../b"));
   }

   void test_is_safe_filename_accepts_dot_in_middle() {
       TEST_ASSERT_TRUE(is_safe_filename("my.file.txt"));
   }

   void test_is_safe_filename_rejects_embedded_nul() {
       TEST_ASSERT_FALSE(is_safe_filename(url_decode("%00abc"))); // would open the card's root
       TEST_ASSERT_FALSE(is_safe_filename(url_decode("a.txt%00.jpg")));
   }

   void test_format_size_bytes() {
       TEST_ASSERT_EQUAL_STRING("0 B", format_size(0).c_str());
       TEST_ASSERT_EQUAL_STRING("1023 B", format_size(1023).c_str());
   }

   void test_format_size_kilobytes() {
       TEST_ASSERT_EQUAL_STRING("1.0 KB", format_size(1024).c_str());
       TEST_ASSERT_EQUAL_STRING("1.5 KB", format_size(1536).c_str());
   }

   void test_format_size_megabytes() {
       TEST_ASSERT_EQUAL_STRING("1.0 MB", format_size(1024L * 1024L).c_str());
   }

   void test_numbered_name_prefixes_plain_file_name() {
       TEST_ASSERT_EQUAL_STRING("1-foto.jpg", numbered_name("foto.jpg", 1).c_str());
   }

   void test_numbered_name_keeps_folder_path() {
       TEST_ASSERT_EQUAL_STRING("a/b/2-foto.jpg", numbered_name("a/b/foto.jpg", 2).c_str());
   }

   void test_numbered_name_keeps_dots_and_spaces() {
       TEST_ASSERT_EQUAL_STRING("12-upload test.tar.gz", numbered_name("upload test.tar.gz", 12).c_str());
   }
   ```
   `test_url_encode_truncates_safely_when_buffer_small` is replaced by `test_url_encode_keeps_long_name_whole`. The behaviour it pinned (truncating into a small buffer) no longer exists. This plan orders that replacement, so it is not an R6 violation.
4. In `host_tests/main/test_main.cpp`, make these 6 edits:
   - Replace the line `void test_url_encode_truncates_safely_when_buffer_small();` with `void test_url_encode_keeps_long_name_whole();`.
   - Immediately after the line `void test_url_decode_invalid_percent_sequence_passed_through();`, insert the line `void test_url_decode_percent_with_one_digit_at_end_passed_through();`.
   - Immediately after the line `void test_is_safe_filename_accepts_dot_in_middle();`, insert the line `void test_is_safe_filename_rejects_embedded_nul();`.
   - Replace the line `    RUN_TEST(test_url_encode_truncates_safely_when_buffer_small);` with `    RUN_TEST(test_url_encode_keeps_long_name_whole);`.
   - Immediately after the line `    RUN_TEST(test_url_decode_invalid_percent_sequence_passed_through);`, insert the line `    RUN_TEST(test_url_decode_percent_with_one_digit_at_end_passed_through);`.
   - Immediately after the line `    RUN_TEST(test_is_safe_filename_accepts_dot_in_middle);`, insert the line `    RUN_TEST(test_is_safe_filename_rejects_embedded_nul);`.
5. In `components/web_content/web_server_render.cpp`, inside `render_file_list`, replace:
   ```cpp
       char encoded_name[200];
       char item[1800];
       for (const FileEntry& entry : entries) {
           url_encode(entry.name.c_str(), encoded_name, sizeof(encoded_name));
           std::string rel_path = current_dir.empty() ? std::string(encoded_name) : current_dir + "/" + encoded_name;
   ```
   with:
   ```cpp
       char item[1800];
       for (const FileEntry& entry : entries) {
           std::string encoded_name = url_encode(entry.name);
           std::string rel_path = current_dir.empty() ? encoded_name : current_dir + "/" + encoded_name;
   ```
   Leave the rest of the loop (`snprintf` into `item`) unchanged. BLOCK 5 rewrites it.
6. In `components/web_server/web_server.cpp`, inside `request_path`, replace:
   ```cpp
       char decoded[256];
       url_decode(req->uri + strlen(route), decoded, sizeof(decoded));
       return decoded;
   ```
   with:
   ```cpp
       return url_decode(req->uri + strlen(route));
   ```
7. In `components/web_server/web_server.cpp`, inside `list_entries`, replace:
   ```cpp
               char size_text[24];
               format_size(static_cast<long>(st.st_size), size_text, sizeof(size_text));
               fe.size_text = size_text;
   ```
   with:
   ```cpp
               fe.size_text = format_size(static_cast<long>(st.st_size));
   ```
8. In `components/web_server/web_server.cpp`, replace each of the 4 occurrences of `is_safe_filename(name.c_str())` with `is_safe_filename(name)`. They are in `files_item_handler`, `upload_handler`, `delete_handler` and `zip_handler`. This is §8 invariant 2: passing `.c_str()` would hide an embedded NUL from the check.

### Do not
- Do not keep the old `char*` overloads next to the new functions. There is no other caller.
- Do not make `url_decode` reject or drop `%00`. It decodes every valid escape; `is_safe_filename` does the rejecting.
- Do not change `numbered_name` or `format_size`'s output format.
- Do not rewrite the `snprintf` row building in `render_file_list`. That is BLOCK 5.

### Verify
Run from the repository root, in this order:
```
mkdir -p build && rm -f build/plan-host.log build/plan-tests.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && export PATH="/usr/bin:$PATH" && idf.py -C host_tests build > build/plan-host.log 2>&1 && ./host_tests/build/ludo001_host_tests.elf > build/plan-tests.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-host.log; tail -3 build/plan-tests.log
grep -cE 'test_(url_encode_keeps_long_name_whole|url_decode_percent_with_one_digit_at_end_passed_through|is_safe_filename_rejects_embedded_nul):PASS' build/plan-tests.log
mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log
grep -c 'is_safe_filename(name)' components/web_server/web_server.cpp
grep -cE 'is_safe_filename\(name\.c_str\(\)\)|char decoded\[|char size_text\[' components/web_server/web_server.cpp
```
Expected:
- First command: `exit=0`, no `warning:`/`error:` line, `80 Tests 0 Failures 0 Ignored`, `OK`.
- Second command: `3`.
- Third command: `exit=0`, no `warning:`/`error:` line, and `ludo001.bin binary size ... bytes (3%) free.` (measured while planning: `0x7420 bytes` free, give or take 0x100).
- Fourth command: `4`.
- Fifth command: `0`.

### If verification fails
1. Read the failing output in full (`build/plan-host.log`, `build/plan-tests.log`, `build/plan-fw.log`).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set this block's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/web_content/include/web_server_util.hpp components/web_content/web_server_util.cpp host_tests/main/test_web_server_util.cpp host_tests/main/test_main.cpp components/web_content/web_server_render.cpp components/web_server/web_server.cpp PLAN.md
   git commit -m "Return std::string from the URL and size helpers"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
