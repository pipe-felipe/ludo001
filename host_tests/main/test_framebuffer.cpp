#include "framebuffer.hpp"

extern "C" {
#include "unity.h"
}

using oled_display::Framebuffer;

namespace {

bool all_bytes_zero(const Framebuffer& fb) {
    for (size_t i = 0; i < Framebuffer::kByteCount; ++i) {
        if (fb.data()[i] != 0) {
            return false;
        }
    }
    return true;
}

} // namespace

void test_framebuffer_starts_clear() {
    Framebuffer fb;
    TEST_ASSERT_TRUE(all_bytes_zero(fb));
}

void test_set_pixel_and_get_pixel() {
    Framebuffer fb;
    fb.set_pixel(10, 20, true);
    TEST_ASSERT_TRUE(fb.get_pixel(10, 20));
    fb.set_pixel(10, 20, false);
    TEST_ASSERT_FALSE(fb.get_pixel(10, 20));
}

void test_set_pixel_out_of_bounds_is_noop() {
    Framebuffer fb;
    fb.set_pixel(-1, 0, true);
    fb.set_pixel(0, -1, true);
    fb.set_pixel(Framebuffer::kWidth, 0, true);
    fb.set_pixel(0, Framebuffer::kHeight, true);
    TEST_ASSERT_TRUE(all_bytes_zero(fb));
}

void test_get_pixel_out_of_bounds_is_false() {
    Framebuffer fb;
    fb.fill();
    TEST_ASSERT_FALSE(fb.get_pixel(-1, 0));
    TEST_ASSERT_FALSE(fb.get_pixel(Framebuffer::kWidth, 0));
}

void test_fill_sets_every_pixel() {
    Framebuffer fb;
    fb.fill();
    TEST_ASSERT_TRUE(fb.get_pixel(0, 0));
    TEST_ASSERT_TRUE(fb.get_pixel(Framebuffer::kWidth - 1, Framebuffer::kHeight - 1));
    for (size_t i = 0; i < Framebuffer::kByteCount; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0xFF, fb.data()[i]);
    }
}

void test_clear_after_fill() {
    Framebuffer fb;
    fb.fill();
    fb.clear();
    TEST_ASSERT_TRUE(all_bytes_zero(fb));
}

void test_draw_hline() {
    Framebuffer fb;
    fb.draw_hline(5, 10, 4);
    TEST_ASSERT_TRUE(fb.get_pixel(5, 10));
    TEST_ASSERT_TRUE(fb.get_pixel(6, 10));
    TEST_ASSERT_TRUE(fb.get_pixel(7, 10));
    TEST_ASSERT_TRUE(fb.get_pixel(8, 10));
    TEST_ASSERT_FALSE(fb.get_pixel(9, 10));
    TEST_ASSERT_FALSE(fb.get_pixel(4, 10));
}

void test_draw_vline() {
    Framebuffer fb;
    fb.draw_vline(5, 10, 3);
    TEST_ASSERT_TRUE(fb.get_pixel(5, 10));
    TEST_ASSERT_TRUE(fb.get_pixel(5, 11));
    TEST_ASSERT_TRUE(fb.get_pixel(5, 12));
    TEST_ASSERT_FALSE(fb.get_pixel(5, 13));
}

void test_draw_rect_is_outline_only() {
    Framebuffer fb;
    fb.draw_rect(2, 2, 5, 4); // spans x 2..6, y 2..5
    TEST_ASSERT_TRUE(fb.get_pixel(2, 2));
    TEST_ASSERT_TRUE(fb.get_pixel(6, 2));
    TEST_ASSERT_TRUE(fb.get_pixel(2, 5));
    TEST_ASSERT_TRUE(fb.get_pixel(6, 5));
    TEST_ASSERT_FALSE(fb.get_pixel(4, 3)); // interior stays off
}

void test_fill_rect_fills_interior() {
    Framebuffer fb;
    fb.fill_rect(2, 2, 5, 4);
    for (int y = 2; y < 6; ++y) {
        for (int x = 2; x < 7; ++x) {
            TEST_ASSERT_TRUE(fb.get_pixel(x, y));
        }
    }
    TEST_ASSERT_FALSE(fb.get_pixel(1, 2));
    TEST_ASSERT_FALSE(fb.get_pixel(7, 2));
}

