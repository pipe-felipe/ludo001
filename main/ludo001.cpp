#include <cstdio>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "oled_display.hpp"
#include "wifi_manager.hpp"
#include "wifi_credentials.h" // generated at build time from .env, see main/CMakeLists.txt

namespace {

constexpr char kTag[] = "ludo001";

using oled_display::Ssd1306;

// Connected: shows "IP <address>" in the panel's yellow band (y 0-15).
// Not connected: shows a two-line message in the blue area below.
void draw_wifi_status(Ssd1306& display, bool connected, const char* ip) {
    display.clear();

    if (connected) {
        char line[24];
        snprintf(line, sizeof(line), "IP %s", ip);
        display.draw_text(2, 4, line);
    } else {
        display.draw_text(0, 20, "PRECISO ME CONECTAR");
        display.draw_text(0, 32, "A UM WIFI");
    }

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
        draw_wifi_status(display, false, "");
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

    bool last_connected = false;
    while (true) {
        bool connected = wifi_manager::is_connected();
        char ip[16] = "";
        if (connected) {
            wifi_manager::get_ip(ip, sizeof(ip));
        }

        if (display_ready && connected != last_connected) {
            draw_wifi_status(display, connected, ip);
            last_connected = connected;
        }

        if (connected) {
            ESP_LOGI(kTag, "wifi status: CONNECTED ip=%s", ip);
        } else {
            ESP_LOGW(kTag, "wifi status: DISCONNECTED");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
