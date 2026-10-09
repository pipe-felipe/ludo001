#pragma once

#include <cstdint>

#include "esp_err.h"

// Local status + file manager page served over esp_http_server (the
// ESP-IDF-native HTTP server, chosen over ESPAsyncWebServer to avoid
// pulling in Arduino-ESP32). This header only has the lifecycle API; the
// HTTP routing/handlers are web_server.cpp (the I/O adapter), the actual
// HTML/CSS/JS are static files under web/ (embedded via EMBED_TXTFILES),
// and the string-building logic they need is in the web_content
// component (web_server_util.hpp/web_server_render.hpp), kept dependency
// -free so it can be unit tested on the host -- see host_tests/.
namespace web_server {

esp_err_t start();
esp_err_t stop();
bool is_running();

struct UploadProgress {
    bool active = false;
    uint8_t percent = 0;
};

// Snapshot of the in-progress upload (if any), so other tasks -- e.g. the
// OLED display loop -- can show a progress bar without touching httpd
// internals.
UploadProgress upload_progress();

} // namespace web_server
