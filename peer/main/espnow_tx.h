#pragma once

#include <stdint.h>
#include "esp_err.h"

// Bring up WiFi (STA, no AP connect) on the requested channel + ESP-NOW.
// `channel` must match the display's channel; if the display connects to an
// AP, both ends end up on the AP's channel — set this peer to the same.
esp_err_t espnow_tx_init(uint8_t channel);

// Broadcast `data` over ESP-NOW. Returns ESP_OK on success, ESP_FAIL otherwise.
esp_err_t espnow_tx_send(const void *data, size_t len);
