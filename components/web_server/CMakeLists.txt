idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_server wifi_manager sd_card
)
