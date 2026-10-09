#include "sd_card.hpp"

#include <cstring>

#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace sd_card {
namespace {

constexpr char kTag[] = "sd_card";

sdmmc_card_t* s_card = nullptr;
char s_mount_point[32] = {0};
spi_host_device_t s_host_slot = SPI2_HOST;
bool s_mounted = false;

} // namespace

esp_err_t mount(const Config& config) {
    if (s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = config.host;
    host.max_freq_khz = config.max_freq_khz;

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = config.mosi;
    bus_cfg.miso_io_num = config.miso;
    bus_cfg.sclk_io_num = config.sck;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    esp_err_t err = spi_bus_initialize(config.host, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = config.cs;
    slot_config.host_id = config.host;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = config.format_if_mount_failed;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;

    err = esp_vfs_fat_sdspi_mount(config.mount_point, &host, &slot_config, &mount_config, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_vfs_fat_sdspi_mount failed: %s", esp_err_to_name(err));
        spi_bus_free(config.host);
        return err;
    }

    s_host_slot = config.host;
    std::strncpy(s_mount_point, config.mount_point, sizeof(s_mount_point) - 1);
    s_mounted = true;
    return ESP_OK;
}

esp_err_t unmount() {
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_vfs_fat_sdcard_unmount(s_mount_point, s_card);
    spi_bus_free(s_host_slot);
    s_card = nullptr;
    s_mounted = false;
    return err;
}

bool is_mounted() {
    return s_mounted;
}

uint64_t capacity_mb() {
    if (!s_mounted || s_card == nullptr) {
        return 0;
    }
    return static_cast<uint64_t>(s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024);
}

} // namespace sd_card
