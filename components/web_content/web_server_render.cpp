#include "web_server_render.hpp"

#include <cstdio>

#include "web_server_util.hpp"

namespace web_server {

std::string render_wifi_line(bool connected, const char* ip) {
    if (!connected) {
        return "<span class=\"pill bad\">desconectado</span>";
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "<span class=\"pill ok\">conectado</span> %s", ip);
    return buf;
}

std::string render_sd_line(bool mounted, uint64_t capacity_mb) {
    if (!mounted) {
        return "<span class=\"pill bad\">nao montado</span>";
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "<span class=\"pill ok\">montado</span> %llu MB",
        static_cast<unsigned long long>(capacity_mb));
    return buf;
}

std::string render_file_list(const std::vector<FileEntry>& entries) {
    if (entries.empty()) {
        return "<p class=\"empty\">Nenhum arquivo ainda.</p>";
    }

    std::string out = "<ul>";
    char encoded_name[200];
    char item[700];
    for (const FileEntry& entry : entries) {
        url_encode(entry.name.c_str(), encoded_name, sizeof(encoded_name));
        snprintf(item, sizeof(item),
            "<li><a href=\"/files/%s\">%s</a><span class=\"size\">%s</span>"
            "<button class=\"del\" data-name=\"%s\" onclick=\"del(this)\">Apagar</button></li>",
            encoded_name, entry.name.c_str(), entry.size_text.c_str(), encoded_name);
        out += item;
    }
    out += "</ul>";
    return out;
}

std::string render_template(
    std::string_view tmpl, std::initializer_list<std::pair<std::string_view, std::string_view>> values) {
    std::string out(tmpl);
    for (const auto& [token, value] : values) {
        std::string needle = "{{" + std::string(token) + "}}";
        size_t pos = out.find(needle);
        if (pos != std::string::npos) {
            out.replace(pos, needle.size(), value);
        }
    }
    return out;
}

} // namespace web_server
