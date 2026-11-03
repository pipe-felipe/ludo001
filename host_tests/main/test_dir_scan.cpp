#include "dir_scan.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

extern "C" {
#include "unity.h"
}

namespace fs = std::filesystem;

namespace {

// An empty scratch directory; each test starts by recreating it, so one
// left behind by a failed test does no harm.
fs::path fresh_dir() {
    fs::path dir = fs::temp_directory_path() / "ludo001_dir_scan_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void make_file(const fs::path& path) {
    std::ofstream(path) << "x";
}

const dir_scan::Entry* find(const std::vector<dir_scan::Entry>& entries, const char* name) {
    auto it = std::find_if(entries.begin(), entries.end(), [&](const dir_scan::Entry& e) { return e.name == name; });
    return it == entries.end() ? nullptr : &*it;
}

} // namespace

void test_visible_entries_skips_dot_names() {
    fs::path dir = fresh_dir();
    make_file(dir / "a.txt");
    make_file(dir / ".hidden");
    fs::create_directory(dir / ".Trash-1000");
    std::vector<dir_scan::Entry> entries = dir_scan::visible_entries(dir.string());
    TEST_ASSERT_EQUAL_UINT(1, entries.size());
    TEST_ASSERT_EQUAL_STRING("a.txt", entries[0].name.c_str());
    fs::remove_all(dir);
}

void test_visible_entries_marks_directories() {
    fs::path dir = fresh_dir();
    make_file(dir / "a.txt");
    fs::create_directory(dir / "pasta");
    std::vector<dir_scan::Entry> entries = dir_scan::visible_entries(dir.string());
    TEST_ASSERT_EQUAL_UINT(2, entries.size());
    TEST_ASSERT_NOT_NULL(find(entries, "a.txt"));
    TEST_ASSERT_NOT_NULL(find(entries, "pasta"));
    TEST_ASSERT_FALSE(find(entries, "a.txt")->is_dir);
    TEST_ASSERT_TRUE(find(entries, "pasta")->is_dir);
    fs::remove_all(dir);
}

void test_visible_entries_of_missing_dir_is_empty() {
    fs::path dir = fresh_dir();
    TEST_ASSERT_EQUAL_UINT(0, dir_scan::visible_entries((dir / "missing").string()).size());
    fs::remove_all(dir);
}

void test_count_entries_of_empty_dir_is_zero() {
    fs::path dir = fresh_dir();
    TEST_ASSERT_EQUAL_UINT32(0, dir_scan::count_entries(dir.string()));
    fs::remove_all(dir);
}

void test_count_entries_counts_files_and_folders_at_every_depth() {
    fs::path dir = fresh_dir();
    make_file(dir / "a.txt");
    fs::create_directories(dir / "pasta" / "sub");
    make_file(dir / "pasta" / "b.txt");
    make_file(dir / "pasta" / "sub" / "c.txt");
    TEST_ASSERT_EQUAL_UINT32(5, dir_scan::count_entries(dir.string())); // a.txt, pasta, b.txt, sub, c.txt
    fs::remove_all(dir);
}

void test_count_entries_skips_hidden_folders_and_their_contents() {
    fs::path dir = fresh_dir();
    make_file(dir / "a.txt");
    make_file(dir / ".hidden");
    fs::create_directory(dir / ".Trash-1000");
    make_file(dir / ".Trash-1000" / "old.txt");
    TEST_ASSERT_EQUAL_UINT32(1, dir_scan::count_entries(dir.string()));
    fs::remove_all(dir);
}