void test_draw_line_diagonal() {
    Framebuffer fb;
    fb.draw_line(0, 0, 4, 4);
    for (int i = 0; i <= 4; ++i) {
        TEST_ASSERT_TRUE(fb.get_pixel(i, i));
    }
}

void test_draw_char_space_is_noop() {
    Framebuffer fb;
    fb.draw_char(0, 0, ' ');
    TEST_ASSERT_TRUE(all_bytes_zero(fb));
}

void test_draw_char_unsupported_is_noop() {
    Framebuffer fb;
    fb.draw_char(0, 0, '?');
    TEST_ASSERT_TRUE(all_bytes_zero(fb));
}

void test_draw_char_digit_lights_some_pixels() {
    Framebuffer fb;
    fb.draw_char(0, 0, '8'); // every 7-segment stroke
    bool any_on = false;
    for (int y = 0; y < Framebuffer::kGlyphHeight; ++y) {
        for (int x = 0; x < Framebuffer::kGlyphWidth; ++x) {
            if (fb.get_pixel(x, y)) {
                any_on = true;
            }
        }
    }
    TEST_ASSERT_TRUE(any_on);
}

void test_draw_char_lowercase_matches_uppercase() {
    Framebuffer fb_lower;
    Framebuffer fb_upper;
    fb_lower.draw_char(0, 0, 'a');
    fb_upper.draw_char(0, 0, 'A');
    for (size_t i = 0; i < Framebuffer::kByteCount; ++i) {
        TEST_ASSERT_EQUAL_UINT8(fb_upper.data()[i], fb_lower.data()[i]);
    }
}

void test_draw_text_matches_individual_draw_char_calls() {
    Framebuffer fb_chars;
    fb_chars.draw_char(0, 0, 'A');
    fb_chars.draw_char(Framebuffer::kGlyphAdvance, 0, 'A');

    Framebuffer fb_text;
    fb_text.draw_text(0, 0, "AA");

    for (size_t i = 0; i < Framebuffer::kByteCount; ++i) {
        TEST_ASSERT_EQUAL_UINT8(fb_chars.data()[i], fb_text.data()[i]);
    }
}

void test_draw_char_dot() {
    Framebuffer fb;
    fb.draw_char(0, 0, '.');
    TEST_ASSERT_TRUE(fb.get_pixel(1, Framebuffer::kGlyphHeight - 1));
}

void test_draw_char_colon_has_two_dots() {
    Framebuffer fb;
    fb.draw_char(0, 0, ':');
    TEST_ASSERT_TRUE(fb.get_pixel(2, 2));
    TEST_ASSERT_TRUE(fb.get_pixel(2, 5));
}

void test_draw_char_slash_is_diagonal() {
    Framebuffer fb;
    fb.draw_char(0, 0, '/');
    TEST_ASSERT_TRUE(fb.get_pixel(0, Framebuffer::kGlyphHeight - 1));
    TEST_ASSERT_TRUE(fb.get_pixel(Framebuffer::kGlyphWidth - 1, 0));
}

void test_draw_char_every_letter_lights_some_pixels() {
    for (char c = 'A'; c <= 'Z'; ++c) {
        Framebuffer fb;
        fb.draw_char(0, 0, c);
        const char letter[] = {c, '\0'};
        TEST_ASSERT_FALSE_MESSAGE(all_bytes_zero(fb), letter);
    }
}

void test_draw_char_minus_is_middle_line() {
    Framebuffer fb;
    fb.draw_char(0, 0, '-');
    TEST_ASSERT_TRUE(fb.get_pixel(0, 3));
    TEST_ASSERT_TRUE(fb.get_pixel(Framebuffer::kGlyphWidth - 1, 3));
    TEST_ASSERT_FALSE(fb.get_pixel(2, 0));
}

void test_draw_char_percent_has_slash_and_two_dots() {
    Framebuffer fb;
    fb.draw_char(0, 0, '%');
    TEST_ASSERT_TRUE(fb.get_pixel(2, 3)); // middle of the slash
    TEST_ASSERT_TRUE(fb.get_pixel(0, 0)); // top-left dot
    TEST_ASSERT_TRUE(fb.get_pixel(Framebuffer::kGlyphWidth - 1, Framebuffer::kGlyphHeight - 1)); // bottom-right dot
}
