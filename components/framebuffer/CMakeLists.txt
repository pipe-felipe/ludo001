# Pure logic, no hardware/transport dependency -- builds for the real
# target (esp32) and for IDF_TARGET=linux (host_tests/), see
# framebuffer.hpp for why that split exists.
idf_component_register(
    SRCS "framebuffer.cpp"
    INCLUDE_DIRS "include"
)
