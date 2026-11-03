#include "web_server_firmware.hpp"

#include <cstdint>

namespace web_server {
namespace {

constexpr uint8_t kImageMagic = 0xE9;          // ESP_IMAGE_HEADER_MAGIC, the image's first byte
constexpr size_t kAppDescOffset = 24 + 8;      // esp_app_desc_t follows the image and segment headers
constexpr uint32_t kAppDescMagic = 0xABCD5432; // ESP_APP_DESC_MAGIC_WORD, esp_app_desc_t's first field
constexpr size_t kProjectNameOffset = kAppDescOffset + 48; // after magic_word, secure_version, reserv1[2], version[32]
constexpr size_t kProjectNameSize = 32;        // char[32], NUL-padded

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
    if (head.size() < kFirmwareHeadSize || static_cast<uint8_t>(head[0]) != kImageMagic ||
        read_le32(head, kAppDescOffset) != kAppDescMagic) {
        return false;
    }
    std::string_view name = head.substr(kProjectNameOffset, kProjectNameSize);
    return name.substr(0, name.find('\0')) == project_name; // npos: all 32 bytes are the name
}

} // namespace web_server
