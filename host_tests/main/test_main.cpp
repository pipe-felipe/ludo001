#include <cstdlib>

extern "C" {
#include "unity.h"
}

// framebuffer
void test_framebuffer_starts_clear();
void test_set_pixel_and_get_pixel();
void test_set_pixel_out_of_bounds_is_noop();
void test_get_pixel_out_of_bounds_is_false();
void test_fill_sets_every_pixel();
void test_clear_after_fill();
void test_draw_hline();
void test_draw_vline();
void test_draw_rect_is_outline_only();
void test_fill_rect_fills_interior();
void test_draw_line_diagonal();
void test_draw_char_space_is_noop();
void test_draw_char_unsupported_is_noop();
void test_draw_char_digit_lights_some_pixels();
void test_draw_char_lowercase_matches_uppercase();
void test_draw_text_matches_individual_draw_char_calls();
void test_draw_char_dot();
void test_draw_char_colon_has_two_dots();
void test_draw_char_slash_is_diagonal();

// web_server_util
void test_url_encode_leaves_unreserved_untouched();
void test_url_encode_percent_encodes_space();
void test_url_encode_percent_encodes_multibyte_utf8();
void test_url_encode_truncates_safely_when_buffer_small();
void test_url_decode_plain_roundtrip();
void test_url_decode_percent_sequences();
void test_url_decode_invalid_percent_sequence_passed_through();
void test_url_encode_decode_roundtrip();
void test_is_safe_filename_accepts_plain_name();
void test_is_safe_filename_rejects_empty();
void test_is_safe_filename_rejects_leading_slash();
void test_is_safe_filename_rejects_dotdot();
void test_is_safe_filename_accepts_dot_in_middle();
void test_format_size_bytes();
void test_format_size_kilobytes();
void test_format_size_megabytes();

// web_server_render
void test_render_wifi_line_connected();
void test_render_wifi_line_disconnected();
void test_render_sd_line_mounted();
void test_render_sd_line_not_mounted();
void test_render_file_list_empty();
void test_render_file_list_single_entry();
void test_render_file_list_encodes_name_with_space_in_href();
void test_render_file_list_multiple_entries_all_present();
void test_render_template_substitutes_token();
void test_render_template_multiple_tokens();
void test_render_template_unknown_token_left_alone();

extern "C" void app_main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_framebuffer_starts_clear);
    RUN_TEST(test_set_pixel_and_get_pixel);
    RUN_TEST(test_set_pixel_out_of_bounds_is_noop);
    RUN_TEST(test_get_pixel_out_of_bounds_is_false);
    RUN_TEST(test_fill_sets_every_pixel);
    RUN_TEST(test_clear_after_fill);
    RUN_TEST(test_draw_hline);
    RUN_TEST(test_draw_vline);
    RUN_TEST(test_draw_rect_is_outline_only);
    RUN_TEST(test_fill_rect_fills_interior);
    RUN_TEST(test_draw_line_diagonal);
    RUN_TEST(test_draw_char_space_is_noop);
    RUN_TEST(test_draw_char_unsupported_is_noop);
    RUN_TEST(test_draw_char_digit_lights_some_pixels);
    RUN_TEST(test_draw_char_lowercase_matches_uppercase);
    RUN_TEST(test_draw_text_matches_individual_draw_char_calls);
    RUN_TEST(test_draw_char_dot);
    RUN_TEST(test_draw_char_colon_has_two_dots);
    RUN_TEST(test_draw_char_slash_is_diagonal);

    RUN_TEST(test_url_encode_leaves_unreserved_untouched);
    RUN_TEST(test_url_encode_percent_encodes_space);
    RUN_TEST(test_url_encode_percent_encodes_multibyte_utf8);
    RUN_TEST(test_url_encode_truncates_safely_when_buffer_small);
    RUN_TEST(test_url_decode_plain_roundtrip);
    RUN_TEST(test_url_decode_percent_sequences);
    RUN_TEST(test_url_decode_invalid_percent_sequence_passed_through);
    RUN_TEST(test_url_encode_decode_roundtrip);
    RUN_TEST(test_is_safe_filename_accepts_plain_name);
    RUN_TEST(test_is_safe_filename_rejects_empty);
    RUN_TEST(test_is_safe_filename_rejects_leading_slash);
    RUN_TEST(test_is_safe_filename_rejects_dotdot);
    RUN_TEST(test_is_safe_filename_accepts_dot_in_middle);
    RUN_TEST(test_format_size_bytes);
    RUN_TEST(test_format_size_kilobytes);
    RUN_TEST(test_format_size_megabytes);

    RUN_TEST(test_render_wifi_line_connected);
    RUN_TEST(test_render_wifi_line_disconnected);
    RUN_TEST(test_render_sd_line_mounted);
    RUN_TEST(test_render_sd_line_not_mounted);
    RUN_TEST(test_render_file_list_empty);
    RUN_TEST(test_render_file_list_single_entry);
    RUN_TEST(test_render_file_list_encodes_name_with_space_in_href);
    RUN_TEST(test_render_file_list_multiple_entries_all_present);
    RUN_TEST(test_render_template_substitutes_token);
    RUN_TEST(test_render_template_multiple_tokens);
    RUN_TEST(test_render_template_unknown_token_left_alone);

    std::exit(UNITY_END());
}
