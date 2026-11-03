#include "framebuffer.hpp"

#include <cstdlib>
#include <cstring>

namespace oled_display {
namespace {

// Stroke font: each glyph is a short list of line segments inside a
// kGlyphWidth x kGlyphHeight cell (origin top-left), avoiding any copied
// bitmap font table. Digits reuse a classic 7-segment layout; letters are
// hand-drawn block strokes. Covers A-Z, 0-9 and the few symbols the status
// screen and firmware versions use; anything else is skipped by
// draw_char().
struct Segment {
    int8_t x0, y0, x1, y1;
};

constexpr Segment kSegTop = {0, 0, 4, 0};
constexpr Segment kSegTopLeft = {0, 0, 0, 3};
constexpr Segment kSegTopRight = {4, 0, 4, 3};
constexpr Segment kSegMiddle = {0, 3, 4, 3};
constexpr Segment kSegBottomLeft = {0, 3, 0, 6};
constexpr Segment kSegBottomRight = {4, 3, 4, 6};
constexpr Segment kSegBottom = {0, 6, 4, 6};

constexpr Segment kGlyph0[] = {kSegTop, kSegTopLeft, kSegTopRight, kSegBottomLeft, kSegBottomRight, kSegBottom};
constexpr Segment kGlyph1[] = {kSegTopRight, kSegBottomRight};
constexpr Segment kGlyph2[] = {kSegTop, kSegTopRight, kSegMiddle, kSegBottomLeft, kSegBottom};
constexpr Segment kGlyph3[] = {kSegTop, kSegTopRight, kSegMiddle, kSegBottomRight, kSegBottom};
constexpr Segment kGlyph4[] = {kSegTopLeft, kSegTopRight, kSegMiddle, kSegBottomRight};
constexpr Segment kGlyph5[] = {kSegTop, kSegTopLeft, kSegMiddle, kSegBottomRight, kSegBottom};
constexpr Segment kGlyph6[] = {kSegTop, kSegTopLeft, kSegMiddle, kSegBottomLeft, kSegBottomRight, kSegBottom};
constexpr Segment kGlyph7[] = {kSegTop, kSegTopRight, kSegBottomRight};
constexpr Segment kGlyph8[] = {kSegTop, kSegTopLeft, kSegTopRight, kSegMiddle, kSegBottomLeft, kSegBottomRight, kSegBottom};
constexpr Segment kGlyph9[] = {kSegTop, kSegTopLeft, kSegTopRight, kSegMiddle, kSegBottomRight, kSegBottom};

constexpr Segment kGlyphA[] = {{0, 6, 2, 0}, {2, 0, 4, 6}, {1, 3, 3, 3}};
constexpr Segment kGlyphB[] = {{0, 0, 0, 6}, {0, 0, 3, 0}, {3, 0, 4, 1}, {4, 1, 4, 2}, {4, 2, 3, 3}, {0, 3, 3, 3},
                                {3, 3, 4, 4}, {4, 4, 4, 5}, {4, 5, 3, 6}, {3, 6, 0, 6}};
constexpr Segment kGlyphC[] = {{3, 0, 0, 0}, {0, 0, 0, 6}, {0, 6, 3, 6}};
constexpr Segment kGlyphD[] = {{0, 0, 0, 6}, {0, 0, 2, 0}, {2, 0, 4, 2}, {4, 2, 4, 4}, {4, 4, 2, 6}, {2, 6, 0, 6}};
constexpr Segment kGlyphE[] = {{0, 0, 0, 6}, {0, 0, 4, 0}, {0, 3, 3, 3}, {0, 6, 4, 6}};
constexpr Segment kGlyphF[] = {{0, 0, 0, 6}, {0, 0, 4, 0}, {0, 3, 3, 3}};
constexpr Segment kGlyphG[] = {{4, 0, 0, 0}, {0, 0, 0, 6}, {0, 6, 4, 6}, {4, 6, 4, 3}, {4, 3, 2, 3}};
constexpr Segment kGlyphH[] = {{0, 0, 0, 6}, {4, 0, 4, 6}, {0, 3, 4, 3}};
constexpr Segment kGlyphI[] = {{0, 0, 4, 0}, {2, 0, 2, 6}, {0, 6, 4, 6}};
constexpr Segment kGlyphJ[] = {{4, 0, 4, 6}, {4, 6, 0, 6}, {0, 6, 0, 4}};
constexpr Segment kGlyphK[] = {{0, 0, 0, 6}, {4, 0, 0, 3}, {0, 3, 4, 6}};
constexpr Segment kGlyphL[] = {{0, 0, 0, 6}, {0, 6, 4, 6}};
constexpr Segment kGlyphM[] = {{0, 6, 0, 0}, {0, 0, 2, 3}, {2, 3, 4, 0}, {4, 0, 4, 6}};
constexpr Segment kGlyphN[] = {{0, 6, 0, 0}, {0, 0, 4, 6}, {4, 6, 4, 0}};
constexpr Segment kGlyphO[] = {{1, 0, 3, 0}, {3, 0, 4, 1}, {4, 1, 4, 5}, {4, 5, 3, 6},
                                {3, 6, 1, 6}, {1, 6, 0, 5}, {0, 5, 0, 1}, {0, 1, 1, 0}};
constexpr Segment kGlyphP[] = {{0, 6, 0, 0}, {0, 0, 3, 0}, {3, 0, 3, 3}, {3, 3, 0, 3}};
constexpr Segment kGlyphQ[] = {{1, 0, 3, 0}, {3, 0, 4, 1}, {4, 1, 4, 5}, {4, 5, 3, 6},
                                {3, 6, 1, 6}, {1, 6, 0, 5}, {0, 5, 0, 1}, {0, 1, 1, 0}, {2, 4, 4, 6}};
constexpr Segment kGlyphR[] = {{0, 6, 0, 0}, {0, 0, 3, 0}, {3, 0, 3, 3}, {3, 3, 0, 3}, {1, 3, 4, 6}};
constexpr Segment kGlyphS[] = {{4, 0, 0, 0}, {0, 0, 0, 3}, {0, 3, 4, 3}, {4, 3, 4, 6}, {4, 6, 0, 6}};
constexpr Segment kGlyphT[] = {{0, 0, 4, 0}, {2, 0, 2, 6}};
constexpr Segment kGlyphU[] = {{0, 0, 0, 6}, {0, 6, 4, 6}, {4, 6, 4, 0}};
constexpr Segment kGlyphV[] = {{0, 0, 2, 6}, {2, 6, 4, 0}};
constexpr Segment kGlyphW[] = {{0, 0, 1, 6}, {1, 6, 2, 3}, {2, 3, 3, 6}, {3, 6, 4, 0}};
constexpr Segment kGlyphX[] = {{0, 0, 4, 6}, {4, 0, 0, 6}};
constexpr Segment kGlyphY[] = {{0, 0, 2, 3}, {4, 0, 2, 3}, {2, 3, 2, 6}};
constexpr Segment kGlyphZ[] = {{0, 0, 4, 0}, {4, 0, 0, 6}, {0, 6, 4, 6}};

constexpr Segment kGlyphMinus[] = {kSegMiddle};
constexpr Segment kGlyphPercent[] = {{0, 6, 4, 0}, {0, 0, 1, 0}, {0, 1, 1, 1}, {3, 5, 4, 5}, {3, 6, 4, 6}};

template <size_t N>
void select_glyph(const Segment*& segs, size_t& count, const Segment (&arr)[N]) {
    segs = arr;
    count = N;
}

} // namespace

void Framebuffer::clear() {
    std::memset(bytes_, 0x00, kByteCount);
}

void Framebuffer::fill() {
    std::memset(bytes_, 0xFF, kByteCount);
}

void Framebuffer::set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    size_t byte_index = static_cast<size_t>(x) + (y / 8) * kWidth;
    uint8_t mask = static_cast<uint8_t>(1u << (y % 8));
    if (on) {
        bytes_[byte_index] |= mask;
    } else {
        bytes_[byte_index] &= static_cast<uint8_t>(~mask);
    }
}

