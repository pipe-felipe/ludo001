#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Byte layout of a ZIP archive with stored (uncompressed) entries, so
// web_server can stream a whole folder as one .zip without buffering file
// contents: each file's CRC-32 goes in a data descriptor written after its
// data (general-purpose flag bit 3), so the local header can be sent before
// the file is read. No ESP-IDF or network dependency, so this builds and
// runs on the host (IDF_TARGET=linux) for unit testing -- see host_tests/.
namespace web_server {

struct ZipEntry {
    std::string name;                 // path inside the archive, '/'-separated; a directory ends in '/'
    uint32_t size = 0;                // stored (== uncompressed) size; 0 for a directory
    uint32_t crc32 = 0;               // set once the file's data has been streamed
    uint32_t local_header_offset = 0; // archive offset of this entry's local header
};

// Running CRC-32 (the ZIP/zlib polynomial): start with crc = 0 and feed
// consecutive chunks; the value after the last chunk is the checksum of
// all of them.
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len);

// Each zip_append_* appends one little-endian record to `out`.
void zip_append_local_header(std::string& out, const ZipEntry& entry);
void zip_append_data_descriptor(std::string& out, const ZipEntry& entry); // file entries only
void zip_append_central_header(std::string& out, const ZipEntry& entry);
void zip_append_end_of_central_dir(
    std::string& out, uint16_t entry_count, uint32_t central_dir_size, uint32_t central_dir_offset);

// True if the archive for `entries` stays within plain ZIP's limits
// (65535 entries, every size and offset below 4 GiB); the handler checks
// this before sending anything, since there's no ZIP64 support.
bool zip_within_limits(const std::vector<ZipEntry>& entries);

} // namespace web_server
