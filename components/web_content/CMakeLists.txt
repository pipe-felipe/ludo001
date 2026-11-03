# Pure logic, no hardware/network dependency -- builds for the real target
# (esp32) and for IDF_TARGET=linux (host_tests/). web_server links this to
# avoid duplicating the string-building logic in its httpd handlers.
idf_component_register(
    SRCS "web_server_util.cpp" "web_server_render.cpp" "web_server_zip.cpp" "web_server_firmware.cpp"
    INCLUDE_DIRS "include"
)
