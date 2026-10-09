#include "oled_display.hpp"

#include <cstdlib>
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

    clear();
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

void Ssd1306::clear() {
    std::memset(tx_buffer_ + 1, 0x00, kFrameBytes);
}

void Ssd1306::fill() {
    std::memset(tx_buffer_ + 1, 0xFF, kFrameBytes);
}

void Ssd1306::set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    size_t byte_index = 1 + static_cast<size_t>(x) + (y / 8) * kWidth;
    uint8_t mask = static_cast<uint8_t>(1u << (y % 8));
    if (on) {
        tx_buffer_[byte_index] |= mask;
    } else {
        tx_buffer_[byte_index] &= static_cast<uint8_t>(~mask);
    }
}

void Ssd1306::draw_hline(int x, int y, int length, bool on) {
    for (int i = 0; i < length; ++i) {
        set_pixel(x + i, y, on);
    }
}

void Ssd1306::draw_vline(int x, int y, int length, bool on) {
    for (int i = 0; i < length; ++i) {
        set_pixel(x, y + i, on);
    }
}

void Ssd1306::draw_line(int x0, int y0, int x1, int y1, bool on) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = std::abs(dx) > std::abs(dy) ? std::abs(dx) : std::abs(dy);
    if (steps == 0) {
        set_pixel(x0, y0, on);
        return;
    }
    for (int i = 0; i <= steps; ++i) {
        int x = x0 + dx * i / steps;
        int y = y0 + dy * i / steps;
        set_pixel(x, y, on);
    }
}

void Ssd1306::draw_rect(int x, int y, int width, int height, bool on) {
    draw_hline(x, y, width, on);
    draw_hline(x, y + height - 1, width, on);
    draw_vline(x, y, height, on);
    draw_vline(x + width - 1, y, height, on);
}

esp_err_t Ssd1306::flush() {
    uint8_t set_window[] = {
        0x00, 0x21, 0x00, kWidth - 1,  // set column address range
        0x00, 0x22, 0x00, (kHeight / 8) - 1, // set page address range
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

    return i2c_master_transmit(dev_, tx_buffer_, sizeof(tx_buffer_), -1);
}

} // namespace oled_display
