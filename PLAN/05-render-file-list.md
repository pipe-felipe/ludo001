## BLOCK 5 — Build the file list rows without fixed buffers

**Depends on:** BLOCK 4 committed
**Touches:** `components/web_content/web_server_render.cpp` (MODIFY), `host_tests/main/test_web_server_render.cpp` (MODIFY), `host_tests/main/test_main.cpp` (MODIFY)

### Goal
`render_file_list` appends each row to a `std::string` (no `char item[1800]`), produces byte-identical markup for normal names, keeps long encoded names whole, and the host suite runs 83 passing tests.

### Context to read first
1. `components/web_content/web_server_render.cpp`: `render_file_list` as BLOCK 4 left it.
2. `host_tests/main/test_web_server_render.cpp`: the existing render tests and their style.
3. §7 rule 3: the row markup must not change.

Read exactly these. Do not open other files unless a step below names one.

### Steps
Every fenced code block below is literal: type it exactly as written, including the comments it carries over from the current file and its double-brace initialisers.

1. In `components/web_content/web_server_render.cpp`, replace the line `namespace web_server {` (it occurs once) with:
   ```cpp
   namespace web_server {
   namespace {

   // Appends one <li> of the listing. `rel_path` is the entry's encoded path
   // from the card's root: a folder opens with a trailing '/' and downloads
   // as a .zip, a file opens and downloads as itself.
   void append_entry(std::string& out, const FileEntry& entry, const std::string& rel_path) {
       const char* dir_suffix = entry.is_dir ? "/" : "";
       out += entry.is_dir ? "<li><a class=\"dir\" href=\"/files/" : "<li><a href=\"/files/";
       out += rel_path;
       out += dir_suffix;
       out += "\">";
       out += entry.name;
       out += dir_suffix;
       out += "</a><span class=\"size\">";
       out += entry.size_text;
       out += entry.is_dir ? "</span><a class=\"zip\" href=\"/zip/" : "</span><a class=\"zip\" href=\"/files/";
       out += rel_path;
       out += "\">Baixar</a><button class=\"del\" data-name=\"";
       out += rel_path;
       out += "\" onclick=\"del(this)\">Apagar</button></li>";
   }

   } // namespace
   ```
2. In the same file, inside `render_file_list`, replace every line from `    char item[1800];` through the line `        out += item;` and the `    }` that closes the `for` loop right after it, with:
   ```cpp
       for (const FileEntry& entry : entries) {
           std::string encoded_name = url_encode(entry.name);
           append_entry(out, entry, current_dir.empty() ? encoded_name : current_dir + "/" + encoded_name);
       }
   ```
   The lines `    out += "<ul>";` before it and `    out += "</ul>";` after it stay. The "up" link code and the `Pasta vazia.` branch at the top of `render_file_list` stay unchanged.
3. In `host_tests/main/test_web_server_render.cpp`, immediately before the line `void test_render_dir_title_root() {`, insert:
   ```cpp
   void test_render_file_list_file_row_markup() {
       std::vector<FileEntry> entries = {{"a.txt", "1 B"}};
       TEST_ASSERT_EQUAL_STRING("<ul><li><a href=\"/files/a.txt\">a.txt</a><span class=\"size\">1 B</span>"
                                "<a class=\"zip\" href=\"/files/a.txt\">Baixar</a>"
                                "<button class=\"del\" data-name=\"a.txt\" onclick=\"del(this)\">Apagar</button></li></ul>",
           render_file_list(entries, "").c_str());
   }

   void test_render_file_list_directory_row_markup() {
       FileEntry dir_entry;
       dir_entry.name = "pasta";
       dir_entry.is_dir = true;
       TEST_ASSERT_EQUAL_STRING("<ul><li><a class=\"dir\" href=\"/files/pasta/\">pasta/</a><span class=\"size\"></span>"
                                "<a class=\"zip\" href=\"/zip/pasta\">Baixar</a>"
                                "<button class=\"del\" data-name=\"pasta\" onclick=\"del(this)\">Apagar</button></li></ul>",
           render_file_list({dir_entry}, "").c_str());
   }

   void test_render_file_list_keeps_long_accented_name_whole() {
       std::string name;
       std::string encoded;
       for (int i = 0; i < 100; ++i) { // 200 bytes of "é", 600 once encoded
           name += "\xc3\xa9";
           encoded += "%C3%A9";
       }
       std::vector<FileEntry> entries = {{name + ".txt", "1 B"}};
       std::string html = render_file_list(entries, "");
       TEST_ASSERT_NOT_NULL(strstr(html.c_str(), ("href=\"/files/" + encoded + ".txt\"").c_str()));
       TEST_ASSERT_NOT_NULL(strstr(html.c_str(), ("data-name=\"" + encoded + ".txt\"").c_str()));
       TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "</button></li></ul>"));
   }

   ```
   The 2 exact strings were printed by the current `render_file_list` while planning. They pin today's markup.
4. In `host_tests/main/test_main.cpp`, immediately after the line `void test_render_file_list_nested_file_download_link_includes_current_dir();`, insert:
   ```cpp
   void test_render_file_list_file_row_markup();
   void test_render_file_list_directory_row_markup();
   void test_render_file_list_keeps_long_accented_name_whole();
   ```
5. In `host_tests/main/test_main.cpp`, immediately after the line `    RUN_TEST(test_render_file_list_nested_file_download_link_includes_current_dir);`, insert:
   ```cpp
       RUN_TEST(test_render_file_list_file_row_markup);
       RUN_TEST(test_render_file_list_directory_row_markup);
       RUN_TEST(test_render_file_list_keeps_long_accented_name_whole);
   ```

### Do not
- Do not move the row markup into `components/web_server/web/` or a template file. That move is not part of this task; the markup stays in `web_server_render.cpp`.
- Do not change a class name, attribute, text (`Baixar`, `Apagar`) or attribute order in the markup. The 2 exact-string tests must pass unchanged.
- Do not percent-encode `current_dir` (§3).
- Do not change `render_wifi_line`, `render_sd_line`, `render_dir_title` or `render_template`.

### Verify
Run from the repository root, in this order:
```
mkdir -p build && rm -f build/plan-host.log build/plan-tests.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && export PATH="/usr/bin:$PATH" && idf.py -C host_tests build > build/plan-host.log 2>&1 && ./host_tests/build/ludo001_host_tests.elf > build/plan-tests.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-host.log; tail -3 build/plan-tests.log
grep -cE 'test_render_file_list_(file_row_markup|directory_row_markup|keeps_long_accented_name_whole):PASS' build/plan-tests.log
grep -cE 'char item\[|char encoded_name\[' components/web_content/web_server_render.cpp
mkdir -p build && rm -f build/plan-fw.log; source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && idf.py build > build/plan-fw.log 2>&1; echo "exit=$?"; grep -E 'warning:|error:' build/plan-fw.log; grep 'ludo001.bin binary size' build/plan-fw.log
```
Expected:
- First command: `exit=0`, no `warning:`/`error:` line, `83 Tests 0 Failures 0 Ignored`, `OK`.
- Second command: `3`.
- Third command: `0`.
- Fourth command: `exit=0`, no `warning:`/`error:` line, and a `ludo001.bin binary size ... bytes (3%) free.` line.

### If verification fails
1. Read the failing output in full (`build/plan-host.log`, `build/plan-tests.log`, `build/plan-fw.log`).
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set this block's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/web_content/web_server_render.cpp host_tests/main/test_web_server_render.cpp host_tests/main/test_main.cpp PLAN.md
   git commit -m "Build file list rows without fixed-size buffers"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
