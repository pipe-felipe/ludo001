#include "oled_display.hpp"

#include <cstring>

#include "esp_log.h"

namespace oled_display {
namespace {

constexpr char kTag[] = "oled_display";

// Standard SSD1306 128x64 init sequence (internal charge pump). Based on
// the sequence confirmed to light this exact panel (see the `embedded`
// skill's oled-ssd1306-096 hardware notes), with the addressing mode
// switched to horizontal (0x20, 0x00) so flush() can write the whole
// framebuffer in a single contiguous transfer.
constexpr uint8_t kInitSequence[] = {
    0xAE,       // display off
    0xD5, 0x80, // clock divide / osc freq
    0xA8, 0x3F, // multiplex ratio = 63 (64 rows)
    0xD3, 0x00, // display offset = 0
    0x40,       // display start line = 0
    0x8D, 0x14, // enable charge pump
    0x20, 0x00, // memory addressing mode = horizontal (A[1:0]: 00=horizontal, 01=vertical, 10=page)
    0xA1,       // segment remap
    0xC8,       // COM output scan direction, remapped
    0xDA, 0x12, // COM pins hardware configuration
    0x81, 0x7F, // contrast
    0xD9, 0xF1, // pre-charge period
    0xDB, 0x40, // VCOMH deselect level
    0xA4,       // resume to RAM content display
    0xA6,       // normal (not inverted)
    0xAF,       // display on
};

} // namespace

esp_err_t Ssd1306::init(const Config& config) {
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = config.port;
    bus_config.sda_io_num = config.sda;
    bus_config.scl_io_num = config.scl;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = config.address;
    dev_config.scl_speed_hz = config.clock_hz;

    err = i2c_master_bus_add_device(bus_, &dev_config, &dev_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    tx_buffer_[0] = 0x40; // data control byte, fixed for the life of the object

    err = send_commands(kInitSequence, sizeof(kInitSequence));
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "init sequence failed: %s", esp_err_to_name(err));
        return err;
    }

    framebuffer_.clear();
    return flush();
}

esp_err_t Ssd1306::send_commands(const uint8_t* cmds, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        uint8_t buf[2] = {0x00, cmds[i]}; // 0x00 = command control byte
        esp_err_t err = i2c_master_transmit(dev_, buf, sizeof(buf), -1);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t Ssd1306::flush() {
    uint8_t set_window[] = {
        0x00, 0x21, 0x00, Framebuffer::kWidth - 1,       // set column address range
        0x00, 0x22, 0x00, (Framebuffer::kHeight / 8) - 1, // set page address range
    };
    // Two commands, each with its own control byte; send separately.
    esp_err_t err = i2c_master_transmit(dev_, set_window, 4, -1);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_master_transmit(dev_, set_window + 4, 4, -1);
    if (err != ESP_OK) {
        return err;
    }

    std::memcpy(tx_buffer_ + 1, framebuffer_.data(), Framebuffer::kByteCount);
    return i2c_master_transmit(dev_, tx_buffer_, sizeof(tx_buffer_), -1);
}

} // namespace oled_display
