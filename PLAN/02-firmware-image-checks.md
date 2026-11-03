## BLOCK 2 — Add the firmware image checks to web_content, with host tests

**Depends on:** BLOCK 1 committed
**Touches:** `components/web_content/include/web_server_firmware.hpp` (NEW), `components/web_content/web_server_firmware.cpp` (NEW), `components/web_content/CMakeLists.txt` (MODIFY), `host_tests/main/test_web_server_firmware.cpp` (NEW), `host_tests/main/CMakeLists.txt` (MODIFY), `host_tests/main/test_main.cpp` (MODIFY)
6 files: one new pure module and its test file, plus the 3 registration edits the two builds need (one `SRCS` entry each, and the declarations and `RUN_TEST` calls).

### Goal
`firmware_size_fits` and `firmware_head_matches` exist in `web_content`, and 9 new host tests pass, for 92 in total.

### Context to read first
1. `components/web_content/include/web_server_zip.hpp`: the header style to mirror (`#pragma once`, a block comment that says why the module has no ESP-IDF dependency, `namespace web_server`).
2. `components/web_content/web_server_zip.cpp:1-40`: constants style (`constexpr` `k...` names with short trailing comments, inside an anonymous namespace).
3. `host_tests/main/test_web_server_zip.cpp:1-24`: test file style (`extern "C"` Unity include, `using namespace web_server;`, helpers in an anonymous namespace).
4. `host_tests/main/test_main.cpp`: tests are declared under a `// <module>` comment and registered with `RUN_TEST`, in the same order.

Read exactly these. Do not open other files unless a step below names one.

### Steps
1. Create `components/web_content/include/web_server_firmware.hpp` with exactly:
   ```cpp
   #pragma once

   #include <cstddef>
   #include <string_view>

   // Checks POST /firmware runs on an uploaded image before anything is
   // written to flash. An ESP-IDF app image starts with a 24-byte image
   // header and an 8-byte segment header, followed by the 256-byte
   // esp_app_desc_t that holds the project name. No ESP-IDF or network
   // dependency, so this builds and runs on the host (IDF_TARGET=linux) for
   // unit testing -- see host_tests/.
   namespace web_server {

   // How many bytes from the start of an image firmware_head_matches() reads.
   constexpr size_t kFirmwareHeadSize = 24 + 8 + 256;

   // True if an image of `image_size` bytes fits an update partition of
   // `partition_size` bytes: not empty, and no larger than the partition.
   bool firmware_size_fits(size_t image_size, size_t partition_size);

   // True if `head`, the first kFirmwareHeadSize bytes of an upload, starts
   // an app image built for the project called `project_name`. Says nothing
   // about the rest of the image: esp_ota_end() checks that, signature
   // included.
   bool firmware_head_matches(std::string_view head, std::string_view project_name);

   } // namespace web_server
   ```
2. Create `components/web_content/web_server_firmware.cpp` with exactly:
   ```cpp
   #include "web_server_firmware.hpp"

   #include <cstdint>

   namespace web_server {
   namespace {

   constexpr uint8_t kImageMagic = 0xE9;          // ESP_IMAGE_HEADER_MAGIC, the image's first byte
   constexpr size_t kAppDescOffset = 24 + 8;      // esp_app_desc_t follows the image and segment headers
   constexpr uint32_t kAppDescMagic = 0xABCD5432; // ESP_APP_DESC_MAGIC_WORD, esp_app_desc_t's first field
   constexpr size_t kProjectNameOffset = kAppDescOffset + 48; // after magic_word, secure_version, reserv1[2], version[32]
   constexpr size_t kProjectNameSize = 32;        // char[32], NUL-padded

   uint32_t read_le32(std::string_view bytes, size_t pos) {
       uint32_t value = 0;
       for (size_t i = 0; i < 4; ++i) {
           value |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[pos + i])) << (8 * i);
       }
       return value;
   }

   } // namespace

   bool firmware_size_fits(size_t image_size, size_t partition_size) {
       return image_size > 0 && image_size <= partition_size;
   }

   bool firmware_head_matches(std::string_view head, std::string_view project_name) {
       if (head.size() < kFirmwareHeadSize || static_cast<uint8_t>(head[0]) != kImageMagic ||
           read_le32(head, kAppDescOffset) != kAppDescMagic) {
           return false;
       }
       std::string_view name = head.substr(kProjectNameOffset, kProjectNameSize);
       return name.substr(0, name.find('\0')) == project_name; // npos: all 32 bytes are the name
   }

   } // namespace web_server
   ```
