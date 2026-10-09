#include "wifi_manager.hpp"

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

namespace wifi_manager {
namespace {

constexpr char kTag[] = "wifi_manager";
constexpr int kConnectedBit = BIT0;

EventGroupHandle_t s_event_group = nullptr;
bool s_initialized = false;
bool s_started = false;
char s_ip[16] = {0};

void event_handler(void* /*arg*/, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ip[0] = '\0';
        xEventGroupClearBits(s_event_group, kConnectedBit);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(data);
        esp_ip4addr_ntoa(&event->ip_info.ip, s_ip, sizeof(s_ip));
        xEventGroupSetBits(s_event_group, kConnectedBit);
    }
}

esp_err_t ensure_initialized() {
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    s_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    s_initialized = true;
    return ESP_OK;
}

} // namespace

esp_err_t connect(const StaConfig& config) {
    esp_err_t err = ensure_initialized();
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), config.ssid,
                 sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), config.password,
                 sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    xEventGroupClearBits(s_event_group, kConnectedBit);

    if (!s_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_started = true;
    } else {
        ESP_ERROR_CHECK(esp_wifi_connect());
    }

    EventBits_t bits = xEventGroupWaitBits(s_event_group, kConnectedBit, pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(config.connect_timeout_ms));

    if (!(bits & kConnectedBit)) {
        ESP_LOGW(kTag, "timed out waiting for IP after %u ms",
                 static_cast<unsigned>(config.connect_timeout_ms));
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(kTag, "connected, ip=%s", s_ip);
    return ESP_OK;
}

esp_err_t disconnect() {
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_wifi_disconnect();
}

bool is_connected() {
    return s_initialized && (xEventGroupGetBits(s_event_group) & kConnectedBit);
}

esp_err_t get_ip(char* out, size_t out_len) {
    if (!is_connected() || out_len == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    std::strncpy(out, s_ip, out_len - 1);
    out[out_len - 1] = '\0';
    return ESP_OK;
}

} // namespace wifi_manager
