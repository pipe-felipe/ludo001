#include "web_server.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "dir_scan.hpp"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "mdns.h"
#include "web_server_firmware.hpp"
#include "web_server_render.hpp"
#include "web_server_util.hpp"
#include "web_server_zip.hpp"

#include "sd_card.hpp"
#include "wifi_manager.hpp"

namespace web_server {
namespace {

constexpr char kTag[] = "web_server";
constexpr char kHostname[] = "ludo001"; // reachable at http://ludo001.local/

httpd_handle_t s_server = nullptr;
bool s_mdns_started = false;

// Written by the httpd task inside recv_body, read by the display loop (a
// different task); a spinlock keeps the read/write atomic without pulling
// in a full mutex for three small fields.
portMUX_TYPE s_progress_lock = portMUX_INITIALIZER_UNLOCKED;
UploadProgress s_progress;

void set_progress(const UploadProgress& progress) {
    portENTER_CRITICAL(&s_progress_lock);
    s_progress = progress;
    portEXIT_CRITICAL(&s_progress_lock);
}

// Set once firmware_handler has written a new firmware and scheduled the
// restart into it. From then on POST /firmware and POST /upload get a 409
// instead of starting work the restart would cut short.
std::atomic<bool> s_restart_pending{false};

// Time for firmware_handler's "OK" to reach the client before the restart.
constexpr uint64_t kRestartDelayUs = 1000 * 1000;

// Bumped by the httpd task after every upload or delete, read by the
// display loop to know when the card's item count and free space are stale.
std::atomic<uint32_t> s_content_version{0};

// Static assets embedded from web/ (see this component's CMakeLists.txt,
// EMBED_TXTFILES). Each is null-terminated by the embed step; the extra
// byte is dropped when building the std::string/send length below.
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t files_html_start[] asm("_binary_files_html_start");
extern const uint8_t files_html_end[] asm("_binary_files_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

std::string_view embedded_text(const uint8_t* start, const uint8_t* end) {
    return std::string_view(reinterpret_cast<const char*>(start), static_cast<size_t>(end - start - 1));
}

esp_err_t send_text(httpd_req_t* req, const char* content_type, std::string_view body) {
    httpd_resp_set_type(req, content_type);
    return httpd_resp_send(req, body.data(), body.size());
}

esp_err_t style_css_handler(httpd_req_t* req) {
    return send_text(req, "text/css", embedded_text(style_css_start, style_css_end));
}

esp_err_t app_js_handler(httpd_req_t* req) {
    return send_text(req, "application/javascript", embedded_text(app_js_start, app_js_end));
}

esp_err_t root_handler(httpd_req_t* req) {
    bool wifi_connected = wifi_manager::is_connected();
    char ip[16] = "";
    if (wifi_connected) {
        wifi_manager::get_ip(ip, sizeof(ip));
    }

    bool sd_mounted = sd_card::is_mounted();

    std::string body = render_template(embedded_text(index_html_start, index_html_end),
        {
            {"WIFI_LINE", render_wifi_line(wifi_connected, ip)},
            {"SD_LINE", render_sd_line(sd_mounted, sd_card::capacity_mb())},
        });

    return send_text(req, "text/html", body);
}

// The decoded <path> of a request to `route` + <path>: "a/b.txt" for
// "/files/a/b.txt" with route "/files/".
std::string request_path(httpd_req_t* req, const char* route) {
    return url_decode(req->uri + strlen(route));
}

// SD card path of `relative_path`, which has no leading slash ("" is the
// card's root).
std::string sd_path(const std::string& relative_path) {
    std::string path = sd_card::mount_point();
    if (!relative_path.empty()) {
        path += "/" + relative_path;
    }
    return path;
}

// The listing's entries for the SD card folder `full_dir`, with file sizes
// already formatted. A file stat() can't read is skipped rather than
// failing the whole listing.
std::vector<FileEntry> list_entries(const std::string& full_dir) {
    std::vector<FileEntry> entries;
    for (const dir_scan::Entry& child : dir_scan::visible_entries(full_dir)) {
        FileEntry fe;
        fe.name = child.name;
        fe.is_dir = child.is_dir;
        if (!child.is_dir) {
            struct stat st {};
            if (stat((full_dir + "/" + child.name).c_str(), &st) != 0) {
                continue;
            }
            fe.size_text = format_size(static_cast<long>(st.st_size));
        }
        entries.push_back(std::move(fe));
    }
    return entries;
}

// Shared by the root listing ("/files") and directory browsing under
// "/files/*" (see files_item_handler) once a target turns out to be a
// directory rather than a file. `relative_dir` has no leading/trailing
// slash ("" for the root).
esp_err_t render_directory(httpd_req_t* req, const std::string& relative_dir) {
    std::string list_html = sd_card::is_mounted()
        ? render_file_list(list_entries(sd_path(relative_dir)), relative_dir)
        : "<p class=\"empty\">SD card nao montado.</p>";

    std::string body = render_template(embedded_text(files_html_start, files_html_end),
        {{"DIR_TITLE", render_dir_title(relative_dir)}, {"FILE_LIST", list_html}});

    return send_text(req, "text/html", body);
}

// GET /files -- the root directory listing, plus a plain upload form (the
// upload itself is a raw-body fetch(), not multipart/form-data, to avoid
// writing a multipart parser for what's meant to be a minimal tool).
esp_err_t files_page_handler(httpd_req_t* req) {
    return render_directory(req, "");
}

// Sends what's left of `file` as response chunks of up to 1 KB, handing
// each chunk to `on_chunk` first. False if the client went away.
template <typename OnChunk>
bool send_file_chunks(httpd_req_t* req, FILE* file, OnChunk on_chunk) {
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        on_chunk(reinterpret_cast<const uint8_t*>(buffer), read_bytes);
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            return false;
        }
    }
    return true;
}

// Streams the file at `path` back with a Content-Disposition that makes the
// browser save it as `name` instead of trying to render it.
esp_err_t send_download(httpd_req_t* req, const std::string& path, const std::string& name) {
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/octet-stream");
    // httpd keeps only a pointer to the header value, so it must live until
    // the last chunk is out.
    std::string content_disposition = "attachment; filename=\"" + name + "\"";
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());

