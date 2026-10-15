#include "web_server_zip.hpp"

namespace web_server {
namespace {

constexpr uint32_t kLocalHeaderSignature = 0x04034b50;
constexpr uint32_t kDataDescriptorSignature = 0x08074b50;
constexpr uint32_t kCentralHeaderSignature = 0x02014b50;
constexpr uint32_t kEndOfCentralDirSignature = 0x06054b50;

constexpr uint64_t kLocalHeaderSize = 30;
constexpr uint64_t kDataDescriptorSize = 16;
constexpr uint64_t kCentralHeaderSize = 46;
constexpr uint64_t kEndOfCentralDirSize = 22;

constexpr size_t kMaxEntries = 0xFFFF;
constexpr uint64_t kMaxArchiveSize = 0xFFFFFFFF;

constexpr uint16_t kVersion = 20;                // 2.0, the minimum for directories and data descriptors
constexpr uint16_t kFlagDataDescriptor = 0x0008; // CRC-32 and sizes follow the data
constexpr uint16_t kFlagUtf8Names = 0x0800;      // names are UTF-8, as the browser sent them on upload
constexpr uint16_t kMethodStored = 0;
constexpr uint16_t kDosTime = 0;                 // 00:00:00
constexpr uint16_t kDosDate = (1 << 5) | 1;      // 1980-01-01: the board has no real-time clock
constexpr uint32_t kDosDirectoryAttribute = 0x10;

bool is_directory(const ZipEntry& entry) {
    return !entry.name.empty() && entry.name.back() == '/';
}

uint16_t flags_for(const ZipEntry& entry) {
    return is_directory(entry) ? kFlagUtf8Names : (kFlagUtf8Names | kFlagDataDescriptor);
}

void put16(std::string& out, uint16_t value) {
    out += static_cast<char>(value & 0xFF);
    out += static_cast<char>(value >> 8);
}

void put32(std::string& out, uint32_t value) {
    put16(out, static_cast<uint16_t>(value & 0xFFFF));
    put16(out, static_cast<uint16_t>(value >> 16));
}

} // namespace

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void zip_append_local_header(std::string& out, const ZipEntry& entry) {
    put32(out, kLocalHeaderSignature);
    put16(out, kVersion);
    put16(out, flags_for(entry));
    put16(out, kMethodStored);
    put16(out, kDosTime);
    put16(out, kDosDate);
    put32(out, 0); // CRC-32 and sizes: in the data descriptor (all 0 for a directory)
    put32(out, 0);
    put32(out, 0);
    put16(out, static_cast<uint16_t>(entry.name.size()));
    put16(out, 0); // extra field length
    out += entry.name;
}

void zip_append_data_descriptor(std::string& out, const ZipEntry& entry) {
    put32(out, kDataDescriptorSignature);
    put32(out, entry.crc32);
    put32(out, entry.size); // compressed size == uncompressed size (stored)
    put32(out, entry.size);
}

void zip_append_central_header(std::string& out, const ZipEntry& entry) {
    put32(out, kCentralHeaderSignature);
    put16(out, kVersion); // version made by (MS-DOS)
    put16(out, kVersion); // version needed to extract
    put16(out, flags_for(entry));
    put16(out, kMethodStored);
    put16(out, kDosTime);
    put16(out, kDosDate);
    put32(out, entry.crc32);
    put32(out, entry.size);
    put32(out, entry.size);
    put16(out, static_cast<uint16_t>(entry.name.size()));
    put16(out, 0); // extra field length
    put16(out, 0); // comment length
    put16(out, 0); // disk number start
    put16(out, 0); // internal attributes
    put32(out, is_directory(entry) ? kDosDirectoryAttribute : 0);
    put32(out, entry.local_header_offset);
    out += entry.name;
}

void zip_append_end_of_central_dir(
    std::string& out, uint16_t entry_count, uint32_t central_dir_size, uint32_t central_dir_offset) {
    put32(out, kEndOfCentralDirSignature);
    put16(out, 0); // number of this disk
    put16(out, 0); // disk where the central directory starts
    put16(out, entry_count); // entries on this disk
    put16(out, entry_count); // entries in total
    put32(out, central_dir_size);
    put32(out, central_dir_offset);
    put16(out, 0); // comment length
}

bool zip_within_limits(const std::vector<ZipEntry>& entries) {
    if (entries.size() > kMaxEntries) {
        return false;
    }
    uint64_t total = kEndOfCentralDirSize;
    for (const ZipEntry& entry : entries) {
        total += kLocalHeaderSize + kCentralHeaderSize + 2 * entry.name.size() + entry.size;
        if (!is_directory(entry)) {
            total += kDataDescriptorSize;
        }
    }
    return total <= kMaxArchiveSize;
}

} // namespace web_server
