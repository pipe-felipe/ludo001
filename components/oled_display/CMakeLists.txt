idf_component_register(
    SRCS "oled_display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c framebuffer
)