    bool sent = send_file_chunks(req, file, [](const uint8_t*, size_t) {});
    fclose(file);
    httpd_resp_send_chunk(req, nullptr, 0);
    return sent ? ESP_OK : ESP_FAIL;
}

// GET /files/<path> -- either downloads a file (send_download) or, if
// <path> names a directory, browses into it via render_directory().
// <path> may be several segments deep ("folder/sub/file.txt"), matching
// how a folder upload lays files out (see upload_handler).
esp_err_t files_item_handler(httpd_req_t* req) {
    std::string name = request_path(req, "/files/");
    while (!name.empty() && name.back() == '/') {
        name.pop_back(); // a directory link's trailing slash
    }

    if (!sd_card::is_mounted()) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    if (name.empty()) {
        return render_directory(req, ""); // "/files/" with nothing after it
    }
    if (!is_safe_filename(name)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    std::string path = sd_path(name);
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    if (S_ISDIR(st.st_mode)) {
        return render_directory(req, name);
    }
    return send_download(req, path, name);
}

// Creates every directory in `relative_path`'s prefix on the card (e.g.
// "a/b/c.txt" -> "a", then "a/b"), so a folder upload's nested files land
// in the right place. Failures are ignored here: if a directory genuinely
// can't be created, the fopen() that follows will fail and report it.
void ensure_parent_dirs(const std::string& relative_path) {
    for (size_t slash = relative_path.find('/'); slash != std::string::npos;
         slash = relative_path.find('/', slash + 1)) {
        mkdir(sd_path(relative_path.substr(0, slash)).c_str(), 0755);
    }
}

// `name` if nothing on the card has that name yet, otherwise the first
// free "1-<name>", "2-<name>", ...: an upload never overwrites a file or a
// folder.
std::string unused_name(const std::string& name) {
    std::string candidate = name;
    struct stat existing {};
    for (unsigned n = 1; stat(sd_path(candidate).c_str(), &existing) == 0; ++n) {
        candidate = numbered_name(name, n);
    }
    return candidate;
}

// Reads the rest of the request body, from byte `done` on, in chunks of up
// to 1 KB, handing each one to `on_chunk`, which returns nullptr or the
// reason to stop. Keeps the OLED's progress bar up to date meanwhile;
// `firmware` only labels that progress. nullptr once the whole body is in,
// otherwise the reason it stopped.
template <typename OnChunk>
const char* recv_body(httpd_req_t* req, int done, bool firmware, OnChunk on_chunk) {
    const int total = req->content_len;
    const char* error = nullptr;
    set_progress({true, 0, firmware});

    char buffer[1024];
    while (done < total && error == nullptr) {
        int remaining = total - done;
        int received = httpd_req_recv(req, buffer, remaining < static_cast<int>(sizeof(buffer)) ? remaining : sizeof(buffer));
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            error = "upload interrupted";
        } else {
            error = on_chunk(buffer, received);
            done += received;
            // 64-bit product: done * 100 overflows an int past 21 MB.
            set_progress({true, static_cast<uint8_t>(static_cast<uint64_t>(done) * 100 / total), firmware});
        }
    }
    set_progress({});
    return error;
}

