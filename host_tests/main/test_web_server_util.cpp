#include "web_server_util.hpp"

#include <cstring>

extern "C" {
#include "unity.h"
}

using namespace web_server;

void test_url_encode_leaves_unreserved_untouched() {
    char out[64];
    url_encode("abcXYZ019-_.~", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("abcXYZ019-_.~", out);
}

void test_url_encode_percent_encodes_space() {
    char out[64];
    url_encode("a b", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("a%20b", out);
}

void test_url_encode_percent_encodes_multibyte_utf8() {
    char out[64];
    url_encode("caf\xc3\xa9", out, sizeof(out)); // "café", e-acute as UTF-8
    TEST_ASSERT_EQUAL_STRING("caf%C3%A9", out);
}

void test_url_encode_truncates_safely_when_buffer_small() {
    char out[4];
    url_encode("abcdef", out, sizeof(out));
    TEST_ASSERT_LESS_OR_EQUAL(3, strlen(out));
    TEST_ASSERT_EQUAL_CHAR('\0', out[strlen(out)]);
}

void test_url_decode_plain_roundtrip() {
    char out[64];
    url_decode("abc", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("abc", out);
}

void test_url_decode_percent_sequences() {
    char out[64];
    url_decode("a%20b%2Fc", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("a b/c", out);
}

void test_url_decode_invalid_percent_sequence_passed_through() {
    char out[64];
    url_decode("100%", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("100%", out);
}

void test_url_encode_decode_roundtrip() {
    const char* original = "arquivo com espa\xc3\xa7o.txt"; // has a space and a UTF-8 char
    char encoded[128];
    char decoded[128];
    url_encode(original, encoded, sizeof(encoded));
    url_decode(encoded, decoded, sizeof(decoded));
    TEST_ASSERT_EQUAL_STRING(original, decoded);
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

void test_format_size_bytes() {
    char out[32];
    format_size(0, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("0 B", out);
    format_size(1023, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("1023 B", out);
}

void test_format_size_kilobytes() {
    char out[32];
    format_size(1024, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("1.0 KB", out);
    format_size(1536, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("1.5 KB", out);
}

void test_format_size_megabytes() {
    char out[32];
    format_size(1024L * 1024L, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("1.0 MB", out);
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
