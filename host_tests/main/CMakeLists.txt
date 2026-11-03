# Unit tests for the components with no hardware/network dependency
# (framebuffer, web_content, dir_scan). Runs natively on this machine
# (IDF_TARGET=linux), not on the ESP32 -- see host_tests/README.md.
idf_component_register(
    SRCS "test_main.cpp" "test_framebuffer.cpp" "test_web_server_util.cpp" "test_web_server_render.cpp" "test_web_server_zip.cpp" "test_dir_scan.cpp" "test_web_server_firmware.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity framebuffer web_content dir_scan
)
