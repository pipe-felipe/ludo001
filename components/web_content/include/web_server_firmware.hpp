#pragma once

#include <cstddef>
#include <string_view>

#include "esp_app_desc.h"

// Checks POST /firmware runs on an uploaded image before anything is
// written to flash. An ESP-IDF app image starts with an image header and a
// segment header, followed by the esp_app_desc_t that holds the project
// name. Only esp_app_desc.h (esp_app_format, header-only here) is used, so
// this still builds and runs on the host (IDF_TARGET=linux) for unit
// testing -- see host_tests/. The image and segment headers are defined in
// bootloader_support, which has no linux build: their sizes and magic are
// repeated below, and web_server.cpp static_asserts them against the real
// definitions.
namespace web_server {

constexpr size_t kImageHeaderSize = 24;       // sizeof(esp_image_header_t)
constexpr size_t kSegmentHeaderSize = 8;      // sizeof(esp_image_segment_header_t)
constexpr unsigned char kImageMagic = 0xE9;   // ESP_IMAGE_HEADER_MAGIC, the image's first byte

// How many bytes from the start of an image firmware_head_matches() reads.
constexpr size_t kFirmwareHeadSize = kImageHeaderSize + kSegmentHeaderSize + sizeof(esp_app_desc_t);

// True if an image of `image_size` bytes fits an update partition of
// `partition_size` bytes: not empty, and no larger than the partition.
bool firmware_size_fits(size_t image_size, size_t partition_size);

// True if `head`, the first kFirmwareHeadSize bytes of an upload, starts
// an app image built for the project called `project_name`. Says nothing
// about the rest of the image: esp_ota_end() checks that, signature
// included.
bool firmware_head_matches(std::string_view head, std::string_view project_name);

} // namespace web_server
