#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "framebuffer.hpp"
#include "hal/gpio_types.h"

// I2C transport for a 128x64 SSD1306 OLED. Drawing itself lives in
// Framebuffer (framebuffer.hpp), which has no hardware dependency; this
// class only owns the I2C bus/device and pushes the framebuffer's bytes to
// the panel. See framebuffer() for drawing, flush() to push it to the
// panel.
namespace oled_display {

struct Config {
    i2c_port_num_t port = I2C_NUM_0;
    gpio_num_t sda = GPIO_NUM_21;
    gpio_num_t scl = GPIO_NUM_22;
    uint8_t address = 0x3C;
    uint32_t clock_hz = 400000;
};

class Ssd1306 {
public:
    esp_err_t init(const Config& config = Config());

    Framebuffer& framebuffer() { return framebuffer_; }
    const Framebuffer& framebuffer() const { return framebuffer_; }

    // Pushes the framebuffer to the panel over I2C.
    esp_err_t flush();

private:
    esp_err_t send_commands(const uint8_t* cmds, size_t len);

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;

    Framebuffer framebuffer_;

    // tx_buffer_[0] is the fixed I2C data-control byte (0x40); the rest is
    // a copy of framebuffer_'s bytes, refreshed on every flush() so the
    // whole GDDRAM mirror can be sent in one contiguous transfer.
    uint8_t tx_buffer_[Framebuffer::kByteCount + 1] = {0};
};

} // namespace oled_display
