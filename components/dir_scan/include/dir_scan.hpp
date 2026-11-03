#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Lists and counts what's inside a directory with plain POSIX
// opendir/readdir, skipping every name that starts with '.' (dotfiles, and
// dot-folders such as ".Trash-1000" together with everything inside them),
// the same names the web file listing hides. No ESP-IDF dependency, so this
// builds and runs on the host (IDF_TARGET=linux) for unit testing -- see
// host_tests/.
namespace dir_scan {

struct Entry {
    std::string name; // name inside the directory, no path
    bool is_dir = false;
};

// The visible entries of `dir`, in readdir order; empty if `dir` can't be
// opened. Only one directory is open at a time, and only during the call.
std::vector<Entry> visible_entries(const std::string& dir);

// Number of visible files and folders under `dir`, at any depth, not
// counting `dir` itself.
uint32_t count_entries(const std::string& dir);

} // namespace dir_scan
