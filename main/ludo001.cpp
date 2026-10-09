#include <cstdio>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "oled_display.hpp"
#include "sd_card.hpp"
#include "wifi_manager.hpp"

namespace {

constexpr char kTag[] = "ludo001";

// TODO: fill in before relying on Wi-Fi.
constexpr char kWifiSsid[] = "YOUR_SSID";
constexpr char kWifiPassword[] = "YOUR_PASSWORD";

} // namespace

extern "C" void app_main(void) {
    oled_display::Ssd1306 display;
    esp_err_t err = display.init();
    if (err == ESP_OK) {
        display.clear();
        display.draw_rect(0, 0, oled_display::Ssd1306::kWidth, oled_display::Ssd1306::kHeight);
        display.flush();
        ESP_LOGI(kTag, "display ready");
    } else {
        ESP_LOGE(kTag, "display init failed: %s", esp_err_to_name(err));
    }

    err = sd_card::mount();
    if (err == ESP_OK) {
        ESP_LOGI(kTag, "sd card mounted, %llu MB", sd_card::capacity_mb());
    } else {
        ESP_LOGE(kTag, "sd card mount failed: %s", esp_err_to_name(err));
    }

    wifi_manager::StaConfig wifi_config{kWifiSsid, kWifiPassword};
    err = wifi_manager::connect(wifi_config);
    if (err == ESP_OK) {
        char ip[16];
        wifi_manager::get_ip(ip, sizeof(ip));
        ESP_LOGI(kTag, "wifi connected, ip=%s", ip);
    } else {
        ESP_LOGE(kTag, "wifi connect failed: %s", esp_err_to_name(err));
    }

    int count = 0;
    while (true) {
        printf("ludo001: hello #%d\n", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
