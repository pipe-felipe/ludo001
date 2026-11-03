# Pure logic, no hardware/network dependency -- builds for the real target
# (esp32) and for IDF_TARGET=linux (host_tests/). web_server links this to
# avoid duplicating the string-building logic in its httpd handlers.
# esp_app_format is only here for esp_app_desc.h's struct and magic word.
idf_component_register(
    SRCS "web_server_util.cpp" "web_server_render.cpp" "web_server_zip.cpp" "web_server_firmware.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_app_format
)
