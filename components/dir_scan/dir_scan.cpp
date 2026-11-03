#include "dir_scan.hpp"

#include <dirent.h>

namespace dir_scan {

std::vector<Entry> visible_entries(const std::string& dir) {
    std::vector<Entry> entries;
    DIR* handle = opendir(dir.c_str());
    if (handle == nullptr) {
        return entries;
    }
    struct dirent* entry;
    while ((entry = readdir(handle)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        entries.push_back({entry->d_name, entry->d_type == DT_DIR});
    }
    closedir(handle);
    return entries;
}

uint32_t count_entries(const std::string& dir) {
    uint32_t count = 0;
    std::vector<std::string> pending = {dir}; // a list, not recursion: one open directory at a time
    while (!pending.empty()) {
        std::string current = std::move(pending.back());
        pending.pop_back();
        for (const Entry& entry : visible_entries(current)) {
            ++count;
            if (entry.is_dir) {
                pending.push_back(current + "/" + entry.name);
            }
        }
    }
    return count;
}

} // namespace dir_scan
