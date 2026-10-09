#pragma once

#include "esp_err.h"

// Local status page served over esp_http_server (the ESP-IDF-native HTTP
// server, chosen over ESPAsyncWebServer to avoid pulling in Arduino-ESP32 --
// see reports/Servidor web leve ESP32.md for the comparison). A single GET
// "/" handler reports live Wi-Fi and SD card status as HTML; it reads
// wifi_manager/sd_card state directly rather than caching it.
namespace web_server {

esp_err_t start();
esp_err_t stop();
bool is_running();

} // namespace web_server
