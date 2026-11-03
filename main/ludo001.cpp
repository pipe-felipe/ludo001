#include <cstdio>
#include <optional>

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
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

// A firmware fresh from an OTA update has this long after boot to bring up
// Wi-Fi and the web server, or the bootloader goes back to the previous one.
constexpr int kNewFirmwareTimeoutS = 60;
constexpr int64_t kNewFirmwareTimeoutUs = int64_t{kNewFirmwareTimeoutS} * 1000 * 1000;

// Everything the OLED shows, gathered by the loop in app_main.
struct StatusSnapshot {
    bool connected = false;
    char ip[16] = "";
    uint32_t item_count = 0;
    sd_card::SpaceInfo space;
    web_server::UploadProgress progress;
    const char* version = ""; // the running firmware's, from esp_app_get_description()
};

// Yellow band (y 0-15): Wi-Fi status, as "IP <address>" or "SEM WIFI".
// Blue area (y 16-63): progress bar, item count (or "ATUALIZANDO <n>%"
// during a firmware update), SD space and firmware version.
void draw_status(Ssd1306& display, const StatusSnapshot& status) {
    auto& fb = display.framebuffer();
    fb.clear();

    if (status.connected) {
        char line[24];
        snprintf(line, sizeof(line), "IP %s", status.ip);
        fb.draw_text(2, 4, line);
    } else {
        fb.draw_text(2, 4, "SEM WIFI");
    }

    fb.draw_rect(kBarX, kBarY, kBarW, kBarH);
    if (status.progress.kind != web_server::UploadKind::None && status.progress.percent > 0) {
        int fill_w = (kBarW - 2) * status.progress.percent / 100;
        fb.fill_rect(kBarX + 1, kBarY + 1, fill_w, kBarH - 2);
    }

    char info_line[24];
    if (status.progress.kind == web_server::UploadKind::Firmware) {
        snprintf(info_line, sizeof(info_line), "ATUALIZANDO %u%%", static_cast<unsigned>(status.progress.percent));
    } else {
        snprintf(info_line, sizeof(info_line), "ITENS %lu", static_cast<unsigned long>(status.item_count));
    }
    fb.draw_text(0, 32, info_line);

    char mem_line[32];
    snprintf(mem_line, sizeof(mem_line), "USO %llu/%lluMB",
        static_cast<unsigned long long>(status.space.used_mb), static_cast<unsigned long long>(status.space.free_mb));
    fb.draw_text(0, 44, mem_line);

    char version_line[40];
    snprintf(version_line, sizeof(version_line), "FW %s", status.version);
    fb.draw_text(0, 56, version_line);

    display.flush();
}

// True while the running firmware (in `running`, which may be null if the
// partition can't be found) came from an OTA update and hasn't been confirmed
// yet: unless check_new_firmware() confirms it, the next restart goes back to
// the previous one.
bool new_firmware_pending(const esp_partition_t* running) {
    esp_ota_img_states_t state;
    return running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

// Confirms a new firmware once Wi-Fi and the web server are both up, since
// only then can the next update reach the board. If the confirmation can't be
// written, the firmware stays unconfirmed and the loop tries again. Rolls
// back to the previous firmware (restarting) when kNewFirmwareTimeoutUs
// passes first, which also ends those retries. If even the rollback fails
// (there is no previous firmware to go back to), the check gives up instead
// of retrying forever. Returns whether the check is still pending.
bool check_new_firmware(bool wifi_connected) {
    if (wifi_connected && web_server::is_running()) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGI(kTag, "new firmware confirmed");
            return false;
        }
        ESP_LOGE(kTag, "could not confirm the new firmware: %s", esp_err_to_name(err));
    }
    if (esp_timer_get_time() >= kNewFirmwareTimeoutUs) {
        ESP_LOGE(kTag, "new firmware not confirmed within %d s, rolling back", kNewFirmwareTimeoutS);
        esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot(); // only returns on failure
        ESP_LOGE(kTag, "rollback failed, staying on this firmware: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

} // namespace

extern "C" void app_main(void) {
    const esp_app_desc_t* app = esp_app_get_description();
    const esp_partition_t* running = esp_ota_get_running_partition();
    ESP_LOGI(kTag, "firmware %s running from %s", app->version, running != nullptr ? running->label : "an unknown partition");
    bool firmware_pending = new_firmware_pending(running);

    Ssd1306 display;
    esp_err_t err = display.init();
    bool display_ready = (err == ESP_OK);
    if (!display_ready) {
        ESP_LOGE(kTag, "display init failed: %s", esp_err_to_name(err));
    } else {
        draw_status(display, StatusSnapshot{});
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

    StatusSnapshot status;
    status.version = app->version;
    std::optional<uint32_t> sd_read_at; // content_version() when the SD figures were last read
    bool last_connected = false;
    while (true) {
        status.connected = wifi_manager::is_connected();
        status.ip[0] = '\0';
        if (status.connected) {
            wifi_manager::get_ip(status.ip, sizeof(status.ip));
        }

        if (status.connected != last_connected) {
            if (status.connected) {
                ESP_LOGI(kTag, "wifi status: CONNECTED ip=%s", status.ip);
            } else {
                ESP_LOGW(kTag, "wifi status: DISCONNECTED");
            }
            last_connected = status.connected;
        }

        if (firmware_pending) {
            firmware_pending = check_new_firmware(status.connected);
        }

        if (display_ready) {
            // Walking the card is slow over SPI: re-read it only at start-up
            // and after an upload or delete.
            uint32_t version = web_server::content_version();
            if (sd_read_at != version) {
                status.item_count = sd_card::item_count();
                status.space = sd_card::space_info();
                sd_read_at = version;
                ESP_LOGI(kTag, "sd card: %lu items, %llu MB used, %llu MB free",
                    static_cast<unsigned long>(status.item_count), static_cast<unsigned long long>(status.space.used_mb),
                    static_cast<unsigned long long>(status.space.free_mb));
            }
            status.progress = web_server::upload_progress();
            draw_status(display, status);
        }

        vTaskDelay(pdMS_TO_TICKS(400));
    }
}