// Writes the request body to a new file at `path`, keeping the upload
// progress up to date for the OLED. A partly written file is removed.
// nullptr on success, otherwise the reason for the 500 response.
const char* store_body(httpd_req_t* req, const std::string& path) {
    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return "could not open file for writing";
    }

    const char* error = recv_body(req, 0, false, [file](const char* data, int len) -> const char* {
        return fwrite(data, 1, len, file) == static_cast<size_t>(len) ? nullptr : "could not write file";
    });
    fclose(file);
    if (error != nullptr) {
        remove(path.c_str());
    }
    return error;
}

// The answer to a request that would start writing while a firmware
// update has the board about to restart.
esp_err_t send_restart_pending(httpd_req_t* req) {
    httpd_resp_set_status(req, "409 Conflict");
    return httpd_resp_sendstr(req, "firmware update in progress, the board is restarting");
}

// POST /upload/<path> -- writes the raw request body to the SD card. The
// page's JS (web/app.js) sends the File object directly as the body, so
// there's no multipart boundary to parse. <path> may be several segments
// deep ("folder/sub/file.txt") for a folder upload; ensure_parent_dirs()
// creates whatever directories that needs first. A name that already
// exists on the card gets a number prefix instead of being overwritten.
esp_err_t upload_handler(httpd_req_t* req) {
    if (s_restart_pending) {
        return send_restart_pending(req);
    }

    std::string name = request_path(req, "/upload/");
    if (!sd_card::is_mounted() || !is_safe_filename(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
        return ESP_FAIL;
    }

    ensure_parent_dirs(name);
    std::string final_name = unused_name(name);
    const char* error = store_body(req, sd_path(final_name));
    s_content_version++; // even a failed upload may have created folders
    if (error != nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, error);
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "uploaded %s", final_name.c_str());
    return httpd_resp_sendstr(req, "OK");
}

// Deletes a file, or a directory and everything under it.
bool remove_recursive(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    if (!S_ISDIR(st.st_mode)) {
        return remove(path.c_str()) == 0;
    }

    bool ok = true;
    for (const dir_scan::Entry& child : dir_scan::visible_entries(path)) {
        if (!remove_recursive(path + "/" + child.name)) {
            ok = false;
        }
    }
    return rmdir(path.c_str()) == 0 && ok;
}

// POST /delete/<path> -- removes a file, or a whole folder (recursively),
// from the SD card.
esp_err_t delete_handler(httpd_req_t* req) {
    std::string name = request_path(req, "/delete/");
    if (!sd_card::is_mounted() || !is_safe_filename(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
        return ESP_FAIL;
    }

    bool removed = remove_recursive(sd_path(name));
    s_content_version++; // a delete that failed half way may still have removed items
    if (!removed) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "deleted %s", name.c_str());
    return httpd_resp_sendstr(req, "OK");
}

