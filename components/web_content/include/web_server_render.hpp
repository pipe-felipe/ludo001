#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Builds the small dynamic HTML fragments ("Wi-Fi: <pill> 1.2.3.4", the
// <ul> of files, ...) that get substituted into the static page templates
// in web_server/web/. Pure string building, no ESP-IDF/network
// dependency, so this builds and runs on the host (IDF_TARGET=linux) for
// unit testing -- see host_tests/.
namespace web_server {

std::string render_wifi_line(bool connected, const char* ip);

// capacity_mb is the card's total capacity, as shown on the status page
// (not used/free -- that's a separate figure shown on the OLED).
std::string render_sd_line(bool mounted, uint64_t capacity_mb);

struct FileEntry {
    std::string name;            // raw display name from the filesystem
    std::string size_text;       // already formatted, e.g. "36 B"; empty for directories
    bool is_dir = false;
};

// Title for the files page: "Arquivos no SD" at the root, or
// "Pasta: /a/b" while browsing a subdirectory.
std::string render_dir_title(const std::string& current_dir);

// Renders the listing for one directory: an "up" link (unless at the
// root), then the <ul> of entries (href/data-name targets are
// percent-encoded per path segment internally) or an "empty" message.
// `current_dir` is the already-decoded path being listed, with real '/'
// separators and no leading/trailing slash ("" for the root).
std::string render_file_list(const std::vector<FileEntry>& entries, const std::string& current_dir);

// Replaces each "{{token}}" in `tmpl` with its paired value. A token with
// no matching pair, or a pair with no matching token, is left/ignored.
// Plain single-pass substitution, not a general template engine.
std::string render_template(
    std::string_view tmpl, std::initializer_list<std::pair<std::string_view, std::string_view>> values);

} // namespace web_server