bool Framebuffer::get_pixel(int x, int y) const {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return false;
    }
    size_t byte_index = static_cast<size_t>(x) + (y / 8) * kWidth;
    uint8_t mask = static_cast<uint8_t>(1u << (y % 8));
    return (bytes_[byte_index] & mask) != 0;
}

void Framebuffer::draw_hline(int x, int y, int length, bool on) {
    for (int i = 0; i < length; ++i) {
        set_pixel(x + i, y, on);
    }
}

void Framebuffer::draw_vline(int x, int y, int length, bool on) {
    for (int i = 0; i < length; ++i) {
        set_pixel(x, y + i, on);
    }
}

void Framebuffer::draw_line(int x0, int y0, int x1, int y1, bool on) {
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

void Framebuffer::draw_rect(int x, int y, int width, int height, bool on) {
    draw_hline(x, y, width, on);
    draw_hline(x, y + height - 1, width, on);
    draw_vline(x, y, height, on);
    draw_vline(x + width - 1, y, height, on);
}

void Framebuffer::fill_rect(int x, int y, int width, int height, bool on) {
    for (int row = 0; row < height; ++row) {
        draw_hline(x, y + row, width, on);
    }
}

void Framebuffer::draw_char(int x, int y, char c, bool on) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - ('a' - 'A'));
    }

    const Segment* segs = nullptr;
    size_t count = 0;

    switch (c) {
        case '0': select_glyph(segs, count, kGlyph0); break;
        case '1': select_glyph(segs, count, kGlyph1); break;
        case '2': select_glyph(segs, count, kGlyph2); break;
        case '3': select_glyph(segs, count, kGlyph3); break;
        case '4': select_glyph(segs, count, kGlyph4); break;
        case '5': select_glyph(segs, count, kGlyph5); break;
        case '6': select_glyph(segs, count, kGlyph6); break;
        case '7': select_glyph(segs, count, kGlyph7); break;
        case '8': select_glyph(segs, count, kGlyph8); break;
        case '9': select_glyph(segs, count, kGlyph9); break;
        case 'A': select_glyph(segs, count, kGlyphA); break;
        case 'B': select_glyph(segs, count, kGlyphB); break;
        case 'C': select_glyph(segs, count, kGlyphC); break;
        case 'D': select_glyph(segs, count, kGlyphD); break;
        case 'E': select_glyph(segs, count, kGlyphE); break;
        case 'F': select_glyph(segs, count, kGlyphF); break;
        case 'G': select_glyph(segs, count, kGlyphG); break;
        case 'H': select_glyph(segs, count, kGlyphH); break;
        case 'I': select_glyph(segs, count, kGlyphI); break;
        case 'J': select_glyph(segs, count, kGlyphJ); break;
        case 'K': select_glyph(segs, count, kGlyphK); break;
        case 'L': select_glyph(segs, count, kGlyphL); break;
        case 'M': select_glyph(segs, count, kGlyphM); break;
        case 'N': select_glyph(segs, count, kGlyphN); break;
        case 'O': select_glyph(segs, count, kGlyphO); break;
        case 'P': select_glyph(segs, count, kGlyphP); break;
        case 'Q': select_glyph(segs, count, kGlyphQ); break;
        case 'R': select_glyph(segs, count, kGlyphR); break;
        case 'S': select_glyph(segs, count, kGlyphS); break;
        case 'T': select_glyph(segs, count, kGlyphT); break;
        case 'U': select_glyph(segs, count, kGlyphU); break;
        case 'V': select_glyph(segs, count, kGlyphV); break;
        case 'W': select_glyph(segs, count, kGlyphW); break;
        case 'X': select_glyph(segs, count, kGlyphX); break;
        case 'Y': select_glyph(segs, count, kGlyphY); break;
        case 'Z': select_glyph(segs, count, kGlyphZ); break;
        case '-': select_glyph(segs, count, kGlyphMinus); break;
        case '%': select_glyph(segs, count, kGlyphPercent); break;
        case '.':
            set_pixel(x + 1, y + kGlyphHeight - 1, on);
            return;
        case '/':
            draw_line(x, y + kGlyphHeight - 1, x + kGlyphWidth - 1, y, on);
            return;
        case ':':
            set_pixel(x + 2, y + 2, on);
            set_pixel(x + 2, y + 5, on);
            return;
        default:
            return; // space or unsupported character: just advance
    }

    for (size_t i = 0; i < count; ++i) {
        draw_line(x + segs[i].x0, y + segs[i].y0, x + segs[i].x1, y + segs[i].y1, on);
    }
}

void Framebuffer::draw_text(int x, int y, const char* text, bool on) {
    for (; *text != '\0'; ++text) {
        draw_char(x, y, *text, on);
        x += kGlyphAdvance;
    }
}

} // namespace oled_display
