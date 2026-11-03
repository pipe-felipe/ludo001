#include "web_server_firmware.hpp"

#include <cstddef>
#include <cstdint>

namespace web_server {
namespace {

// esp_app_desc_t follows the image header and the first segment's header.
constexpr size_t kAppDescOffset = kImageHeaderSize + kSegmentHeaderSize;
constexpr size_t kProjectNameOffset = kAppDescOffset + offsetof(esp_app_desc_t, project_name);
constexpr size_t kProjectNameSize = sizeof(esp_app_desc_t::project_name); // NUL-padded

uint32_t read_le32(std::string_view bytes, size_t pos) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[pos + i])) << (8 * i);
    }
    return value;
}

} // namespace

bool firmware_size_fits(size_t image_size, size_t partition_size) {
    return image_size > 0 && image_size <= partition_size;
}

bool firmware_head_matches(std::string_view head, std::string_view project_name) {
    // magic_word is esp_app_desc_t's first field, so it sits at kAppDescOffset.
    if (head.size() < kFirmwareHeadSize || static_cast<uint8_t>(head[0]) != kImageMagic ||
        read_le32(head, kAppDescOffset) != ESP_APP_DESC_MAGIC_WORD) {
        return false;
    }
    std::string_view name = head.substr(kProjectNameOffset, kProjectNameSize);
    return name.substr(0, name.find('\0')) == project_name; // npos: all bytes are the name
}

} // namespace web_server
