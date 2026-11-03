#pragma once

#include <string>
#include <string_view>

// Small string/path helpers for web_server's HTTP handlers. No ESP-IDF or
// network dependency, so this builds and runs on the host (IDF_TARGET=linux)
// for unit testing -- see host_tests/.
namespace web_server {

// Decodes %XX escapes (as produced by encodeURIComponent on the client).
std::string url_decode(std::string_view in);

// Percent-encodes everything outside the URL-safe unreserved set, so file
// names with spaces or accents still work as href targets.
std::string url_encode(std::string_view in);

// Rejects anything that could escape the SD card's mount point, and any
// NUL byte (a decoded "%00"), which would cut the path short.
bool is_safe_filename(std::string_view name);

// Puts "<n>-" in front of the file name at the end of `path`
// ("a/b/foto.jpg", 2 -> "a/b/2-foto.jpg"): the name an upload gets when
// `path` is already taken on the card.
std::string numbered_name(const std::string& path, unsigned n);

// Formats a byte count as "123 B", "4.5 KB" or "1.2 MB".
std::string format_size(long size);

} // namespace web_server
