#include "web_server.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

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

void format_size(long size, char* out, size_t out_size) {
    if (size < 1024) {
        snprintf(out, out_size, "%ld B", size);
    } else if (size < 1024 * 1024) {
        snprintf(out, out_size, "%.1f KB", size / 1024.0);
    } else {
        snprintf(out, out_size, "%.1f MB", size / (1024.0 * 1024.0));
    }
}

// Shared across both pages. Plain CSS, no external fonts/scripts, so the
// whole UI still comes from a single request to the device.
constexpr char kStyle[] =
    "<style>"
    "*{box-sizing:border-box;}"
    "body{margin:0;padding:28px 16px;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
    "background:#f1f4f8;color:#1f2430;}"
    ".card{max-width:420px;margin:0 auto 16px;background:#fff;border-radius:14px;"
    "box-shadow:0 2px 10px rgba(0,0,0,.07);padding:22px 24px;}"
    "h1{margin:0 0 14px;font-size:1.3rem;}"
    "h2{margin:4px 0 12px;font-size:1rem;color:#555;}"
    "p{margin:8px 0;}"
    ".pill{display:inline-block;padding:2px 10px;border-radius:999px;font-size:.82rem;font-weight:600;}"
    ".ok{background:#dcfce7;color:#15803d;}"
    ".bad{background:#fee2e2;color:#b91c1c;}"
    ".btn{display:inline-block;background:#2563eb;color:#fff;padding:9px 16px;border-radius:8px;"
    "text-decoration:none;font-weight:600;font-size:.92rem;border:none;cursor:pointer;}"
    ".btn:hover{background:#1d4ed8;}"
    "ul{list-style:none;margin:0;padding:0;}"
    "li{display:flex;align-items:center;gap:10px;padding:10px 0;"
    "border-bottom:1px solid #eef0f3;}"
    "li:last-child{border-bottom:none;}"
    "li a{flex:1 1 auto;min-width:0;color:#2563eb;text-decoration:none;font-weight:500;"
    "overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}"
    "li a:hover{text-decoration:underline;}"
    ".size{flex:0 0 auto;width:56px;text-align:right;color:#8a93a3;font-size:.8rem;white-space:nowrap;}"
    ".del{flex:0 0 auto;width:52px;text-align:right;background:none;border:none;color:#b91c1c;"
    "font-size:.8rem;cursor:pointer;padding:2px 0;}"
    ".del:hover{text-decoration:underline;}"
    ".empty{color:#8a93a3;font-style:italic;margin:4px 0 0;}"
    ".upload-row{display:flex;align-items:center;gap:10px;flex-wrap:wrap;}"
    "input[type=file]{font-size:.85rem;max-width:100%;}"
    "#status{color:#8a93a3;font-size:.85rem;min-height:1.2em;margin-top:8px;}"
    "</style>";

esp_err_t root_handler(httpd_req_t* req) {
    bool wifi_connected = wifi_manager::is_connected();
    char wifi_line[64];
    if (wifi_connected) {
        char ip[16];
        wifi_manager::get_ip(ip, sizeof(ip));
        snprintf(wifi_line, sizeof(wifi_line), "<span class=\"pill ok\">conectado</span> %s", ip);
    } else {
        std::strcpy(wifi_line, "<span class=\"pill bad\">desconectado</span>");
    }

    bool sd_mounted = sd_card::is_mounted();
    char sd_line[48];
    if (sd_mounted) {
        snprintf(sd_line, sizeof(sd_line), "<span class=\"pill ok\">montado</span> %llu MB",
            static_cast<unsigned long long>(sd_card::capacity_mb()));
    } else {
        std::strcpy(sd_line, "<span class=\"pill bad\">nao montado</span>");
    }

    char body[2304];
    snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>ludo001</title>%s</head><body>"
        "<div class=\"card\">"
        "<h1>ludo001</h1>"
        "<p>Wi-Fi: %s</p>"
        "<p>SD card: %s</p>"
        "<p><a class=\"btn\" href=\"/files\">Ver arquivos</a></p>"
        "</div>"
        "</body></html>",
        kStyle, wifi_line, sd_line);

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, body);
}

constexpr char kFilesPageHeadTop[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>ludo001 - arquivos</title>";

constexpr char kFilesPageHeadBottom[] =
    "</head><body>"
    "<div class=\"card\"><h1>Arquivos no SD</h1>";

constexpr char kFilesPageFoot[] =
    "</div>"
    "<div class=\"card\">"
    "<h2>Enviar arquivo</h2>"
    "<div class=\"upload-row\">"
    "<input type=\"file\" id=\"f\">"
    "<button class=\"btn\" onclick=\"up()\">Enviar</button>"
    "</div>"
    "<p id=\"status\"></p>"
    "</div>"
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
    "function del(btn){"
    "var encoded=btn.getAttribute('data-name');"
    "if(!confirm('Apagar '+decodeURIComponent(encoded)+'?')){return;}"
    "fetch('/delete/'+encoded,{method:'POST'})"
    ".then(function(r){return r.text();})"
    ".then(function(){location.reload();})"
    ".catch(function(e){alert('erro: '+e);});"
    "}"
    "</script></body></html>";

// GET /files -- lists the SD card and offers a plain upload form (the
// upload itself is a raw-body fetch(), not multipart/form-data, to avoid
// writing a multipart parser for what's meant to be a minimal tool).
esp_err_t files_page_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, kFilesPageHeadTop, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, kStyle, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, kFilesPageHeadBottom, HTTPD_RESP_USE_STRLEN);

    if (!sd_card::is_mounted()) {
        httpd_resp_send_chunk(req, "<p class=\"empty\">SD card nao montado.</p>", HTTPD_RESP_USE_STRLEN);
    } else {
        DIR* dir = opendir(sd_card::mount_point());
        if (dir == nullptr) {
            httpd_resp_send_chunk(req, "<p class=\"empty\">Nao foi possivel abrir o SD.</p>", HTTPD_RESP_USE_STRLEN);
        } else {
            bool any_entries = false;
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_name[0] == '.') {
                    continue;
                }
                if (!any_entries) {
                    httpd_resp_send_chunk(req, "<ul>", HTTPD_RESP_USE_STRLEN);
                    any_entries = true;
                }

                char full_path[300];
                snprintf(full_path, sizeof(full_path), "%s/%s", sd_card::mount_point(), entry->d_name);
                struct stat st {};
                char size_text[24] = "";
                if (stat(full_path, &st) == 0) {
                    format_size(static_cast<long>(st.st_size), size_text, sizeof(size_text));
                }

                char encoded_name[200];
                url_encode(entry->d_name, encoded_name, sizeof(encoded_name));
                char item[1100];
                snprintf(item, sizeof(item),
                    "<li><a href=\"/files/%s\">%s</a><span class=\"size\">%s</span>"
                    "<button class=\"del\" data-name=\"%s\" onclick=\"del(this)\">Apagar</button></li>",
                    encoded_name, entry->d_name, size_text, encoded_name);
                httpd_resp_send_chunk(req, item, HTTPD_RESP_USE_STRLEN);
            }
            closedir(dir);
            if (any_entries) {
                httpd_resp_send_chunk(req, "</ul>", HTTPD_RESP_USE_STRLEN);
            } else {
                httpd_resp_send_chunk(req, "<p class=\"empty\">Nenhum arquivo ainda.</p>", HTTPD_RESP_USE_STRLEN);
            }
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

    const httpd_uri_t* uris[] = {&kRootUri, &kFilesPageUri, &kDownloadUri, &kUploadUri, &kDeleteUri};
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
