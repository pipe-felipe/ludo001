#pragma once

#include <cstddef>
#include <string_view>

// Checks POST /firmware runs on an uploaded image before anything is
// written to flash. An ESP-IDF app image starts with a 24-byte image
// header and an 8-byte segment header, followed by the 256-byte
// esp_app_desc_t that holds the project name. No ESP-IDF or network
// dependency, so this builds and runs on the host (IDF_TARGET=linux) for
// unit testing -- see host_tests/.
namespace web_server {

// How many bytes from the start of an image firmware_head_matches() reads.
constexpr size_t kFirmwareHeadSize = 24 + 8 + 256;

// True if an image of `image_size` bytes fits an update partition of
// `partition_size` bytes: not empty, and no larger than the partition.
bool firmware_size_fits(size_t image_size, size_t partition_size);

// True if `head`, the first kFirmwareHeadSize bytes of an upload, starts
// an app image built for the project called `project_name`. Says nothing
// about the rest of the image: esp_ota_end() checks that, signature
// included.
bool firmware_head_matches(std::string_view head, std::string_view project_name);

} // namespace web_server
