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

// Fills most of the screen with a checkmark to mean "connected", or an
// "X" to mean "not connected", inside a border so the icon is readable
// even if the panel is dim.
void draw_wifi_status(Ssd1306& display, bool connected) {
    display.clear();
    display.draw_rect(0, 0, Ssd1306::kWidth, Ssd1306::kHeight);

    if (connected) {
        display.draw_line(30, 34, 50, 50);
        display.draw_line(50, 50, 95, 15);
    } else {
        display.draw_line(35, 15, 93, 49);
        display.draw_line(93, 15, 35, 49);
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

        if (display_ready && connected != last_connected) {
            draw_wifi_status(display, connected);
            last_connected = connected;
        }

        if (connected) {
            char ip[16];
            wifi_manager::get_ip(ip, sizeof(ip));
            ESP_LOGI(kTag, "wifi status: CONNECTED ip=%s", ip);
        } else {
            ESP_LOGW(kTag, "wifi status: DISCONNECTED");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
