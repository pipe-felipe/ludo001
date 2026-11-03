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
