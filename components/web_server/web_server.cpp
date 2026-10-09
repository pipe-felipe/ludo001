#include "web_server.hpp"

#include <cstdio>

#include "esp_http_server.h"
#include "esp_log.h"

#include "sd_card.hpp"
#include "wifi_manager.hpp"

namespace web_server {
namespace {

constexpr char kTag[] = "web_server";

httpd_handle_t s_server = nullptr;

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
            "</body></html>",
            wifi_connected ? "conectado" : "desconectado", ip);
    }

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, body);
}

constexpr httpd_uri_t kRootUri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = nullptr,
};

} // namespace

esp_err_t start() {
    if (s_server != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = nullptr;
        return err;
    }

    err = httpd_register_uri_handler(s_server, &kRootUri);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "httpd_register_uri_handler failed: %s", esp_err_to_name(err));
        httpd_stop(s_server);
        s_server = nullptr;
        return err;
    }

    return ESP_OK;
}

esp_err_t stop() {
    if (s_server == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = httpd_stop(s_server);
    s_server = nullptr;
    return err;
}

bool is_running() {
    return s_server != nullptr;
}

} // namespace web_server
