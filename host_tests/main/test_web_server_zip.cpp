#include "web_server_zip.hpp"

extern "C" {
#include "unity.h"
}

using namespace web_server;

namespace {

const uint8_t* bytes(const char* text) {
    return reinterpret_cast<const uint8_t*>(text);
}

uint16_t read16(const std::string& s, size_t pos) {
    return static_cast<uint16_t>(static_cast<uint8_t>(s[pos]) | (static_cast<uint8_t>(s[pos + 1]) << 8));
}

uint32_t read32(const std::string& s, size_t pos) {
    return read16(s, pos) | (static_cast<uint32_t>(read16(s, pos + 2)) << 16);
}

} // namespace

void test_crc32_of_nothing_is_zero() {
    TEST_ASSERT_EQUAL_HEX32(0, crc32_update(0, bytes(""), 0));
}

void test_crc32_check_value() {
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32_update(0, bytes("123456789"), 9));
}

void test_crc32_chunked_matches_one_shot() {
    uint32_t crc = crc32_update(0, bytes("1234"), 4);
    crc = crc32_update(crc, bytes("56789"), 5);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

void test_zip_local_header_file_layout() {
    ZipEntry entry;
    entry.name = "a.txt";
    std::string out;
    zip_append_local_header(out, entry);
    TEST_ASSERT_EQUAL(35, out.size());
    TEST_ASSERT_EQUAL_HEX32(0x04034b50, read32(out, 0));
    TEST_ASSERT_EQUAL_HEX16(0x0808, read16(out, 6)); // UTF-8 names + data descriptor
    TEST_ASSERT_EQUAL(0, read16(out, 8));            // stored
    TEST_ASSERT_EQUAL(5, read16(out, 26));
    TEST_ASSERT_EQUAL_STRING("a.txt", out.substr(30).c_str());
}

void test_zip_local_header_directory_has_no_data_descriptor_flag() {
    ZipEntry entry;
    entry.name = "pasta/";
    std::string out;
    zip_append_local_header(out, entry);
    TEST_ASSERT_EQUAL_HEX16(0x0800, read16(out, 6));
}

void test_zip_data_descriptor_layout() {
    ZipEntry entry;
    entry.name = "a.txt";
    entry.size = 9;
    entry.crc32 = 0xCBF43926;
    std::string out;
    zip_append_data_descriptor(out, entry);
    TEST_ASSERT_EQUAL(16, out.size());
    TEST_ASSERT_EQUAL_HEX32(0x08074b50, read32(out, 0));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, read32(out, 4));
    TEST_ASSERT_EQUAL(9, read32(out, 8));
    TEST_ASSERT_EQUAL(9, read32(out, 12));
}

void test_zip_central_header_layout() {
    ZipEntry entry;
    entry.name = "a.txt";
    entry.size = 9;
    entry.crc32 = 0xCBF43926;
    entry.local_header_offset = 100;
    std::string out;
    zip_append_central_header(out, entry);
    TEST_ASSERT_EQUAL(51, out.size());
    TEST_ASSERT_EQUAL_HEX32(0x02014b50, read32(out, 0));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, read32(out, 16));
    TEST_ASSERT_EQUAL(9, read32(out, 20));
    TEST_ASSERT_EQUAL(9, read32(out, 24));
    TEST_ASSERT_EQUAL(5, read16(out, 28));
    TEST_ASSERT_EQUAL_HEX32(0, read32(out, 38)); // no directory attribute
    TEST_ASSERT_EQUAL(100, read32(out, 42));
    TEST_ASSERT_EQUAL_STRING("a.txt", out.substr(46).c_str());
}

void test_zip_central_header_directory_attribute() {
    ZipEntry entry;
    entry.name = "pasta/";
    std::string out;
    zip_append_central_header(out, entry);
    TEST_ASSERT_EQUAL_HEX32(0x10, read32(out, 38));
}

void test_zip_end_of_central_dir_layout() {
    std::string out;
    zip_append_end_of_central_dir(out, 3, 150, 1000);
    TEST_ASSERT_EQUAL(22, out.size());
    TEST_ASSERT_EQUAL_HEX32(0x06054b50, read32(out, 0));
    TEST_ASSERT_EQUAL(3, read16(out, 8));
    TEST_ASSERT_EQUAL(3, read16(out, 10));
    TEST_ASSERT_EQUAL(150, read32(out, 12));
    TEST_ASSERT_EQUAL(1000, read32(out, 16));
}

void test_zip_within_limits_accepts_folder_under_4gb() {
    std::vector<ZipEntry> entries = {{"pasta/", 0}, {"pasta/a.bin", 4000000000u}};
    TEST_ASSERT_TRUE(zip_within_limits(entries));
}

void test_zip_within_limits_rejects_archive_over_4gb() {
    std::vector<ZipEntry> entries = {{"big.bin", 0xFFFFFFF0u}};
    TEST_ASSERT_FALSE(zip_within_limits(entries));
}

void test_zip_within_limits_rejects_too_many_entries() {
    std::vector<ZipEntry> entries(65536, ZipEntry{"f", 1});
    TEST_ASSERT_FALSE(zip_within_limits(entries));
}
