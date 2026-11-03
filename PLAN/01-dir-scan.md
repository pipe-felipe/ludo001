## BLOCK 1 — Add the dir_scan component and its host tests

**Depends on:** none
**Touches:** `components/dir_scan/CMakeLists.txt` (NEW), `components/dir_scan/include/dir_scan.hpp` (NEW), `components/dir_scan/dir_scan.cpp` (NEW), `host_tests/main/test_dir_scan.cpp` (NEW), `host_tests/CMakeLists.txt` (MODIFY), `host_tests/main/CMakeLists.txt` (MODIFY), `host_tests/main/test_main.cpp` (MODIFY)
7 files: one new component (3 files) plus its test file and the 3 registration edits the host test project needs (§7 rules 1 and 2). Every path is listed.

### Goal
`dir_scan::visible_entries` and `dir_scan::count_entries` exist, and the host suite runs 6 new passing tests for them (78 in total).

### Context to read first
1. `components/web_content/CMakeLists.txt` and `components/web_content/include/web_server_util.hpp`: the pure-component pattern to mirror (comment at the top, `idf_component_register` with `SRCS` and `INCLUDE_DIRS` only, no `REQUIRES`).
2. `host_tests/main/test_web_server_render.cpp`: test style (`extern "C" { #include "unity.h" }`, one `void test_*()` per case, `TEST_ASSERT_*`).
3. `host_tests/CMakeLists.txt`, `host_tests/main/CMakeLists.txt`, `host_tests/main/test_main.cpp`: where components, sources and tests are registered.

Read exactly these. Do not open other files unless a step below names one.

### Steps
1. Create `components/dir_scan/CMakeLists.txt` with exactly:
   ```cmake
   # Pure POSIX directory walking, no hardware dependency -- builds for the
   # real target (esp32) and for IDF_TARGET=linux (host_tests/). sd_card and
   # web_server link this instead of each repeating the same readdir loop.
   idf_component_register(
       SRCS "dir_scan.cpp"
       INCLUDE_DIRS "include"
   )
   ```
2. Create `components/dir_scan/include/dir_scan.hpp` with exactly:
   ```cpp
   #pragma once

   #include <cstdint>
   #include <string>
   #include <vector>

   // Lists and counts what's inside a directory with plain POSIX
   // opendir/readdir, skipping every name that starts with '.' (dotfiles, and
   // dot-folders such as ".Trash-1000" together with everything inside them),
   // the same names the web file listing hides. No ESP-IDF dependency, so this
   // builds and runs on the host (IDF_TARGET=linux) for unit testing -- see
   // host_tests/.
   namespace dir_scan {

   struct Entry {
       std::string name; // name inside the directory, no path
       bool is_dir = false;
   };

   // The visible entries of `dir`, in readdir order; empty if `dir` can't be
   // opened. Only one directory is open at a time, and only during the call.
   std::vector<Entry> visible_entries(const std::string& dir);

   // Number of visible files and folders under `dir`, at any depth, not
   // counting `dir` itself.
   uint32_t count_entries(const std::string& dir);

   } // namespace dir_scan
   ```
3. Create `components/dir_scan/dir_scan.cpp` with exactly:
   ```cpp
   #include "dir_scan.hpp"

   #include <dirent.h>

   namespace dir_scan {

   std::vector<Entry> visible_entries(const std::string& dir) {
       std::vector<Entry> entries;
       DIR* handle = opendir(dir.c_str());
       if (handle == nullptr) {
           return entries;
       }
       struct dirent* entry;
       while ((entry = readdir(handle)) != nullptr) {
           if (entry->d_name[0] == '.') {
               continue;
           }
           entries.push_back({entry->d_name, entry->d_type == DT_DIR});
       }
       closedir(handle);
       return entries;
   }

   uint32_t count_entries(const std::string& dir) {
       uint32_t count = 0;
       std::vector<std::string> pending = {dir}; // a list, not recursion: one open directory at a time
       while (!pending.empty()) {
           std::string current = std::move(pending.back());
           pending.pop_back();
           for (const Entry& entry : visible_entries(current)) {
               ++count;
               if (entry.is_dir) {
                   pending.push_back(current + "/" + entry.name);
               }
           }
       }
       return count;
   }

   } // namespace dir_scan
   ```
