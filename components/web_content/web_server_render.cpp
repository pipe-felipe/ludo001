#include "web_server_render.hpp"

#include <cstdio>

#include "web_server_util.hpp"

namespace web_server {
namespace {

// Appends one <li> of the listing. `rel_path` is the entry's encoded path
// from the card's root: a folder opens with a trailing '/' and downloads
// as a .zip, a file opens and downloads as itself.
void append_entry(std::string& out, const FileEntry& entry, const std::string& rel_path) {
    const char* dir_suffix = entry.is_dir ? "/" : "";
    out += entry.is_dir ? "<li><a class=\"dir\" href=\"/files/" : "<li><a href=\"/files/";
    out += rel_path;
    out += dir_suffix;
    out += "\">";
    out += entry.name;
    out += dir_suffix;
    out += "</a><span class=\"size\">";
    out += entry.size_text;
    out += entry.is_dir ? "</span><a class=\"zip\" href=\"/zip/" : "</span><a class=\"zip\" href=\"/files/";
    out += rel_path;
    out += "\">Baixar</a><button class=\"del\" data-name=\"";
    out += rel_path;
    out += "\" onclick=\"del(this)\">Apagar</button></li>";
}

} // namespace

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

std::string render_dir_title(const std::string& current_dir) {
    if (current_dir.empty()) {
        return "Arquivos no SD";
    }
    return "Pasta: /" + current_dir;
}

std::string render_file_list(const std::vector<FileEntry>& entries, const std::string& current_dir) {
    std::string out;

    if (!current_dir.empty()) {
        std::string parent;
        size_t slash = current_dir.find_last_of('/');
        if (slash != std::string::npos) {
            parent = current_dir.substr(0, slash);
        }
        out += "<p><a class=\"up\" href=\"/files/";
        out += parent;
        out += "\">\xE2\x86\x91 pasta anterior</a></p>";
    }

    if (entries.empty()) {
        out += "<p class=\"empty\">Pasta vazia.</p>";
        return out;
    }

    out += "<ul>";
    for (const FileEntry& entry : entries) {
        std::string encoded_name = url_encode(entry.name);
        append_entry(out, entry, current_dir.empty() ? encoded_name : current_dir + "/" + encoded_name);
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
