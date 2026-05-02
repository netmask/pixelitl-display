#include "wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>

// OFFLINE_ONLY: skip the AP connect handshake. WiFi radio is still started
// (so ESP-NOW works), and pinned to channel 1 to match the peer. Use this
// when the AP is rejecting auth and you just want the radar face to work.
// Set to 0 to restore normal behavior (HN, glucose, dota proxy, etc.).
#define OFFLINE_ONLY 0
#define OFFLINE_CHANNEL 1

static const char *TAG = "wifi";

static volatile bool s_connected = false;
static TimerHandle_t s_reconnect_timer;
static uint32_t s_backoff_ms = 1000;
#define MAX_BACKOFF_MS 30000

static void reconnect_cb(TimerHandle_t timer) {
    if (!s_connected) {
        ESP_LOGI(TAG, "Reconnecting (backoff %lu ms)...", (unsigned long)s_backoff_ms);
        esp_wifi_connect();
    }
}

static void schedule_reconnect(void) {
    if (s_backoff_ms > MAX_BACKOFF_MS) s_backoff_ms = MAX_BACKOFF_MS;
    xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(s_backoff_ms), 0);
    xTimerStart(s_reconnect_timer, 0);
    s_backoff_ms *= 2;
}

static void event_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *data) {
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
#if OFFLINE_ONLY
            // Don't try to associate. Pin the radio so ESP-NOW is reliable.
            esp_wifi_set_channel(OFFLINE_CHANNEL, WIFI_SECOND_CHAN_NONE);
            ESP_LOGI(TAG, "OFFLINE_ONLY: skipping AP connect, pinned ch=%d",
                     OFFLINE_CHANNEL);
#else
            esp_wifi_connect();
#endif
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            ESP_LOGW(TAG, "Disconnected");
#if !OFFLINE_ONLY
            schedule_reconnect();
#endif
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected, IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        s_connected = true;
        s_backoff_ms = 1000;
        // Re-apply PS_NONE on every association — without this, the IDF can
        // re-enable DTIM-based sleep on reconnect, which kills ESP-NOW recv.
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
}

esp_err_t wifi_init(void) {
    s_reconnect_timer = xTimerCreate("wifi_re", pdMS_TO_TICKS(1000),
                                      pdFALSE, NULL, reconnect_cb);

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &event_handler, NULL, NULL),
        TAG, "wifi event reg");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            &event_handler, NULL, NULL),
        TAG, "ip event reg");

    // Read credentials: NVS first, fall back to Kconfig
    char ssid[33] = {0};
    char pass[65] = {0};

    nvs_handle_t nvs;
    if (nvs_open("wifi_cred", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(ssid);
        if (nvs_get_str(nvs, "ssid", ssid, &len) != ESP_OK) ssid[0] = '\0';
        len = sizeof(pass);
        if (nvs_get_str(nvs, "pass", pass, &len) != ESP_OK) pass[0] = '\0';
        nvs_close(nvs);
    }

    if (ssid[0] == '\0') {
        strncpy(ssid, CONFIG_PIXELITL_WIFI_SSID, sizeof(ssid) - 1);
        strncpy(pass, CONFIG_PIXELITL_WIFI_PASSWORD, sizeof(pass) - 1);
        ESP_LOGI(TAG, "Using Kconfig WiFi creds (SSID: %s)", ssid);
    } else {
        ESP_LOGI(TAG, "Using NVS WiFi creds (SSID: %s)", ssid);
    }

    wifi_config_t wifi_cfg = {0};
    memcpy(wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    memcpy(wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set STA mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg), TAG, "set config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");

    // Disable WiFi power-save: STA otherwise sleeps for ~300 ms DTIM windows
    // (listen_interval = 3 × beacon_interval ≈ 307 ms), and any ESP-NOW packet
    // arriving during a sleep window is missed. With lazy face loading there's
    // no longer a RAM-fragmentation issue around this call, so PS_NONE is safe.
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "WiFi STA started, connecting to '%s'", ssid);
    return ESP_OK;
}

bool wifi_is_connected(void) {
    return s_connected;
}