4. Create `host_tests/main/test_dir_scan.cpp` with exactly:
   ```cpp
   #include "dir_scan.hpp"

   #include <algorithm>
   #include <filesystem>
   #include <fstream>

   extern "C" {
   #include "unity.h"
   }

   namespace fs = std::filesystem;

   namespace {

   // An empty scratch directory; each test starts by recreating it, so one
   // left behind by a failed test does no harm.
   fs::path fresh_dir() {
       fs::path dir = fs::temp_directory_path() / "ludo001_dir_scan_test";
       fs::remove_all(dir);
       fs::create_directories(dir);
       return dir;
   }

   void make_file(const fs::path& path) {
       std::ofstream(path) << "x";
   }

   const dir_scan::Entry* find(const std::vector<dir_scan::Entry>& entries, const char* name) {
       auto it = std::find_if(entries.begin(), entries.end(), [&](const dir_scan::Entry& e) { return e.name == name; });
       return it == entries.end() ? nullptr : &*it;
   }

   } // namespace

   void test_visible_entries_skips_dot_names() {
       fs::path dir = fresh_dir();
       make_file(dir / "a.txt");
       make_file(dir / ".hidden");
       fs::create_directory(dir / ".Trash-1000");
       std::vector<dir_scan::Entry> entries = dir_scan::visible_entries(dir.string());
       TEST_ASSERT_EQUAL_UINT(1, entries.size());
       TEST_ASSERT_EQUAL_STRING("a.txt", entries[0].name.c_str());
       fs::remove_all(dir);
   }

   void test_visible_entries_marks_directories() {
       fs::path dir = fresh_dir();
       make_file(dir / "a.txt");
       fs::create_directory(dir / "pasta");
       std::vector<dir_scan::Entry> entries = dir_scan::visible_entries(dir.string());
       TEST_ASSERT_EQUAL_UINT(2, entries.size());
       TEST_ASSERT_NOT_NULL(find(entries, "a.txt"));
       TEST_ASSERT_NOT_NULL(find(entries, "pasta"));
       TEST_ASSERT_FALSE(find(entries, "a.txt")->is_dir);
       TEST_ASSERT_TRUE(find(entries, "pasta")->is_dir);
       fs::remove_all(dir);
   }

   void test_visible_entries_of_missing_dir_is_empty() {
       fs::path dir = fresh_dir();
       TEST_ASSERT_EQUAL_UINT(0, dir_scan::visible_entries((dir / "missing").string()).size());
       fs::remove_all(dir);
   }

   void test_count_entries_of_empty_dir_is_zero() {
       fs::path dir = fresh_dir();
       TEST_ASSERT_EQUAL_UINT32(0, dir_scan::count_entries(dir.string()));
       fs::remove_all(dir);
   }

   void test_count_entries_counts_files_and_folders_at_every_depth() {
       fs::path dir = fresh_dir();
       make_file(dir / "a.txt");
       fs::create_directories(dir / "pasta" / "sub");
       make_file(dir / "pasta" / "b.txt");
       make_file(dir / "pasta" / "sub" / "c.txt");
       TEST_ASSERT_EQUAL_UINT32(5, dir_scan::count_entries(dir.string())); // a.txt, pasta, b.txt, sub, c.txt
       fs::remove_all(dir);
   }

   void test_count_entries_skips_hidden_folders_and_their_contents() {
       fs::path dir = fresh_dir();
       make_file(dir / "a.txt");
       make_file(dir / ".hidden");
       fs::create_directory(dir / ".Trash-1000");
       make_file(dir / ".Trash-1000" / "old.txt");
       TEST_ASSERT_EQUAL_UINT32(1, dir_scan::count_entries(dir.string()));
       fs::remove_all(dir);
   }
   ```
