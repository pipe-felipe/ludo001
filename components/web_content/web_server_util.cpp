#include "web_server_util.hpp"

#include <cstdio>
#include <cstring>

namespace web_server {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

void url_decode(const char* in, char* out, size_t out_size) {
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < out_size; ++i) {
        int hi = (in[i] == '%') ? hex_value(in[i + 1]) : -1;
        int lo = (hi >= 0) ? hex_value(in[i + 2]) : -1;
        if (hi >= 0 && lo >= 0) {
            out[o++] = static_cast<char>((hi << 4) | lo);
            i += 2;
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

void url_encode(const char* in, char* out, size_t out_size) {
    constexpr char kHex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 4 < out_size; ++i) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out[o++] = static_cast<char>(c);
        } else {
            out[o++] = '%';
            out[o++] = kHex[c >> 4];
            out[o++] = kHex[c & 0xF];
        }
    }
    out[o] = '\0';
}

bool is_safe_filename(const char* name) {
    return name[0] != '\0' && name[0] != '/' && std::strstr(name, "..") == nullptr;
}

void format_size(long size, char* out, size_t out_size) {
    if (size < 1024) {
        snprintf(out, out_size, "%ld B", size);
    } else if (size < 1024 * 1024) {
        snprintf(out, out_size, "%.1f KB", size / 1024.0);
    } else {
        snprintf(out, out_size, "%.1f MB", size / (1024.0 * 1024.0));
    }
}

} // namespace web_server
