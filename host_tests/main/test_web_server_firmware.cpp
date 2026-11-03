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
