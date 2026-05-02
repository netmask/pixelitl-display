// pixelitl peer — ESP32 (WROOM) hello world.
//
// Prints chip info, the WiFi STA + SoftAP MAC addresses (those are the ones
// ESP-NOW uses for peering), and a heartbeat every 5 s.
//
// The MAC lines look like:
//   I (xxx) peer: WIFI_STA MAC: AA:BB:CC:DD:EE:FF
// Copy the STA MAC of each peer into the other peer's esp_now_peer_info_t.

#include <inttypes.h>
#include <stdio.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ld2450.h"
#include "espnow_tx.h"

// LD2450 wiring (UART2 on the WROOM dev board)
#define LD2450_UART      UART_NUM_2
#define LD2450_RX_GPIO   16   // ESP32 GPIO16 ⇐ LD2450 TX
#define LD2450_TX_GPIO   17   // ESP32 GPIO17 ⇒ LD2450 RX

// ESP-NOW channel — must match the display's WiFi channel.
// The display logs its channel on boot ("ready, station channel = N"). If
// they don't match, change this to the display's channel and reflash.
#define ESPNOW_CHANNEL   1

// Radar broadcast packet — 32 bytes total. Magic byte distinguishes this
// from any future ESP-NOW packet types we add.
#define RADAR_MAGIC   0xA4
#define RADAR_VERSION 1

typedef struct __attribute__((packed)) {
    uint8_t  magic;          // 0xA4
    uint8_t  version;        // 1
    uint8_t  flags;          // bit0..2: target presence
    uint8_t  reserved;
    int16_t  x_mm[3];        // signed cartesian
    int16_t  y_mm[3];
    int16_t  speed_cms[3];   // signed
    uint16_t res_mm[3];
    uint32_t frame_count;    // monotonic, taken from radar
} radar_pkt_t;

_Static_assert(sizeof(radar_pkt_t) == 32, "radar_pkt_t must be 32 bytes");

static const char *TAG = "peer";

static void log_mac(const char *label, esp_mac_type_t type) {
    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%s MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        ESP_LOGW(TAG, "%s MAC: <unavailable: %s>", label, esp_err_to_name(err));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "----- pixelitl peer (ESP32 WROOM) booting -----");

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "chip: %s, %d core(s), rev %d, features:%s%s%s",
             CONFIG_IDF_TARGET,
             chip.cores,
             chip.revision,
             (chip.features & CHIP_FEATURE_WIFI_BGN) ? " WiFi" : "",
             (chip.features & CHIP_FEATURE_BT)       ? " BT"   : "",
             (chip.features & CHIP_FEATURE_BLE)      ? " BLE"  : "");

    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "flash: %" PRIu32 " MB", flash_size / (1024U * 1024U));
    }

    ESP_LOGI(TAG, "free heap: %" PRIu32 " bytes", esp_get_free_heap_size());

    log_mac("WIFI_STA", ESP_MAC_WIFI_STA);
    log_mac("WIFI_AP ", ESP_MAC_WIFI_SOFTAP);
    log_mac("BASE    ", ESP_MAC_BASE);

    // Bring up LD2450 reader on UART2 + ESP-NOW broadcast.
    ld2450_init(LD2450_UART, LD2450_RX_GPIO, LD2450_TX_GPIO);
    ESP_ERROR_CHECK(espnow_tx_init(ESPNOW_CHANNEL));

    ESP_LOGI(TAG, "ready. broadcasting radar @10Hz, heartbeat every 5s.");

    int n = 0;
    int hb_ms_accum = 0;

    // Constant 10 Hz broadcast — every packet, every frame, regardless of
    // whether the radar has anything to report.
    while (true) {
        const ld2450_state_t *st = ld2450_get_state();

        radar_pkt_t pkt = {
            .magic = RADAR_MAGIC,
            .version = RADAR_VERSION,
            .flags = 0,
            .reserved = 0,
            .frame_count = st->frame_count,
        };
        for (int i = 0; i < 3; i++) {
            if (st->targets[i].present) pkt.flags |= (uint8_t)(1u << i);
            pkt.x_mm[i]      = st->targets[i].x_mm;
            pkt.y_mm[i]      = st->targets[i].y_mm;
            pkt.speed_cms[i] = st->targets[i].speed_cms;
            pkt.res_mm[i]    = st->targets[i].res_mm;
        }
        espnow_tx_send(&pkt, sizeof(pkt));

        vTaskDelay(pdMS_TO_TICKS(100));

        // Heartbeat log every 5 s — same data plus radar diagnostics.
        hb_ms_accum += 100;
        if (hb_ms_accum >= 5000) {
            hb_ms_accum = 0;
            int64_t uptime_s = esp_timer_get_time() / 1000000;
            int present = 0;
            for (int i = 0; i < 3; i++) if (st->targets[i].present) present++;
            ESP_LOGI(TAG,
                     "heartbeat #%d  uptime=%lld s  free=%" PRIu32 " B  "
                     "radar: frames=%lu bad=%lu present=%d",
                     n++, (long long)uptime_s, esp_get_free_heap_size(),
                     (unsigned long)st->frame_count,
                     (unsigned long)st->bad_frames,
                     present);
        }
    }
}
