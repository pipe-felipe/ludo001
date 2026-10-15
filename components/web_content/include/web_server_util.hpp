#pragma once

#include <cstddef>
#include <string>

// Small string/path helpers for web_server's HTTP handlers. No ESP-IDF or
// network dependency, so this builds and runs on the host (IDF_TARGET=linux)
// for unit testing -- see host_tests/.
namespace web_server {

// Decodes %XX escapes (as produced by encodeURIComponent on the client).
void url_decode(const char* in, char* out, size_t out_size);

// Percent-encodes everything outside the URL-safe unreserved set, so file
// names with spaces or accents still work as href targets.
void url_encode(const char* in, char* out, size_t out_size);

// Rejects anything that could escape the SD card's mount point.
bool is_safe_filename(const char* name);

// Puts "<n>-" in front of the file name at the end of `path`
// ("a/b/foto.jpg", 2 -> "a/b/2-foto.jpg"): the name an upload gets when
// `path` is already taken on the card.
std::string numbered_name(const std::string& path, unsigned n);

// Formats a byte count as "123 B", "4.5 KB" or "1.2 MB".
void format_size(long size, char* out, size_t out_size);

} // namespace web_server
