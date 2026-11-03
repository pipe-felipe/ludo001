idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_server wifi_manager sd_card web_content dir_scan app_update esp_app_format esp_timer
    EMBED_TXTFILES "web/index.html" "web/files.html" "web/style.css" "web/app.js"
)
