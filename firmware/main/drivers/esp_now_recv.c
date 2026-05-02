#include "esp_now_recv.h"

#include <string.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "espnow";

#define MAX_PKT_LEN 250

static SemaphoreHandle_t s_lock;
static uint8_t  s_buf[MAX_PKT_LEN];
static int      s_buf_len = -1;
static int64_t  s_last_rx_us = 0;
static uint32_t s_seq = 0;
static uint32_t s_last_read_seq = 0;

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) != pdTRUE) return;
    int copy = (len > MAX_PKT_LEN) ? MAX_PKT_LEN : len;
    memcpy(s_buf, data, copy);
    s_buf_len = len;
    s_last_rx_us = esp_timer_get_time();
    s_seq++;

    // Diagnostic: log every 30th packet (≈3 s at 10 Hz peer cadence) with the
    // sender MAC and the first 8 bytes so we can see if magic survives.
    if (s_seq <= 5 || (s_seq % 30) == 0) {
        const uint8_t *m = info->src_addr;
        const uint8_t *d = data;
        ESP_LOGI(TAG,
                 "rx#%lu len=%d from %02x:%02x:%02x:%02x:%02x:%02x"
                 " bytes=%02x %02x %02x %02x %02x %02x %02x %02x",
                 (unsigned long)s_seq, len,
                 m[0], m[1], m[2], m[3], m[4], m[5],
                 d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
    }

    xSemaphoreGive(s_lock);
}

esp_err_t pxl_espnow_init(void) {
    s_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

    // Receive-only: no peer registration needed. The recv callback fires for
    // any incoming ESP-NOW packet on the current STA channel regardless of
    // peer table contents. Skipping esp_now_add_peer here avoids racing the
    // WiFi association handshake — adding a peer with .channel=0 while STA
    // is still authing has been observed to break the auth on some setups.

    ESP_LOGI(TAG, "ready (recv-only)");
    return ESP_OK;
}

int pxl_espnow_get_latest(uint8_t *dst, int max_len) {
    int ret = -1;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(5)) != pdTRUE) return -1;
    if (s_buf_len >= 0 && max_len > 0) {
        int copy = (s_buf_len > max_len) ? max_len : s_buf_len;
        memcpy(dst, s_buf, copy);
        ret = s_buf_len;
        s_last_read_seq = s_seq;
    }
    xSemaphoreGive(s_lock);
    return ret;
}

int pxl_espnow_has_new(void) {
    int ret = 0;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(5)) != pdTRUE) return 0;
    ret = (s_seq != s_last_read_seq) ? 1 : 0;
    xSemaphoreGive(s_lock);
    return ret;
}

int64_t pxl_espnow_last_rx_ms(void) {
    return s_last_rx_us / 1000;
}

uint32_t pxl_espnow_total_rx(void) {
    return s_seq;
}

int pxl_espnow_pkt_byte(int idx) {
    int ret = -1;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(5)) != pdTRUE) return -1;
    if (s_buf_len > 0 && idx >= 0 && idx < s_buf_len) {
        ret = (int)s_buf[idx];
    }
    xSemaphoreGive(s_lock);
    return ret;
}

int pxl_espnow_pkt_len(void) {
    int ret = -1;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(5)) != pdTRUE) return -1;
    ret = s_buf_len;
    xSemaphoreGive(s_lock);
    return ret;
}

void pxl_espnow_consume(void) {
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(5)) != pdTRUE) return;
    s_last_read_seq = s_seq;
    xSemaphoreGive(s_lock);
}
