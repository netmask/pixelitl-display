#include "espnow_tx.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "espnow_tx";

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void on_send(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    // Quiet by default — uncomment when debugging dropped packets.
    // ESP_LOGI(TAG, "send -> %s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

static esp_err_t wifi_init_sta_no_connect(uint8_t channel) {
    // NVS is required by esp_wifi_init.
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Pin the radio to the requested channel — required because we don't
    // associate to an AP, so it would otherwise sit on channel 1 by default.
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    uint8_t pri = 0;
    wifi_second_chan_t sec;
    if (esp_wifi_get_channel(&pri, &sec) == ESP_OK) {
        ESP_LOGI(TAG, "wifi STA up on channel %u", (unsigned)pri);
    }
    return ESP_OK;
}

esp_err_t espnow_tx_init(uint8_t channel) {
    ESP_ERROR_CHECK(wifi_init_sta_no_connect(channel));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_send));

    esp_now_peer_info_t peer = {
        .channel = channel,
        .ifidx   = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, BCAST, 6);
    if (esp_now_add_peer(&peer) != ESP_OK) {
        ESP_LOGW(TAG, "add broadcast peer failed (already present?)");
    }

    ESP_LOGI(TAG, "esp-now ready, broadcasting on channel %u", (unsigned)channel);
    return ESP_OK;
}

esp_err_t espnow_tx_send(const void *data, size_t len) {
    return esp_now_send(BCAST, (const uint8_t *)data, len);
}
