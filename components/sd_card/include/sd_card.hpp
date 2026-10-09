#pragma once

#include <cstdint>

#include "driver/spi_common.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

// Mounts a micro SD card over SPI as a FAT filesystem at a VFS mount
// point, so plain fopen/fread/fwrite/fclose work against it.
//
// Default pins match the module wired in this project's setup (VSPI:
// CS=GPIO5, SCK=GPIO18, MOSI=GPIO23, MISO=GPIO19). That module needs its
// VCC pin on 5V, not 3.3V -- see the embedded skill's
// microsd-spi-module-large hardware notes.
namespace sd_card {

struct Config {
    gpio_num_t cs = GPIO_NUM_5;
    gpio_num_t sck = GPIO_NUM_18;
    gpio_num_t mosi = GPIO_NUM_23;
    gpio_num_t miso = GPIO_NUM_19;
    spi_host_device_t host = SPI2_HOST;
    int max_freq_khz = 4000;
    const char* mount_point = "/sdcard";
    bool format_if_mount_failed = false;
};

esp_err_t mount(const Config& config = Config());
esp_err_t unmount();
bool is_mounted();

// Card capacity in megabytes, or 0 if not mounted.
uint64_t capacity_mb();

// VFS mount point in use (e.g. "/sdcard"), or "" if not mounted.
const char* mount_point();

// Number of regular entries at the mount point's root (dotfiles
// excluded), or 0 if not mounted.
uint32_t file_count();

// Used/free space on the mounted filesystem, in megabytes.
// ESP_ERR_INVALID_STATE if not mounted.
esp_err_t space_info(uint64_t* used_mb, uint64_t* free_mb);

} // namespace sd_card
