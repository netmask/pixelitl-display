#pragma once

#include <stdint.h>
#include "esp_err.h"

// Initialize ESP-NOW receive. Call after WiFi has been started in STA mode.
esp_err_t pxl_espnow_init(void);

// Copy the most recent received packet into `dst`. Returns the packet's full
// byte length (clipped by max_len for the copy), or -1 if no packet has ever
// been received.
int pxl_espnow_get_latest(uint8_t *dst, int max_len);

// 1 if a packet has arrived since the last call to pxl_espnow_get_latest, else 0.
int pxl_espnow_has_new(void);

// Wall-clock millis when the last packet arrived (esp_timer-based). 0 if never.
int64_t pxl_espnow_last_rx_ms(void);

// Total packets received since boot. Useful as a diagnostic for "are packets
// still arriving" independent of any face's polling state.
uint32_t pxl_espnow_total_rx(void);

// Read a single byte from the last received packet at index `idx`.
// Returns -1 if no packet, or idx out of range. Lets faces read packet data
// without going through the array-passing path that has wasm3 quirks.
int pxl_espnow_pkt_byte(int idx);

// Length of the last received packet, or -1 if none.
int pxl_espnow_pkt_len(void);

// Mark current packet as "consumed" (so espnow_has_new returns 0 until next).
void pxl_espnow_consume(void);
