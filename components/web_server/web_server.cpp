#include "web_server.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "mdns.h"
#include "web_server_render.hpp"
#include "web_server_util.hpp"

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

esp_err_t send_embedded(httpd_req_t* req, const char* content_type, const uint8_t* start, const uint8_t* end) {
    httpd_resp_set_type(req, content_type);
    return httpd_resp_send(req, reinterpret_cast<const char*>(start), end - start - 1);
}

esp_err_t style_css_handler(httpd_req_t* req) {
    return send_embedded(req, "text/css", style_css_start, style_css_end);
}

esp_err_t app_js_handler(httpd_req_t* req) {
    return send_embedded(req, "application/javascript", app_js_start, app_js_end);
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

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, body.c_str(), body.size());
}

// GET /files -- lists the SD card and offers a plain upload form (the
// upload itself is a raw-body fetch(), not multipart/form-data, to avoid
// writing a multipart parser for what's meant to be a minimal tool).
esp_err_t files_page_handler(httpd_req_t* req) {
    std::string list_html;
    if (!sd_card::is_mounted()) {
        list_html = "<p class=\"empty\">SD card nao montado.</p>";
    } else {
        DIR* dir = opendir(sd_card::mount_point());
        if (dir == nullptr) {
            list_html = "<p class=\"empty\">Nao foi possivel abrir o SD.</p>";
        } else {
            std::vector<FileEntry> entries;
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_name[0] == '.') {
                    continue;
                }
                char full_path[300];
                snprintf(full_path, sizeof(full_path), "%s/%s", sd_card::mount_point(), entry->d_name);
                struct stat st {};
                char size_text[24] = "";
                if (stat(full_path, &st) == 0) {
                    format_size(static_cast<long>(st.st_size), size_text, sizeof(size_text));
                }
                entries.push_back(FileEntry{entry->d_name, size_text});
            }
            closedir(dir);
            list_html = render_file_list(entries);
        }
    }

    std::string body =
        render_template(embedded_text(files_html_start, files_html_end), {{"FILE_LIST", list_html}});

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, body.c_str(), body.size());
}

// GET /files/<name> -- streams the file back with a Content-Disposition
// that makes the browser save it instead of trying to render it.
esp_err_t download_handler(httpd_req_t* req) {
    char decoded_name[200];
    url_decode(req->uri + strlen("/files/"), decoded_name, sizeof(decoded_name));

    if (!sd_card::is_mounted() || !is_safe_filename(decoded_name)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", sd_card::mount_point(), decoded_name);

    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/octet-stream");
    char content_disposition[256];
    snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s\"", decoded_name);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    char buffer[1024];
    size_t read_bytes;
    esp_err_t err = ESP_OK;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            err = ESP_FAIL;
            break;
        }
    }
    fclose(file);
    httpd_resp_send_chunk(req, nullptr, 0);
    return err;
}

// POST /upload/<name> -- writes the raw request body to the SD card. The
// page's JS (web/app.js) sends the File object directly as the body, so
// there's no multipart boundary to parse.
esp_err_t upload_handler(httpd_req_t* req) {
    char decoded_name[200];
    url_decode(req->uri + strlen("/upload/"), decoded_name, sizeof(decoded_name));

    if (!sd_card::is_mounted() || !is_safe_filename(decoded_name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
        return ESP_FAIL;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", sd_card::mount_point(), decoded_name);

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

    ESP_LOGI(kTag, "uploaded %s", decoded_name);
    return httpd_resp_sendstr(req, "OK");
}

// POST /delete/<name> -- removes the file from the SD card.
esp_err_t delete_handler(httpd_req_t* req) {
    char decoded_name[200];
    url_decode(req->uri + strlen("/delete/"), decoded_name, sizeof(decoded_name));

    if (!sd_card::is_mounted() || !is_safe_filename(decoded_name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name or no SD card");
        return ESP_FAIL;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", sd_card::mount_point(), decoded_name);

    if (remove(path) != 0) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "deleted %s", decoded_name);
    return httpd_resp_sendstr(req, "OK");
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

constexpr httpd_uri_t kDownloadUri = {
    .uri = "/files/*",
    .method = HTTP_GET,
    .handler = download_handler,
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

} // namespace

esp_err_t start() {
    if (s_server != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // needed for "/files/*" and "/upload/*"
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = nullptr;
        return err;
    }

    const httpd_uri_t* uris[] = {
        &kRootUri, &kStyleCssUri, &kAppJsUri, &kFilesPageUri, &kDownloadUri, &kUploadUri, &kDeleteUri};
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