3. In `components/web_content/CMakeLists.txt`, replace the line
   `    SRCS "web_server_util.cpp" "web_server_render.cpp" "web_server_zip.cpp"`
   with
   `    SRCS "web_server_util.cpp" "web_server_render.cpp" "web_server_zip.cpp" "web_server_firmware.cpp"`
4. Create `host_tests/main/test_web_server_firmware.cpp` with exactly:
   ```cpp
   #include "web_server_firmware.hpp"

   #include <cstring>
   #include <string>

   extern "C" {
   #include "unity.h"
   }

   using namespace web_server;

   namespace {

   constexpr size_t kPartitionSize = 1700 * 1024; // ota_0 and ota_1 in partitions_two_ota_large.csv

   // The first kFirmwareHeadSize bytes of an app image built for
   // `project_name`, laid out like build/ludo001.bin: image magic at 0, app
   // description magic at 32, project name at 80.
   std::string image_head(const char* project_name) {
       std::string head(kFirmwareHeadSize, '\0');
       head[0] = static_cast<char>(0xE9);
       const char app_desc_magic[] = {'\x32', '\x54', '\xCD', '\xAB'}; // 0xABCD5432, little-endian
       head.replace(32, sizeof(app_desc_magic), app_desc_magic, sizeof(app_desc_magic));
       head.replace(80, strlen(project_name), project_name);
       return head;
   }

   } // namespace

   void test_firmware_size_fits_up_to_partition_size() {
       TEST_ASSERT_TRUE(firmware_size_fits(1, kPartitionSize));
       TEST_ASSERT_TRUE(firmware_size_fits(kPartitionSize, kPartitionSize));
   }

   void test_firmware_size_fits_rejects_empty_image() {
       TEST_ASSERT_FALSE(firmware_size_fits(0, kPartitionSize));
   }

   void test_firmware_size_fits_rejects_image_larger_than_partition() {
       TEST_ASSERT_FALSE(firmware_size_fits(kPartitionSize + 1, kPartitionSize));
   }

   void test_firmware_head_matches_own_project() {
       TEST_ASSERT_TRUE(firmware_head_matches(image_head("ludo001"), "ludo001"));
   }

   void test_firmware_head_matches_rejects_other_project() {
       TEST_ASSERT_FALSE(firmware_head_matches(image_head("esp-idf-hello"), "ludo001"));
   }

   void test_firmware_head_matches_rejects_longer_or_shorter_name() {
       TEST_ASSERT_FALSE(firmware_head_matches(image_head("ludo0012"), "ludo001"));
       TEST_ASSERT_FALSE(firmware_head_matches(image_head("ludo"), "ludo001"));
   }

   void test_firmware_head_matches_rejects_bad_image_magic() {
       std::string head = image_head("ludo001");
       head[0] = '\0';
       TEST_ASSERT_FALSE(firmware_head_matches(head, "ludo001"));
   }

   void test_firmware_head_matches_rejects_bad_app_desc_magic() {
       std::string head = image_head("ludo001");
       head[32] = '\0';
       TEST_ASSERT_FALSE(firmware_head_matches(head, "ludo001"));
   }

   void test_firmware_head_matches_rejects_short_head() {
       std::string head = image_head("ludo001");
       head.pop_back();
       TEST_ASSERT_FALSE(firmware_head_matches(head, "ludo001"));
   }
   ```
