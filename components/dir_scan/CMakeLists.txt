# Pure POSIX directory walking, no hardware dependency -- builds for the
# real target (esp32) and for IDF_TARGET=linux (host_tests/). sd_card and
# web_server link this instead of each repeating the same readdir loop.
idf_component_register(
    SRCS "dir_scan.cpp"
    INCLUDE_DIRS "include"
)
