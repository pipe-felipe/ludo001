#include "web_server.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "mdns.h"

#include "sd_card.hpp"
#include "wifi_manager.hpp"

namespace web_server {
namespace {

constexpr char kTag[] = "web_server";
constexpr char kHostname[] = "ludo001"; // reachable at http://ludo001.local/

httpd_handle_t s_server = nullptr;
bool s_mdns_started = false;

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Decodes %XX escapes (as produced by encodeURIComponent on the client).
void url_decode(const char* in, char* out, size_t out_size) {
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < out_size; ++i) {
        int hi = (in[i] == '%') ? hex_value(in[i + 1]) : -1;
        int lo = (hi >= 0) ? hex_value(in[i + 2]) : -1;
        if (hi >= 0 && lo >= 0) {
            out[o++] = static_cast<char>((hi << 4) | lo);
            i += 2;
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

// Percent-encodes everything outside the URL-safe unreserved set, so file
// names with spaces or accents still work as href targets.
void url_encode(const char* in, char* out, size_t out_size) {
    constexpr char kHex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 4 < out_size; ++i) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out[o++] = static_cast<char>(c);
        } else {
            out[o++] = '%';
            out[o++] = kHex[c >> 4];
            out[o++] = kHex[c & 0xF];
        }
    }
    out[o] = '\0';
}

// Rejects anything that could escape the SD card's mount point.
bool is_safe_filename(const char* name) {
    return name[0] != '\0' && name[0] != '/' && std::strstr(name, "..") == nullptr;
}

esp_err_t root_handler(httpd_req_t* req) {
    char body[512];

    bool wifi_connected = wifi_manager::is_connected();
    char ip[16] = "-";
    if (wifi_connected) {
        wifi_manager::get_ip(ip, sizeof(ip));
    }

    bool sd_mounted = sd_card::is_mounted();
    uint64_t sd_mb = sd_mounted ? sd_card::capacity_mb() : 0;

    if (sd_mounted) {
        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
            "<title>ludo001</title></head><body>"
            "<h1>ludo001</h1>"
            "<p>Wi-Fi: %s (IP %s)</p>"
            "<p>SD card: montado, %llu MB</p>"
            "<p><a href=\"/files\">arquivos</a></p>"
            "</body></html>",
            wifi_connected ? "conectado" : "desconectado", ip,
            static_cast<unsigned long long>(sd_mb));
    } else {
        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
            "<title>ludo001</title></head><body>"
            "<h1>ludo001</h1>"
            "<p>Wi-Fi: %s (IP %s)</p>"
            "<p>SD card: nao montado</p>"
            "<p><a href=\"/files\">arquivos</a></p>"
            "</body></html>",
            wifi_connected ? "conectado" : "desconectado", ip);
    }

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, body);
}

constexpr char kFilesPageHead[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<title>ludo001 - arquivos</title></head><body>"
    "<h1>Arquivos no SD</h1>";

constexpr char kFilesPageFoot[] =
    "<h2>Enviar arquivo</h2>"
    "<input type=\"file\" id=\"f\"><button onclick=\"up()\">Enviar</button>"
    "<p id=\"status\"></p>"
    "<script>"
    "function up(){"
    "var f=document.getElementById('f').files[0];"
    "if(!f){return;}"
    "document.getElementById('status').textContent='enviando...';"
    "fetch('/upload/'+encodeURIComponent(f.name),{method:'POST',body:f})"
    ".then(function(r){return r.text();})"
    ".then(function(t){document.getElementById('status').textContent=t;location.reload();})"
    ".catch(function(e){document.getElementById('status').textContent='erro: '+e;});"
    "}"
    "</script></body></html>";

// GET /files -- lists the SD card and offers a plain upload form (the
// upload itself is a raw-body fetch(), not multipart/form-data, to avoid
// writing a multipart parser for what's meant to be a minimal tool).
esp_err_t files_page_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, kFilesPageHead, HTTPD_RESP_USE_STRLEN);

    if (!sd_card::is_mounted()) {
        httpd_resp_send_chunk(req, "<p>SD card nao montado.</p>", HTTPD_RESP_USE_STRLEN);
    } else {
        DIR* dir = opendir(sd_card::mount_point());
        if (dir == nullptr) {
            httpd_resp_send_chunk(req, "<p>Nao foi possivel abrir o SD.</p>", HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_send_chunk(req, "<ul>", HTTPD_RESP_USE_STRLEN);
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_name[0] == '.') {
                    continue;
                }
                char encoded_name[200];
                url_encode(entry->d_name, encoded_name, sizeof(encoded_name));
                char item[700];
                snprintf(item, sizeof(item), "<li><a href=\"/files/%s\">%s</a></li>", encoded_name, entry->d_name);
                httpd_resp_send_chunk(req, item, HTTPD_RESP_USE_STRLEN);
            }
            closedir(dir);
            httpd_resp_send_chunk(req, "</ul>", HTTPD_RESP_USE_STRLEN);
        }
    }

    httpd_resp_send_chunk(req, kFilesPageFoot, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
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
// page's JS sends the File object directly as the body (see
// kFilesPageFoot), so there's no multipart boundary to parse.
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

    char buffer[1024];
    int remaining = req->content_len;
    while (remaining > 0) {
        int received = httpd_req_recv(req, buffer, remaining < static_cast<int>(sizeof(buffer)) ? remaining : sizeof(buffer));
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            fclose(file);
            remove(path);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "upload interrupted");
            return ESP_FAIL;
        }
        fwrite(buffer, 1, received, file);
        remaining -= received;
    }
    fclose(file);

    ESP_LOGI(kTag, "uploaded %s", decoded_name);
    return httpd_resp_sendstr(req, "OK");
}

constexpr httpd_uri_t kRootUri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
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

    const httpd_uri_t* uris[] = {&kRootUri, &kFilesPageUri, &kDownloadUri, &kUploadUri};
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

} // namespace web_server
