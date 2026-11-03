#include "web_server_util.hpp"

#include <cstdio>

namespace web_server {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

std::string url_decode(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        int hi = (in[i] == '%' && i + 2 < in.size()) ? hex_value(in[i + 1]) : -1;
        int lo = (hi >= 0) ? hex_value(in[i + 2]) : -1;
        if (hi >= 0 && lo >= 0) {
            out += static_cast<char>((hi << 4) | lo);
            i += 2;
        } else {
            out += in[i];
        }
    }
    return out;
}

std::string url_encode(std::string_view in) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(in.size());
    for (char ch : in) {
        unsigned char c = static_cast<unsigned char>(ch);
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out += ch;
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0xF];
        }
    }
    return out;
}

bool is_safe_filename(std::string_view name) {
    return !name.empty() && name[0] != '/' && name.find("..") == std::string_view::npos &&
           name.find('\0') == std::string_view::npos;
}

std::string numbered_name(const std::string& path, unsigned n) {
    size_t name_start = path.find_last_of('/') + 1; // no '/': npos + 1 wraps to 0, the whole path is the name
    return path.substr(0, name_start) + std::to_string(n) + "-" + path.substr(name_start);
}

std::string format_size(long size) {
    char out[24];
    if (size < 1024) {
        snprintf(out, sizeof(out), "%ld B", size);
    } else if (size < 1024 * 1024) {
        snprintf(out, sizeof(out), "%.1f KB", size / 1024.0);
    } else {
        snprintf(out, sizeof(out), "%.1f MB", size / (1024.0 * 1024.0));
    }
    return out;
}

} // namespace web_server
