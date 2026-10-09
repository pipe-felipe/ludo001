#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

// Framebuffer-level access to a 128x64 I2C SSD1306 OLED. Covers init,
// pixel/line/rect drawing and pushing the framebuffer to the panel.
// Text/font rendering is intentionally out of scope here -- build it on
// top of set_pixel() if/when needed.
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
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;

    esp_err_t init(const Config& config = Config());

    void clear();
    void fill();
    void set_pixel(int x, int y, bool on);
    void draw_hline(int x, int y, int length, bool on = true);
    void draw_vline(int x, int y, int length, bool on = true);
    void draw_rect(int x, int y, int width, int height, bool on = true);

    // Pushes the framebuffer to the panel over I2C.
    esp_err_t flush();

private:
    static constexpr size_t kFrameBytes = kWidth * kHeight / 8;

    esp_err_t send_commands(const uint8_t* cmds, size_t len);

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;

    // tx_buffer_[0] is the fixed I2C data-control byte (0x40); the rest
    // is the actual GDDRAM mirror, so flush() can send it in one shot.
    uint8_t tx_buffer_[kFrameBytes + 1] = {0};
};

} // namespace oled_display
