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
void test_draw_char_every_letter_lights_some_pixels();
void test_draw_char_minus_is_middle_line();
void test_draw_char_percent_has_slash_and_two_dots();

// web_server_util
void test_url_encode_leaves_unreserved_untouched();
void test_url_encode_percent_encodes_space();
void test_url_encode_percent_encodes_multibyte_utf8();
void test_url_encode_keeps_long_name_whole();
void test_url_decode_plain_roundtrip();
void test_url_decode_percent_sequences();
void test_url_decode_invalid_percent_sequence_passed_through();
void test_url_decode_percent_with_one_digit_at_end_passed_through();
void test_url_encode_decode_roundtrip();
void test_is_safe_filename_accepts_plain_name();
void test_is_safe_filename_rejects_empty();
void test_is_safe_filename_rejects_leading_slash();
void test_is_safe_filename_rejects_dotdot();
void test_is_safe_filename_accepts_dot_in_middle();
void test_is_safe_filename_rejects_embedded_nul();
void test_format_size_bytes();
void test_format_size_kilobytes();
void test_format_size_megabytes();
void test_numbered_name_prefixes_plain_file_name();
void test_numbered_name_keeps_folder_path();
void test_numbered_name_keeps_dots_and_spaces();

// web_server_render
void test_render_wifi_line_connected();
void test_render_wifi_line_disconnected();
void test_render_sd_line_mounted();
void test_render_sd_line_not_mounted();
void test_render_file_list_empty();
void test_render_file_list_single_entry();
void test_render_file_list_encodes_name_with_space_in_href();
void test_render_file_list_multiple_entries_all_present();
void test_render_file_list_root_has_no_up_link();
void test_render_file_list_subdir_has_up_link_to_parent();
void test_render_file_list_top_level_subdir_up_link_goes_to_root();
void test_render_file_list_directory_entry_has_trailing_slash_and_no_size();
void test_render_file_list_nested_entry_includes_current_dir_in_paths();
void test_render_file_list_directory_entry_has_zip_link();
void test_render_file_list_nested_directory_zip_link_includes_current_dir();
void test_render_file_list_file_entry_has_download_link();
void test_render_file_list_nested_file_download_link_includes_current_dir();
void test_render_file_list_file_row_markup();
void test_render_file_list_directory_row_markup();
void test_render_file_list_keeps_long_accented_name_whole();
void test_render_dir_title_root();
void test_render_dir_title_subdir();
void test_render_template_substitutes_token();
void test_render_template_multiple_tokens();
void test_render_template_unknown_token_left_alone();

// web_server_zip
void test_crc32_of_nothing_is_zero();
void test_crc32_check_value();
void test_crc32_chunked_matches_one_shot();
void test_zip_local_header_file_layout();
void test_zip_local_header_directory_has_no_data_descriptor_flag();
void test_zip_data_descriptor_layout();
void test_zip_central_header_layout();
void test_zip_central_header_directory_attribute();
void test_zip_end_of_central_dir_layout();
void test_zip_within_limits_accepts_folder_under_4gb();
void test_zip_within_limits_rejects_archive_over_4gb();
void test_zip_within_limits_rejects_too_many_entries();

// dir_scan
void test_visible_entries_skips_dot_names();
void test_visible_entries_marks_directories();
void test_visible_entries_of_missing_dir_is_empty();
void test_count_entries_of_empty_dir_is_zero();
void test_count_entries_counts_files_and_folders_at_every_depth();
void test_count_entries_skips_hidden_folders_and_their_contents();

