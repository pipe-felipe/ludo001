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
    std::string html = render_file_list({});
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "Nenhum arquivo"));
}

void test_render_file_list_single_entry() {
    std::vector<FileEntry> entries = {{"log.txt", "36 B"}};
    std::string html = render_file_list(entries);
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/log.txt\""));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), ">log.txt<"));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "36 B"));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "data-name=\"log.txt\""));
}

void test_render_file_list_encodes_name_with_space_in_href() {
    std::vector<FileEntry> entries = {{"my file.txt", "1.0 KB"}};
    std::string html = render_file_list(entries);
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "href=\"/files/my%20file.txt\""));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), ">my file.txt<")); // display text stays unencoded
}

void test_render_file_list_multiple_entries_all_present() {
    std::vector<FileEntry> entries = {{"a.txt", "1 B"}, {"b.txt", "2 B"}};
    std::string html = render_file_list(entries);
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "a.txt"));
    TEST_ASSERT_NOT_NULL(strstr(html.c_str(), "b.txt"));
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
