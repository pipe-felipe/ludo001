#include "web_server_firmware.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

extern "C" {
#include "unity.h"
}

using namespace web_server;

namespace {

constexpr size_t kPartitionSize = 1700 * 1024; // ota_0 and ota_1 in partitions_two_ota_large.csv

// The first 288 bytes of build/ludo001.bin as ESP-IDF 6.1 produced them (the
// image header, the first segment's header and the whole esp_app_desc_t,
// version "afd64a5", project "ludo001"). The tests below take their layout
// from this image instead of repeating the offsets web_server_firmware.cpp
// uses. To regenerate it after an ESP-IDF upgrade:
//   python3 -c "d=open('build/ludo001.bin','rb').read(288); print(', '.join('0x%02x' % b for b in d))"
const unsigned char kReferenceHead[] = {
    0xe9, 0x07, 0x02, 0x20, 0xd0, 0x18, 0x08, 0x40, 0xee, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x8f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x20, 0x00, 0x40, 0x3f, 0x14, 0xac, 0x02, 0x00, 0x32, 0x54, 0xcd, 0xab,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x61, 0x66, 0x64, 0x36, 0x34, 0x61, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x75, 0x64, 0x6f,
    0x30, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x31, 0x34, 0x3a, 0x30, 0x34, 0x3a, 0x32, 0x32,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4e, 0x6f, 0x76, 0x20,
    0x20, 0x33, 0x20, 0x32, 0x30, 0x32, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x76, 0x36, 0x2e, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0xb3, 0x96, 0xa0,
    0x3b, 0xaf, 0x13, 0x22, 0x22, 0xb5, 0x3f, 0x58, 0x6d, 0x04, 0x1c, 0x1b,
    0x3e, 0x5c, 0xb9, 0x66, 0xa7, 0x36, 0x6c, 0xf4, 0x8c, 0x94, 0xbe, 0x99,
    0x3f, 0x24, 0xac, 0x69, 0x00, 0x00, 0x63, 0x00, 0x10, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

std::string reference_head() {
    return std::string(reinterpret_cast<const char*>(kReferenceHead), sizeof(kReferenceHead));
}

// Where esp_app_desc_t starts in the reference image, found through its
// magic word (the ESP-IDF definition, little-endian in the image).
size_t reference_app_desc_offset() {
    const uint32_t magic = ESP_APP_DESC_MAGIC_WORD;
    const char magic_bytes[] = {static_cast<char>(magic), static_cast<char>(magic >> 8), static_cast<char>(magic >> 16),
        static_cast<char>(magic >> 24)};
    return reference_head().find(std::string(magic_bytes, sizeof(magic_bytes)));
}

// The reference head with its project name replaced by `project_name`.
std::string image_head(const std::string& project_name) {
    std::string head = reference_head();
    const size_t name_at = reference_app_desc_offset() + offsetof(esp_app_desc_t, project_name);
    head.replace(name_at, sizeof(esp_app_desc_t::project_name), sizeof(esp_app_desc_t::project_name), '\0');
    head.replace(name_at, project_name.size(), project_name);
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

void test_firmware_reference_head_has_the_layout_the_checks_assume() {
    const std::string head = reference_head();
    TEST_ASSERT_EQUAL_UINT(kFirmwareHeadSize, head.size());
    TEST_ASSERT_EQUAL_UINT8(kImageMagic, static_cast<uint8_t>(head[0]));
    TEST_ASSERT_EQUAL_UINT(kImageHeaderSize + kSegmentHeaderSize, reference_app_desc_offset());

    esp_app_desc_t desc;
    memcpy(&desc, head.data() + reference_app_desc_offset(), sizeof(desc));
    TEST_ASSERT_EQUAL_UINT32(ESP_APP_DESC_MAGIC_WORD, desc.magic_word);
    TEST_ASSERT_EQUAL_STRING("ludo001", desc.project_name);
}

void test_firmware_head_matches_real_image_head() {
    TEST_ASSERT_TRUE(firmware_head_matches(reference_head(), "ludo001"));
    TEST_ASSERT_FALSE(firmware_head_matches(reference_head(), "esp-idf-hello"));
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

void test_firmware_head_matches_name_filling_the_whole_field() {
    const std::string name(sizeof(esp_app_desc_t::project_name), 'x'); // no NUL inside the field
    TEST_ASSERT_TRUE(firmware_head_matches(image_head(name), name));
    TEST_ASSERT_FALSE(firmware_head_matches(image_head(name), "ludo001"));
}

void test_firmware_head_matches_rejects_bad_image_magic() {
    std::string head = image_head("ludo001");
    head[0] = '\0';
    TEST_ASSERT_FALSE(firmware_head_matches(head, "ludo001"));
}

void test_firmware_head_matches_rejects_bad_app_desc_magic() {
    std::string head = image_head("ludo001");
    head[reference_app_desc_offset()] = '\0';
    TEST_ASSERT_FALSE(firmware_head_matches(head, "ludo001"));
}

void test_firmware_head_matches_rejects_short_head() {
    std::string head = image_head("ludo001");
    head.pop_back();
    TEST_ASSERT_FALSE(firmware_head_matches(head, "ludo001"));
}
