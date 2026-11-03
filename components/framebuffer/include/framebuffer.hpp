#pragma once

#include <cstddef>
#include <cstdint>

// Pure pixel/text drawing logic for a 128x64 1-bit display, with no
// dependency on any transport (I2C, SPI, ...) or hardware headers. This is
// what makes it buildable and testable on the host (IDF_TARGET=linux),
// separate from oled_display's Ssd1306, which only adds the I2C transport
// on top of this buffer.
namespace oled_display {

class Framebuffer {
public:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;
    static constexpr size_t kByteCount = kWidth * kHeight / 8;

    void clear();
    void fill();
    void set_pixel(int x, int y, bool on);
    bool get_pixel(int x, int y) const;
    void draw_hline(int x, int y, int length, bool on = true);
    void draw_vline(int x, int y, int length, bool on = true);
    void draw_rect(int x, int y, int width, int height, bool on = true);
    void fill_rect(int x, int y, int width, int height, bool on = true);
    void draw_line(int x0, int y0, int x1, int y1, bool on = true);

    // Stroke-font text. Supports 'A'-'Z'/'a'-'z' (rendered as uppercase),
    // '0'-'9', ' ', '.', '/', ':', '-' and '%'; unsupported characters are
    // skipped (cursor still advances). Each glyph is kGlyphWidth x
    // kGlyphHeight, advancing by kGlyphAdvance per character.
    static constexpr int kGlyphWidth = 5;
    static constexpr int kGlyphHeight = 7;
    static constexpr int kGlyphAdvance = 6;

    void draw_char(int x, int y, char c, bool on = true);
    void draw_text(int x, int y, const char* text, bool on = true);

    const uint8_t* data() const { return bytes_; }
    static constexpr size_t size() { return kByteCount; }

private:
    uint8_t bytes_[kByteCount] = {0};
};

} // namespace oled_display
