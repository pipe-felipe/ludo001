#include <cstdio>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "oled_display.hpp"
#include "sd_card.hpp"
#include "web_server.hpp"
#include "wifi_manager.hpp"
#include "wifi_credentials.h" // generated at build time from .env, see main/CMakeLists.txt

namespace {

constexpr char kTag[] = "ludo001";

using oled_display::Ssd1306;

constexpr int kBarX = 4;
constexpr int kBarY = 20;
constexpr int kBarW = 120;
constexpr int kBarH = 8;

// Yellow band (y 0-15): Wi-Fi status, as "IP <address>" or "SEM WIFI".
// Blue area (y 16-63): upload progress bar, file count and SD space.
void draw_status(Ssd1306& display, bool connected, const char* ip, uint32_t file_count,
    uint64_t used_mb, uint64_t free_mb, web_server::UploadProgress progress) {
    auto& fb = display.framebuffer();
    fb.clear();

    if (connected) {
        char line[24];
        snprintf(line, sizeof(line), "IP %s", ip);
        fb.draw_text(2, 4, line);
    } else {
        fb.draw_text(2, 4, "SEM WIFI");
    }

    fb.draw_rect(kBarX, kBarY, kBarW, kBarH);
    if (progress.active && progress.percent > 0) {
        int fill_w = (kBarW - 2) * progress.percent / 100;
        fb.fill_rect(kBarX + 1, kBarY + 1, fill_w, kBarH - 2);
    }

    char items_line[24];
    snprintf(items_line, sizeof(items_line), "ITENS %lu", static_cast<unsigned long>(file_count));
    fb.draw_text(0, 32, items_line);

    char mem_line[32];
    snprintf(mem_line, sizeof(mem_line), "USO %llu/%lluMB",
        static_cast<unsigned long long>(used_mb), static_cast<unsigned long long>(free_mb));
    fb.draw_text(0, 44, mem_line);

    display.flush();
}

} // namespace

extern "C" void app_main(void) {
    Ssd1306 display;
    esp_err_t err = display.init();
    bool display_ready = (err == ESP_OK);
    if (!display_ready) {
        ESP_LOGE(kTag, "display init failed: %s", esp_err_to_name(err));
    } else {
        draw_status(display, false, "", 0, 0, 0, web_server::UploadProgress{});
    }

    wifi_manager::StaConfig wifi_config{WIFI_SSID, WIFI_PASSWORD};
    err = wifi_manager::connect(wifi_config);
    if (err == ESP_OK) {
        char ip[16];
        wifi_manager::get_ip(ip, sizeof(ip));
        ESP_LOGI(kTag, "wifi connected, ssid=%s ip=%s", WIFI_SSID, ip);
    } else {
        ESP_LOGE(kTag, "wifi connect failed: ssid=%s error=%s", WIFI_SSID, esp_err_to_name(err));
    }

    err = sd_card::mount();
    if (err == ESP_OK) {
        ESP_LOGI(kTag, "sd card mounted, capacity=%llu MB", static_cast<unsigned long long>(sd_card::capacity_mb()));
    } else {
        ESP_LOGE(kTag, "sd card mount failed: %s", esp_err_to_name(err));
    }

    err = web_server::start();
    if (err == ESP_OK) {
        ESP_LOGI(kTag, "web server started");
    } else {
        ESP_LOGE(kTag, "web server start failed: %s", esp_err_to_name(err));
    }

    bool last_connected = false;
    while (true) {
        bool connected = wifi_manager::is_connected();
        char ip[16] = "";
        if (connected) {
            wifi_manager::get_ip(ip, sizeof(ip));
        }

        if (connected != last_connected) {
            if (connected) {
                ESP_LOGI(kTag, "wifi status: CONNECTED ip=%s", ip);
            } else {
                ESP_LOGW(kTag, "wifi status: DISCONNECTED");
            }
            last_connected = connected;
        }

        if (display_ready) {
            uint32_t file_count = sd_card::file_count();
            uint64_t used_mb = 0;
            uint64_t free_mb = 0;
            sd_card::space_info(&used_mb, &free_mb);
            draw_status(display, connected, ip, file_count, used_mb, free_mb, web_server::upload_progress());
        }

        vTaskDelay(pdMS_TO_TICKS(400));
    }
}