// Appends an entry for everything under `full_dir` (an SD card path) to
// `entries`, recursively, each named `prefix` + its path below `full_dir`.
// Folders get their own "<name>/" entry so empty ones survive the round
// trip. Dotfiles and unreadable files are skipped, matching what
// render_directory() lists.
void collect_zip_entries(const std::string& full_dir, const std::string& prefix, std::vector<ZipEntry>& entries) {
    for (const dir_scan::Entry& child : dir_scan::visible_entries(full_dir)) {
        std::string child_path = full_dir + "/" + child.name;
        ZipEntry zip_entry;
        zip_entry.name = prefix + child.name;
        if (child.is_dir) {
            zip_entry.name += '/';
            entries.push_back(zip_entry);
            collect_zip_entries(child_path, zip_entry.name, entries);
            continue;
        }
        struct stat st {};
        if (stat(child_path.c_str(), &st) != 0) {
            continue;
        }
        zip_entry.size = static_cast<uint32_t>(st.st_size);
        entries.push_back(std::move(zip_entry));
    }
}

// The .zip being streamed as one response, with the archive's running
// byte count that every record's offset is taken from.
struct ZipStream {
    httpd_req_t* req;
    uint32_t offset = 0;

    // Sends `record` as one chunk and moves `offset` past it.
    bool send(const std::string& record) {
        offset += static_cast<uint32_t>(record.size());
        return httpd_resp_send_chunk(req, record.data(), record.size()) == ESP_OK;
    }
};

// Sends one file's bytes as the data of `entry`, computing its CRC-32 on
// the way. False if the file can't be read in full or the client went
// away, so the caller can abort instead of finishing a corrupt archive.
bool send_zip_file_data(ZipStream& zip, const std::string& path, ZipEntry& entry) {
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    uint32_t crc = 0;
    uint32_t sent = 0;
    bool ok = send_file_chunks(zip.req, file, [&](const uint8_t* data, size_t len) {
        crc = crc32_update(crc, data, len);
        sent += static_cast<uint32_t>(len);
    });
    fclose(file);
    entry.crc32 = crc;
    zip.offset += sent;
    return ok && sent == entry.size;
}

// Sends every entry's local header and, for a file, its data and data
// descriptor, filling in the offsets and CRC-32s the central directory
// needs. False as soon as a file can't be read in full or the client goes
// away. `parent_dir` + an entry's name is its SD card path.
bool send_zip_entries(ZipStream& zip, const std::string& parent_dir, std::vector<ZipEntry>& entries) {
    std::string record;
    for (ZipEntry& entry : entries) {
        entry.local_header_offset = zip.offset;
        record.clear();
        zip_append_local_header(record, entry);
        if (!zip.send(record)) {
            return false;
        }
        if (entry.name.back() == '/') {
            continue;
        }
        if (!send_zip_file_data(zip, parent_dir + entry.name, entry)) {
            ESP_LOGE(kTag, "zip aborted at %s", entry.name.c_str());
            return false;
        }
        record.clear();
        zip_append_data_descriptor(record, entry);
        if (!zip.send(record)) {
            return false;
        }
    }
    return true;
}

// Sends the central directory and the record that ends the archive.
bool send_zip_central_dir(ZipStream& zip, const std::vector<ZipEntry>& entries) {
    const uint32_t central_dir_offset = zip.offset;
    std::string record;
    for (const ZipEntry& entry : entries) {
        record.clear();
        zip_append_central_header(record, entry);
        if (!zip.send(record)) {
            return false;
        }
    }
    record.clear();
    zip_append_end_of_central_dir(
        record, static_cast<uint16_t>(entries.size()), zip.offset - central_dir_offset, central_dir_offset);
    return zip.send(record);
}

