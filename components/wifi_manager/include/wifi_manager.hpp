#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

// Station-mode Wi-Fi access. Call connect() once at startup; it brings up
// NVS/netif/event loop/driver on first use and blocks until an IP is
// obtained or the timeout elapses. After a successful connect, the
// driver auto-reconnects on link loss until disconnect() is called.
namespace wifi_manager {

struct StaConfig {
    const char* ssid;
    const char* password;
    uint32_t connect_timeout_ms = 10000;
};

esp_err_t connect(const StaConfig& config);
esp_err_t disconnect();
bool is_connected();

// Writes the current station IP as a dotted string into `out`
// (recommend at least 16 bytes). ESP_ERR_INVALID_STATE if not connected.
esp_err_t get_ip(char* out, size_t out_len);

} // namespace wifi_manager
