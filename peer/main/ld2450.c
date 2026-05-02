#include "ld2450.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ld2450";

// Frame layout (default target-tracking mode):
//   header  AA FF 03 00          4 bytes
//   target0 x(2) y(2) v(2) res(2) 8 bytes  ── repeated 3×
//   target1 ...                   8 bytes
//   target2 ...                   8 bytes
//   footer  55 CC                 2 bytes
//   ────────────────────────────  30 bytes total, ~10 Hz
static const uint8_t HEADER[4] = {0xAA, 0xFF, 0x03, 0x00};
#define BODY_LEN  26   // 24 bytes target data + 2 bytes footer

static uart_port_t s_uart;
static ld2450_state_t s_state;

// LD2450 sign-magnitude: bit 15 set ⇒ value is positive (raw - 0x8000).
// bit 15 clear ⇒ value is negative-of-raw.
static int16_t decode_signed(uint16_t raw) {
    if (raw & 0x8000) return (int16_t)(raw - 0x8000);
    return (int16_t)(-(int32_t)raw);
}

static void parse_frame(const uint8_t *body) {
    s_state.frame_count++;
    for (int i = 0; i < 3; i++) {
        const uint8_t *t = body + i * 8;
        uint16_t xr = (uint16_t)(t[0] | (t[1] << 8));
        uint16_t yr = (uint16_t)(t[2] | (t[3] << 8));
        uint16_t vr = (uint16_t)(t[4] | (t[5] << 8));
        uint16_t rr = (uint16_t)(t[6] | (t[7] << 8));

        s_state.targets[i].x_mm     = decode_signed(xr);
        s_state.targets[i].y_mm     = decode_signed(yr);
        s_state.targets[i].speed_cms = decode_signed(vr);
        s_state.targets[i].res_mm   = rr;
        s_state.targets[i].present  = !(xr == 0 && yr == 0 && vr == 0 && rr == 0);
    }
}

static void radar_task(void *arg) {
    uint8_t body[BODY_LEN];
    int header_pos = 0;
    int64_t last_log_us = 0;

    while (true) {
        uint8_t b;
        int n = uart_read_bytes(s_uart, &b, 1, pdMS_TO_TICKS(500));
        if (n != 1) continue;

        // Sync to "AA FF 03 00"
        if (b == HEADER[header_pos]) {
            header_pos++;
            if (header_pos < 4) continue;
            header_pos = 0;

            int got = uart_read_bytes(s_uart, body, BODY_LEN, pdMS_TO_TICKS(50));
            if (got != BODY_LEN) {
                s_state.bad_frames++;
                continue;
            }
            if (body[24] != 0x55 || body[25] != 0xCC) {
                s_state.bad_frames++;
                continue;
            }
            parse_frame(body);

            // Rate-limit logging to ~1 Hz so the console stays readable.
            int64_t now = esp_timer_get_time();
            if (now - last_log_us > 1000000) {
                last_log_us = now;
                int seen = 0;
                for (int i = 0; i < 3; i++) {
                    const ld2450_target_t *tg = &s_state.targets[i];
                    if (!tg->present) continue;
                    seen++;
                    ESP_LOGI(TAG,
                             "T%d  x=%5d mm  y=%5d mm  v=%4d cm/s  res=%4u mm",
                             i, tg->x_mm, tg->y_mm, tg->speed_cms, tg->res_mm);
                }
                if (seen == 0) {
                    ESP_LOGI(TAG, "no targets  (frames=%lu bad=%lu)",
                             (unsigned long)s_state.frame_count,
                             (unsigned long)s_state.bad_frames);
                }
            }
        } else {
            // Re-sync: maybe this byte is the start of a new header.
            header_pos = (b == HEADER[0]) ? 1 : 0;
        }
    }
}

void ld2450_init(uart_port_t uart_num, int rx_gpio, int tx_gpio) {
    s_uart = uart_num;

    uart_config_t cfg = {
        .baud_rate  = 256000,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(uart_num, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_gpio, rx_gpio,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(radar_task, "ld2450", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "init: uart=%d rx=GPIO%d tx=GPIO%d baud=256000",
             uart_num, rx_gpio, tx_gpio);
}

const ld2450_state_t *ld2450_get_state(void) {
    return &s_state;
}