// web_server_firmware
void test_firmware_size_fits_up_to_partition_size();
void test_firmware_size_fits_rejects_empty_image();
void test_firmware_size_fits_rejects_image_larger_than_partition();
void test_firmware_head_matches_own_project();
void test_firmware_head_matches_rejects_other_project();
void test_firmware_head_matches_rejects_longer_or_shorter_name();
void test_firmware_head_matches_rejects_bad_image_magic();
void test_firmware_head_matches_rejects_bad_app_desc_magic();
void test_firmware_head_matches_rejects_short_head();

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
    RUN_TEST(test_draw_char_every_letter_lights_some_pixels);
    RUN_TEST(test_draw_char_minus_is_middle_line);
    RUN_TEST(test_draw_char_percent_has_slash_and_two_dots);

    RUN_TEST(test_url_encode_leaves_unreserved_untouched);
    RUN_TEST(test_url_encode_percent_encodes_space);
    RUN_TEST(test_url_encode_percent_encodes_multibyte_utf8);
    RUN_TEST(test_url_encode_keeps_long_name_whole);
    RUN_TEST(test_url_decode_plain_roundtrip);
    RUN_TEST(test_url_decode_percent_sequences);
    RUN_TEST(test_url_decode_invalid_percent_sequence_passed_through);
    RUN_TEST(test_url_decode_percent_with_one_digit_at_end_passed_through);
    RUN_TEST(test_url_encode_decode_roundtrip);
    RUN_TEST(test_is_safe_filename_accepts_plain_name);
    RUN_TEST(test_is_safe_filename_rejects_empty);
    RUN_TEST(test_is_safe_filename_rejects_leading_slash);
    RUN_TEST(test_is_safe_filename_rejects_dotdot);
    RUN_TEST(test_is_safe_filename_accepts_dot_in_middle);
    RUN_TEST(test_is_safe_filename_rejects_embedded_nul);
    RUN_TEST(test_format_size_bytes);
    RUN_TEST(test_format_size_kilobytes);
    RUN_TEST(test_format_size_megabytes);
    RUN_TEST(test_numbered_name_prefixes_plain_file_name);
    RUN_TEST(test_numbered_name_keeps_folder_path);
    RUN_TEST(test_numbered_name_keeps_dots_and_spaces);

    RUN_TEST(test_render_wifi_line_connected);
    RUN_TEST(test_render_wifi_line_disconnected);
    RUN_TEST(test_render_sd_line_mounted);
    RUN_TEST(test_render_sd_line_not_mounted);
    RUN_TEST(test_render_file_list_empty);
    RUN_TEST(test_render_file_list_single_entry);
    RUN_TEST(test_render_file_list_encodes_name_with_space_in_href);
    RUN_TEST(test_render_file_list_multiple_entries_all_present);
    RUN_TEST(test_render_file_list_root_has_no_up_link);
    RUN_TEST(test_render_file_list_subdir_has_up_link_to_parent);
    RUN_TEST(test_render_file_list_top_level_subdir_up_link_goes_to_root);
    RUN_TEST(test_render_file_list_directory_entry_has_trailing_slash_and_no_size);
    RUN_TEST(test_render_file_list_nested_entry_includes_current_dir_in_paths);
    RUN_TEST(test_render_file_list_directory_entry_has_zip_link);
    RUN_TEST(test_render_file_list_nested_directory_zip_link_includes_current_dir);
    RUN_TEST(test_render_file_list_file_entry_has_download_link);
    RUN_TEST(test_render_file_list_nested_file_download_link_includes_current_dir);
    RUN_TEST(test_render_file_list_file_row_markup);
    RUN_TEST(test_render_file_list_directory_row_markup);
    RUN_TEST(test_render_file_list_keeps_long_accented_name_whole);
    RUN_TEST(test_render_dir_title_root);
    RUN_TEST(test_render_dir_title_subdir);
    RUN_TEST(test_render_template_substitutes_token);
    RUN_TEST(test_render_template_multiple_tokens);
    RUN_TEST(test_render_template_unknown_token_left_alone);

    RUN_TEST(test_crc32_of_nothing_is_zero);
    RUN_TEST(test_crc32_check_value);
    RUN_TEST(test_crc32_chunked_matches_one_shot);
    RUN_TEST(test_zip_local_header_file_layout);
    RUN_TEST(test_zip_local_header_directory_has_no_data_descriptor_flag);
    RUN_TEST(test_zip_data_descriptor_layout);
    RUN_TEST(test_zip_central_header_layout);
    RUN_TEST(test_zip_central_header_directory_attribute);
    RUN_TEST(test_zip_end_of_central_dir_layout);
    RUN_TEST(test_zip_within_limits_accepts_folder_under_4gb);
    RUN_TEST(test_zip_within_limits_rejects_archive_over_4gb);
    RUN_TEST(test_zip_within_limits_rejects_too_many_entries);

    RUN_TEST(test_visible_entries_skips_dot_names);
    RUN_TEST(test_visible_entries_marks_directories);
    RUN_TEST(test_visible_entries_of_missing_dir_is_empty);
    RUN_TEST(test_count_entries_of_empty_dir_is_zero);
    RUN_TEST(test_count_entries_counts_files_and_folders_at_every_depth);
    RUN_TEST(test_count_entries_skips_hidden_folders_and_their_contents);

    RUN_TEST(test_firmware_size_fits_up_to_partition_size);
    RUN_TEST(test_firmware_size_fits_rejects_empty_image);
    RUN_TEST(test_firmware_size_fits_rejects_image_larger_than_partition);
    RUN_TEST(test_firmware_head_matches_own_project);
    RUN_TEST(test_firmware_head_matches_rejects_other_project);
    RUN_TEST(test_firmware_head_matches_rejects_longer_or_shorter_name);
    RUN_TEST(test_firmware_head_matches_rejects_bad_image_magic);
    RUN_TEST(test_firmware_head_matches_rejects_bad_app_desc_magic);
    RUN_TEST(test_firmware_head_matches_rejects_short_head);

    std::exit(UNITY_END());
}