5. In `host_tests/CMakeLists.txt`, immediately after the line `    "${CMAKE_CURRENT_LIST_DIR}/../components/web_content"`, insert this line:
   ```cmake
       "${CMAKE_CURRENT_LIST_DIR}/../components/dir_scan"
   ```
6. In `host_tests/main/CMakeLists.txt`, make 3 edits:
   - Replace `# (framebuffer, web_content). Runs natively on this machine` with `# (framebuffer, web_content, dir_scan). Runs natively on this machine`.
   - Replace `"test_web_server_zip.cpp"` with `"test_web_server_zip.cpp" "test_dir_scan.cpp"`.
   - Replace `REQUIRES unity framebuffer web_content` with `REQUIRES unity framebuffer web_content dir_scan`.
7. In `host_tests/main/test_main.cpp`, immediately after the line `void test_zip_within_limits_rejects_too_many_entries();`, insert:
   ```cpp

   // dir_scan
   void test_visible_entries_skips_dot_names();
   void test_visible_entries_marks_directories();
   void test_visible_entries_of_missing_dir_is_empty();
   void test_count_entries_of_empty_dir_is_zero();
   void test_count_entries_counts_files_and_folders_at_every_depth();
   void test_count_entries_skips_hidden_folders_and_their_contents();
   ```
8. In `host_tests/main/test_main.cpp`, immediately after the line `    RUN_TEST(test_zip_within_limits_rejects_too_many_entries);`, insert:
   ```cpp

       RUN_TEST(test_visible_entries_skips_dot_names);
       RUN_TEST(test_visible_entries_marks_directories);
       RUN_TEST(test_visible_entries_of_missing_dir_is_empty);
       RUN_TEST(test_count_entries_of_empty_dir_is_zero);
       RUN_TEST(test_count_entries_counts_files_and_folders_at_every_depth);
       RUN_TEST(test_count_entries_skips_hidden_folders_and_their_contents);
   ```
   The line `    std::exit(UNITY_END());` stays the last statement of `app_main`, after a blank line.

### Do not
- Do not add a `stat()` fallback for `DT_UNKNOWN`, a callback-based walker, a `Walker` class or any option parameter. ESP-IDF's FAT VFS always sets `d_type` (`~/.espressif/v6.1/esp-idf/components/fatfs/vfs/vfs_fat.c:1085-1089`), and the host test directory lives on tmpfs, which sets it too.
- Do not make `count_entries` recursive. The explicit `pending` list keeps one directory open at a time on the 6144-byte main task stack.
- Do not touch `components/sd_card/` or `components/web_server/`. They start using `dir_scan` in BLOCK 2 and BLOCK 6.

### Verify
Run from the repository root, in this order:
```
mkdir -p build && rm -f build/plan-host.log build/plan-tests.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && export PATH="/usr/bin:$PATH" && idf.py -C host_tests build > build/plan-host.log 2>&1 && ./host_tests/build/ludo001_host_tests.elf > build/plan-tests.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-host.log; tail -3 build/plan-tests.log
grep -cE 'test_(visible|count)_entries_.*:PASS' build/plan-tests.log
mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log
```
Expected:
- First command: `exit=0`, no `warning:`/`error:` line, then `78 Tests 0 Failures 0 Ignored` and `OK`.
- Second command: `6`.
- Third command: `exit=0`, no `warning:`/`error:` line, and a `ludo001.bin binary size ...` line.

### If verification fails
1. Read the failing output in full (`build/plan-host.log`, `build/plan-tests.log`, `build/plan-fw.log`).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set this block's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/dir_scan/CMakeLists.txt components/dir_scan/include/dir_scan.hpp components/dir_scan/dir_scan.cpp host_tests/main/test_dir_scan.cpp host_tests/CMakeLists.txt host_tests/main/CMakeLists.txt host_tests/main/test_main.cpp PLAN.md
   git commit -m "Add dir_scan to list and count directory entries without ESP-IDF"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