// GET /zip/<path> -- downloads the folder at <path>, and everything under
// it, as one .zip built while it streams (records from web_server_zip.hpp),
// so no file is ever held in memory. Entry names start with the folder's
// own name, so the .zip unpacks into a folder of the same name. Once the
// first chunk is out the status code can't change anymore: on a failure
// the handler returns ESP_FAIL without the final empty chunk, httpd drops
// the connection, and the browser marks the download as failed instead
// of saving a truncated archive.
esp_err_t zip_handler(httpd_req_t* req) {
    std::string name = request_path(req, "/zip/");
    // Strip every trailing slash: "/zip/pasta//" must not leave an empty
    // folder name, which would put "/..." names in the archive.
    while (!name.empty() && name.back() == '/') {
        name.pop_back();
    }

    if (!sd_card::is_mounted() || !is_safe_filename(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid folder name or no SD card");
        return ESP_FAIL;
    }

    std::string full_dir = sd_path(name);
    struct stat st {};
    if (stat(full_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Entry names are relative to the folder's parent ("fotos/a.jpg" for
    // /zip/viagem/fotos), so parent_dir + an entry's name is its SD path.
    std::string folder_name = name.substr(name.find_last_of('/') + 1); // no '/': npos + 1 wraps to 0
    std::string parent_dir = full_dir.substr(0, full_dir.size() - folder_name.size());

    std::vector<ZipEntry> entries;
    ZipEntry root;
    root.name = folder_name + "/";
    entries.push_back(root);
    collect_zip_entries(full_dir, root.name, entries);

    if (!zip_within_limits(entries)) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "folder too large for .zip (max 4 GB, 65535 items)");
        return ESP_FAIL;
    }

    // folder_name comes from a path stat() just found on the FAT card, and
    // FAT names can't contain '"' or control characters, so it can't break
    // out of the quoted header value. httpd keeps only a pointer to the
    // value, so it must live until the last chunk is out.
    httpd_resp_set_type(req, "application/zip");
    std::string content_disposition = "attachment; filename=\"" + folder_name + ".zip\"";
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());

    ZipStream zip{req};
    if (!send_zip_entries(zip, parent_dir, entries) || !send_zip_central_dir(zip, entries)) {
        return ESP_FAIL;
    }
    httpd_resp_send_chunk(req, nullptr, 0);

    ESP_LOGI(kTag, "zipped %s (%u entries)", name.c_str(), static_cast<unsigned>(entries.size()));
    return ESP_OK;
}

// Fills `buffer` with the next `len` bytes of the request body. False if
// the client went away first.
bool recv_exact(httpd_req_t* req, char* buffer, int len) {
    for (int got = 0; got < len;) {
        int received = httpd_req_recv(req, buffer + got, len - got);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            return false;
        }
        got += received;
    }
    return true;
}

// Writes the image whose first `head_len` bytes are `head` (the rest still
// unread in the request body) to `partition`, then has esp_ota_end() check
// the whole image, signature included. On a failure it answers the request
// itself and returns false; the partition the board boots from is never
// touched.
bool write_firmware(httpd_req_t* req, const esp_partition_t* partition, const char* head, int head_len) {
    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not start firmware update");
        return false;
    }

    auto write_chunk = [handle](const char* data, int len) -> const char* {
        return esp_ota_write(handle, data, len) == ESP_OK ? nullptr : "could not write firmware";
    };
    const char* error = write_chunk(head, head_len);
    if (error == nullptr) {
        error = recv_body(req, head_len, true, write_chunk);
    }
    if (error != nullptr) {
        esp_ota_abort(handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, error);
        return false;
    }

    err = esp_ota_end(handle);
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "firmware image truncated, corrupt or not signed with this board's key");
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not finish firmware update");
        return false;
    }
    return true;
}

// Restarts the board kRestartDelayUs from now, from the esp_timer task, so
// firmware_handler can still answer first. Restarts right away if the
// timer can't be set up.
void schedule_restart() {
    const esp_timer_create_args_t args = {
        .callback = [](void*) { esp_restart(); },
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fw_restart",
        .skip_unhandled_events = false,
    };
    esp_timer_handle_t timer = nullptr;
    if (esp_timer_create(&args, &timer) != ESP_OK || esp_timer_start_once(timer, kRestartDelayUs) != ESP_OK) {
        esp_restart();
    }
}

