#include "web_server.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "dir_scan.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "mdns.h"
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

// Written by the httpd task inside upload_handler, read by the display
// loop (a different task); a spinlock keeps the read/write atomic without
// pulling in a full mutex for two small fields.
portMUX_TYPE s_progress_lock = portMUX_INITIALIZER_UNLOCKED;
UploadProgress s_progress;

void set_progress(bool active, uint8_t percent) {
    portENTER_CRITICAL(&s_progress_lock);
    s_progress.active = active;
    s_progress.percent = percent;
    portEXIT_CRITICAL(&s_progress_lock);
}

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
    char decoded[256];
    url_decode(req->uri + strlen(route), decoded, sizeof(decoded));
    return decoded;
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
            char size_text[24];
            format_size(static_cast<long>(st.st_size), size_text, sizeof(size_text));
            fe.size_text = size_text;
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
    if (!is_safe_filename(name.c_str())) {
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

// Creates every directory in `relative_path`'s prefix under mount_point
// (e.g. "a/b/c.txt" -> "a", then "a/b"), so a folder upload's nested files
// land in the right place. Failures are ignored here: if a directory
// genuinely can't be created, the fopen() that follows will fail and
// report it.
void ensure_parent_dirs(const char* mount_point, const char* relative_path) {
    std::string path = mount_point;
    const char* p = relative_path;
    const char* slash;
    while ((slash = strchr(p, '/')) != nullptr) {
        path += '/';
        path.append(p, static_cast<size_t>(slash - p));
        mkdir(path.c_str(), 0755);
        p = slash + 1;
    }
}

// POST /upload/<path> -- writes the raw request body to the SD card. The
// page's JS (web/app.js) sends the File object directly as the body, so
// there's no multipart boundary to parse. <path> may be several segments
// deep ("folder/sub/file.txt") for a folder upload; ensure_parent_dirs()
// creates whatever directories that needs first. A name that already
// exists on the card gets a number prefix instead of being overwritten.
esp_err_t upload_handler(httpd_req_t* req) {
    char decoded_name[256];
    url_decode(req->uri + strlen("/upload/"), decoded_name, sizeof(decoded_name));

    if (!sd_card::is_mounted() || !is_safe_filename(decoded_name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
        return ESP_FAIL;
    }

    ensure_parent_dirs(sd_card::mount_point(), decoded_name);

    // Never overwrite: if the name is taken (by a file or a folder), the
    // upload is stored as "1-<name>", then "2-<name>", and so on.
    std::string final_name = decoded_name;
    char path[400];
    struct stat existing {};
    for (unsigned n = 1;; ++n) {
        snprintf(path, sizeof(path), "%s/%s", sd_card::mount_point(), final_name.c_str());
        if (stat(path, &existing) != 0) {
            break;
        }
        final_name = numbered_name(decoded_name, n);
    }

    FILE* file = fopen(path, "wb");
    if (file == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not open file for writing");
        return ESP_FAIL;
    }

    const int total = req->content_len;
    int remaining = total;
    set_progress(true, 0);

    char buffer[1024];
    while (remaining > 0) {
        int received = httpd_req_recv(req, buffer, remaining < static_cast<int>(sizeof(buffer)) ? remaining : sizeof(buffer));
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            fclose(file);
            remove(path);
            set_progress(false, 0);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "upload interrupted");
            return ESP_FAIL;
        }
        fwrite(buffer, 1, received, file);
        remaining -= received;
        if (total > 0) {
            set_progress(true, static_cast<uint8_t>((total - remaining) * 100 / total));
        }
    }
    fclose(file);
    set_progress(false, 0);

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
    if (!sd_card::is_mounted() || !is_safe_filename(name.c_str())) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
        return ESP_FAIL;
    }

    if (!remove_recursive(sd_path(name))) {
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

// Sends `record` as one chunk and moves `offset` (the archive's running
// byte count) past it.
bool send_zip_record(httpd_req_t* req, const std::string& record, uint32_t& offset) {
    offset += static_cast<uint32_t>(record.size());
    return httpd_resp_send_chunk(req, record.data(), record.size()) == ESP_OK;
}

// Sends one file's bytes as the data of `entry`, computing its CRC-32 on
// the way. False if the file can't be read in full or the client went
// away, so the caller can abort instead of finishing a corrupt archive.
bool send_zip_file_data(httpd_req_t* req, const std::string& path, ZipEntry& entry) {
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    uint32_t crc = 0;
    uint32_t sent = 0;
    bool ok = send_file_chunks(req, file, [&](const uint8_t* data, size_t len) {
        crc = crc32_update(crc, data, len);
        sent += static_cast<uint32_t>(len);
    });
    fclose(file);
    entry.crc32 = crc;
    return ok && sent == entry.size;
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
    char decoded_name[256];
    url_decode(req->uri + strlen("/zip/"), decoded_name, sizeof(decoded_name));

    // Strip every trailing slash: "/zip/pasta//" must not leave an empty
    // folder name, which would put "/..." names in the archive.
    size_t len = strlen(decoded_name);
    while (len > 0 && decoded_name[len - 1] == '/') {
        decoded_name[--len] = '\0';
    }

    if (!sd_card::is_mounted() || !is_safe_filename(decoded_name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid folder name or no SD card");
        return ESP_FAIL;
    }

    std::string full_dir = std::string(sd_card::mount_point()) + "/" + decoded_name;
    struct stat st {};
    if (stat(full_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Entry names are relative to the folder's parent ("fotos/a.jpg" for
    // /zip/viagem/fotos), so parent_dir + an entry's name is its SD path.
    const char* slash = strrchr(decoded_name, '/');
    const char* folder_name = (slash != nullptr) ? slash + 1 : decoded_name;
    std::string parent_dir = full_dir.substr(0, full_dir.size() - strlen(folder_name));

    std::vector<ZipEntry> entries;
    ZipEntry root;
    root.name = std::string(folder_name) + "/";
    entries.push_back(root);
    collect_zip_entries(full_dir, root.name, entries);

    if (!zip_within_limits(entries)) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "folder too large for .zip (max 4 GB, 65535 items)");
        return ESP_FAIL;
    }

    // folder_name comes from a path stat() just found on the FAT card, and
    // FAT names can't contain '"' or control characters, so it can't break
    // out of the quoted header value.
    httpd_resp_set_type(req, "application/zip");
    char content_disposition[300];
    snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s.zip\"", folder_name);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    std::string record;
    uint32_t offset = 0;
    for (ZipEntry& entry : entries) {
        entry.local_header_offset = offset;
        record.clear();
        zip_append_local_header(record, entry);
        if (!send_zip_record(req, record, offset)) {
            return ESP_FAIL;
        }
        if (entry.name.back() == '/') {
            continue;
        }
        if (!send_zip_file_data(req, parent_dir + entry.name, entry)) {
            ESP_LOGE(kTag, "zip of %s aborted at %s", decoded_name, entry.name.c_str());
            return ESP_FAIL;
        }
        offset += entry.size;
        record.clear();
        zip_append_data_descriptor(record, entry);
        if (!send_zip_record(req, record, offset)) {
            return ESP_FAIL;
        }
    }

    const uint32_t central_dir_offset = offset;
    for (const ZipEntry& entry : entries) {
        record.clear();
        zip_append_central_header(record, entry);
        if (!send_zip_record(req, record, offset)) {
            return ESP_FAIL;
        }
    }
    record.clear();
    zip_append_end_of_central_dir(
        record, static_cast<uint16_t>(entries.size()), offset - central_dir_offset, central_dir_offset);
    if (!send_zip_record(req, record, offset)) {
        return ESP_FAIL;
    }
    httpd_resp_send_chunk(req, nullptr, 0);

    ESP_LOGI(kTag, "zipped %s (%u entries)", decoded_name, static_cast<unsigned>(entries.size()));
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
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = nullptr;
        return err;
    }

    const httpd_uri_t* uris[] = {
        &kRootUri, &kStyleCssUri, &kAppJsUri, &kFilesPageUri, &kFilesItemUri, &kUploadUri, &kDeleteUri, &kZipUri};
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

} // namespace web_server