5. In `host_tests/main/CMakeLists.txt`, replace the line
   `    SRCS "test_main.cpp" "test_framebuffer.cpp" "test_web_server_util.cpp" "test_web_server_render.cpp" "test_web_server_zip.cpp" "test_dir_scan.cpp"`
   with
   `    SRCS "test_main.cpp" "test_framebuffer.cpp" "test_web_server_util.cpp" "test_web_server_render.cpp" "test_web_server_zip.cpp" "test_dir_scan.cpp" "test_web_server_firmware.cpp"`
6. In `host_tests/main/test_main.cpp`, immediately after the line `void test_count_entries_skips_hidden_folders_and_their_contents();`, insert:
   ```cpp

   // web_server_firmware
   void test_firmware_size_fits_up_to_partition_size();
   void test_firmware_size_fits_rejects_empty_image();
   void test_firmware_size_fits_rejects_image_larger_than_partition();
   void test_firmware_head_matches_own_project();
   void test_firmware_head_matches_rejects_other_project();
   void test_firmware_head_matches_rejects_longer_or_shorter_name();
   void test_firmware_head_matches_rejects_bad_image_magic();
   void test_firmware_head_matches_rejects_bad_app_desc_magic();
   void test_firmware_head_matches_rejects_short_head();
   ```
7. In `host_tests/main/test_main.cpp`, immediately after the line `    RUN_TEST(test_count_entries_skips_hidden_folders_and_their_contents);`, insert:
   ```cpp

       RUN_TEST(test_firmware_size_fits_up_to_partition_size);
       RUN_TEST(test_firmware_size_fits_rejects_empty_image);
       RUN_TEST(test_firmware_size_fits_rejects_image_larger_than_partition);
       RUN_TEST(test_firmware_head_matches_own_project);
       RUN_TEST(test_firmware_head_matches_rejects_other_project);
       RUN_TEST(test_firmware_head_matches_rejects_longer_or_shorter_name);
       RUN_TEST(test_firmware_head_matches_rejects_bad_image_magic);
       RUN_TEST(test_firmware_head_matches_rejects_bad_app_desc_magic);
       RUN_TEST(test_firmware_head_matches_rejects_short_head);
   ```

### Do not
- Do not include any `esp_*` header in the two new `web_content` files. They must build for `IDF_TARGET=linux` (§7 rule 1).
- Do not add a function that parses the version or other `esp_app_desc_t` fields. Only the project name is checked (§9).
- Do not touch `components/web_server/`; the route is BLOCK 4.
- Do not change any existing test.

### Verify
Run from the repository root, in this order:
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; export PATH="/usr/bin:$PATH"; idf.py -C host_tests build > build/host-tests-build.log 2>&1; echo "host build exit=$?"; ./host_tests/build/ludo001_host_tests.elf | tail -3
grep -c 'RUN_TEST(test_firmware_' host_tests/main/test_main.cpp
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py build > build/fw-build.log 2>&1; echo "fw build exit=$?"; grep -E 'warning:|binary size' build/fw-build.log
```
Expected:
1. `host build exit=0`, `92 Tests 0 Failures 0 Ignored`, `OK`.
2. `9`.
3. `fw build exit=0`, no `warning:` line, and a `ludo001.bin binary size ...` line that shows partition `0x1a9000` with at least `0x90000` bytes free.

### If verification fails
1. Read the failing output in full (for a build failure, `build/host-tests-build.log` or `build/fw-build.log`).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set BLOCK 2's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/web_content/include/web_server_firmware.hpp components/web_content/web_server_firmware.cpp components/web_content/CMakeLists.txt host_tests/main/test_web_server_firmware.cpp host_tests/main/CMakeLists.txt host_tests/main/test_main.cpp PLAN.md
   git commit -m "Add firmware image size and header checks to web_content"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