// POST /firmware -- writes the raw request body, a signed build/ludo001.bin,
// to the OTA partition the board is not running from, makes that the boot
// partition and restarts into it. Nothing reaches flash unless the size
// fits the partition and the image was built for this project; then
// esp_ota_end() checks the whole image and its signature
// (CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT) before the boot partition
// changes. The rejections before the body is read return ESP_OK, so httpd
// reads and discards the rest of the body: the client gets the error
// status instead of a connection reset.
esp_err_t firmware_handler(httpd_req_t* req) {
    if (s_restart_pending) {
        return send_restart_pending(req);
    }

    const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
    if (partition == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no OTA partition to update");
        return ESP_OK;
    }
    if (!firmware_size_fits(req->content_len, partition->size)) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "firmware image empty or larger than the update partition");
        return ESP_OK;
    }

    char head[kFirmwareHeadSize];
    const int head_len = static_cast<int>(req->content_len < kFirmwareHeadSize ? req->content_len : kFirmwareHeadSize);
    if (!recv_exact(req, head, head_len)) {
        return ESP_FAIL; // the client went away
    }
    if (!firmware_head_matches(std::string_view(head, head_len), esp_app_get_description()->project_name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "not a ludo001 firmware image");
        return ESP_OK;
    }

    if (!write_firmware(req, partition, head, head_len)) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not switch to the new firmware");
        return ESP_FAIL;
    }

    s_restart_pending = true;
    ESP_LOGI(kTag, "firmware written to %s (%u bytes), restarting", partition->label, static_cast<unsigned>(req->content_len));
    httpd_resp_sendstr(req, "OK");
    schedule_restart();
    return ESP_OK;
}

constexpr httpd_uri_t kRootUri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kStyleCssUri = {
    .uri = "/style.css",
    .method = HTTP_GET,
    .handler = style_css_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kAppJsUri = {
    .uri = "/app.js",
    .method = HTTP_GET,
    .handler = app_js_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kFilesPageUri = {
    .uri = "/files",
    .method = HTTP_GET,
    .handler = files_page_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kFilesItemUri = {
    .uri = "/files/*",
    .method = HTTP_GET,
    .handler = files_item_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kUploadUri = {
    .uri = "/upload/*",
    .method = HTTP_POST,
    .handler = upload_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kDeleteUri = {
    .uri = "/delete/*",
    .method = HTTP_POST,
    .handler = delete_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kZipUri = {
    .uri = "/zip/*",
    .method = HTTP_GET,
    .handler = zip_handler,
    .user_ctx = nullptr,
};

constexpr httpd_uri_t kFirmwareUri = {
    .uri = "/firmware",
    .method = HTTP_POST,
    .handler = firmware_handler,
    .user_ctx = nullptr,
};

} // namespace

esp_err_t start() {
    if (s_server != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // needed for "/files/*" and "/upload/*"
    // Default 4096 isn't enough for render_directory()'s call chain --
    // confirmed via a captured StoreProhibited panic while browsing a
    // subdirectory.
    config.stack_size = 8192;
    config.max_uri_handlers = 12; // the 9 routes registered below, plus room for 3 more
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = nullptr;
        return err;
    }

    const httpd_uri_t* uris[] = {
        &kRootUri, &kStyleCssUri, &kAppJsUri, &kFilesPageUri, &kFilesItemUri, &kUploadUri, &kDeleteUri, &kZipUri,
        &kFirmwareUri};
    for (const httpd_uri_t* uri : uris) {
        err = httpd_register_uri_handler(s_server, uri);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "httpd_register_uri_handler(%s) failed: %s", uri->uri, esp_err_to_name(err));
            httpd_stop(s_server);
            s_server = nullptr;
            return err;
        }
    }

    // Advertise http://ludo001.local/ so the status page stays reachable
    // regardless of what IP the DHCP server hands out.
    err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "mdns_init failed: %s", esp_err_to_name(err));
    } else {
        s_mdns_started = true;
        mdns_hostname_set(kHostname);
        mdns_instance_name_set("ludo001 status");
        mdns_service_add("ludo001-http", "_http", "_tcp", 80, nullptr, 0);
    }

    return ESP_OK;
}

esp_err_t stop() {
    if (s_server == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mdns_started) {
        mdns_free();
        s_mdns_started = false;
    }
    esp_err_t err = httpd_stop(s_server);
    s_server = nullptr;
    return err;
}

bool is_running() {
    return s_server != nullptr;
}

UploadProgress upload_progress() {
    portENTER_CRITICAL(&s_progress_lock);
    UploadProgress out = s_progress;
    portEXIT_CRITICAL(&s_progress_lock);
    return out;
}

uint32_t content_version() {
    return s_content_version.load();
}

} // namespace web_server
