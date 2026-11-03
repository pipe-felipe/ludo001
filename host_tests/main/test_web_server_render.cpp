#include "web_server_render.hpp"

#include <cstring>

extern "C" {
#include "unity.h"
}

using namespace web_server;

void test_render_wifi_line_connected() {
    std::string line = render_wifi_line(true, "192.168.1.5");
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "pill ok"));
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "192.168.1.5"));
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "conectado"));
}

void test_render_wifi_line_disconnected() {
    std::string line = render_wifi_line(false, "");
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "pill bad"));
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "desconectado"));
}

void test_render_sd_line_mounted() {
    std::string line = render_sd_line(true, 30436);
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "pill ok"));
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "30436 MB"));
}

void test_render_sd_line_not_mounted() {
    std::string line = render_sd_line(false, 0);
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "pill bad"));
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "nao montado"));
}

void test_render_file_list_empty() {
    std::string html = render_file_list({}, "");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "Pasta vazia"));
}

void test_render_file_list_single_entry() {
    std::vector<FileEntry> entries = {{"log.txt", "36 B"}};
    std::string html = render_file_list(entries, "");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/log.txt\""));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), ">log.txt<"));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "36 B"));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "data-name=\"log.txt\""));
}

void test_render_file_list_encodes_name_with_space_in_href() {
    std::vector<FileEntry> entries = {{"my file.txt", "1.0 KB"}};
    std::string html = render_file_list(entries, "");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/my%20file.txt\""));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), ">my file.txt<")); // display text stays unencoded
}

void test_render_file_list_multiple_entries_all_present() {
    std::vector<FileEntry> entries = {{"a.txt", "1 B"}, {"b.txt", "2 B"}};
    std::string html = render_file_list(entries, "");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "a.txt"));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "b.txt"));
}

void test_render_file_list_root_has_no_up_link() {
    std::vector<FileEntry> entries = {{"a.txt", "1 B"}};
    std::string html = render_file_list(entries, "");
    TEST_ASSERT_NULL(strstr(html.c_str(), "class=\"up\""));
}

void test_render_file_list_subdir_has_up_link_to_parent() {
    std::vector<FileEntry> entries = {{"a.txt", "1 B"}};
    std::string html = render_file_list(entries, "folder1/folder2");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/folder1\">"));
}

void test_render_file_list_top_level_subdir_up_link_goes_to_root() {
    std::vector<FileEntry> entries = {{"a.txt", "1 B"}};
    std::string html = render_file_list(entries, "folder1");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/\">"));
}

void test_render_file_list_directory_entry_has_trailing_slash_and_no_size() {
    std::vector<FileEntry> entries;
    FileEntry dir_entry;
    dir_entry.name = "pasta";
    dir_entry.is_dir = true;
    entries.push_back(dir_entry);
    std::string html = render_file_list(entries, "");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/pasta/\""));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "class=\"dir\""));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "<span class=\"size\"></span>"));
}

void test_render_file_list_nested_entry_includes_current_dir_in_paths() {
    std::vector<FileEntry> entries = {{"file.txt", "5 B"}};
    std::string html = render_file_list(entries, "folder1");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/folder1/file.txt\""));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "data-name=\"folder1/file.txt\""));
}

void test_render_file_list_directory_entry_has_zip_link() {
    std::vector<FileEntry> entries;
    FileEntry dir_entry;
    dir_entry.name = "pasta";
    dir_entry.is_dir = true;
    entries.push_back(dir_entry);
    std::string html = render_file_list(entries, "");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "<a class=\"zip\" href=\"/zip/pasta\">Baixar</a>"));
}

void test_render_file_list_nested_directory_zip_link_includes_current_dir() {
    std::vector<FileEntry> entries;
    FileEntry dir_entry;
    dir_entry.name = "sub";
    dir_entry.is_dir = true;
    entries.push_back(dir_entry);
    std::string html = render_file_list(entries, "folder1");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/zip/folder1/sub\""));
}

void test_render_file_list_file_entry_has_download_link() {
    std::vector<FileEntry> entries = {{"a.txt", "1 B"}};
    std::string html = render_file_list(entries, "");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "<a class=\"zip\" href=\"/files/a.txt\">Baixar</a>"));
    TEST_ASSERT_NULL(strstr(html.c_str(), "/zip/")); // a single file is served as is, not zipped
}

void test_render_file_list_nested_file_download_link_includes_current_dir() {
    std::vector<FileEntry> entries = {{"file.txt", "5 B"}};
    std::string html = render_file_list(entries, "folder1");
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "<a class=\"zip\" href=\"/files/folder1/file.txt\">Baixar</a>"));
}

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

void test_render_dir_title_root() {
    TEST_ASSERT_EQUAL_STRING("Arquivos no SD", render_dir_title("").c_str());
}

void test_render_dir_title_subdir() {
    TEST_ASSERT_EQUAL_STRING("Pasta: /folder1/folder2", render_dir_title("folder1/folder2").c_str());
}

void test_render_template_substitutes_token() {
    std::string out = render_template("<p>{{X}}</p>", {{"X", "hello"}});
    TEST_ASSERT_EQUAL_STRING("<p>hello</p>", out.c_str());
}

void test_render_template_multiple_tokens() {
    std::string out = render_template("{{A}}-{{B}}", {{"A", "1"}, {"B", "2"}});
    TEST_ASSERT_EQUAL_STRING("1-2", out.c_str());
}

void test_render_template_unknown_token_left_alone() {
    std::string out = render_template("{{UNKNOWN}}", {{"X", "y"}});
    TEST_ASSERT_EQUAL_STRING("{{UNKNOWN}}", out.c_str());
}
