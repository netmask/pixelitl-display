#include "board.h"
#include "lcd.h"
#include "touch.h"
#include "framebuffer.h"
#include "wasm_runtime.h"
#include "wasm_host.h"
#include "wifi.h"
#include "http_request.h"
#include "kv_store.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

// Embedded face binary — only store is built into flash
extern const uint8_t store_wasm_start[]   asm("_binary_store_wasm_start");
extern const uint8_t store_wasm_end[]     asm("_binary_store_wasm_end");

static int s_active_face = 0;
static uint32_t s_frame_count = 0;

// --- Core 1: WASM executor ---
// Syncs to LCD vsync via fb_wait_vsync() — produces exactly one frame per
// LCD refresh period (~51 Hz at 21 MHz pixel clock).
static void wasm_task(void *arg) {
    ESP_LOGI(TAG, "WASM task on core %d", xPortGetCoreID());
    int64_t last_frame_us = esp_timer_get_time();
    bool wifi_notified = false;

    while (true) {
        fb_wait_vsync();

        // Notify faces once when WiFi connects
        if (!wifi_notified && wifi_is_connected()) {
            wifi_notified = true;
            wasm_notify_wifi_ready();
            ESP_LOGI(TAG, "WiFi ready — notified all faces");
        }

        // Check for face install ready between frames (no face executing)
        if (wasm_install_ready()) {
            int new_id = wasm_install_commit();
            if (new_id >= 0) {
                ESP_LOGI(TAG, "Installed new face id=%d, total=%d", new_id, wasm_face_count());
            }
        }

        int64_t now = esp_timer_get_time();
        uint32_t dt_ms = (uint32_t)((now - last_frame_us) / 1000);
        if (dt_ms == 0) dt_ms = 1;
        last_frame_us = now;

        wasm_host_set_frame(s_frame_count);
        int64_t exec_us = wasm_execute_frame(s_active_face, s_frame_count, dt_ms);
        face_t *f = wasm_get_face(s_active_face);
        bool frame_ok = (exec_us >= 0 && f && !f->over_budget);

        if (frame_ok) {
            fb_publish();
        }

        s_frame_count++;
    }
}

// --- Core 0: Touch polling + face switching ---
static void touch_task(void *arg) {
    ESP_LOGI(TAG, "Touch task on core %d", xPortGetCoreID());

    while (true) {
        touch_poll();

        int32_t gesture = touch_consume_gesture();
        if (gesture == GESTURE_SWIPE_LEFT || gesture == GESTURE_SWIPE_RIGHT) {
            int count = wasm_face_count();
            if (count > 1) {
                int next;
                if (gesture == GESTURE_SWIPE_LEFT) {
                    next = (s_active_face + 1) % count;
                } else {
                    next = (s_active_face + count - 1) % count;
                }
                s_active_face = next;
                face_t *f = wasm_get_face(next);
                ESP_LOGI(TAG, "Switched to face '%s' (%d)", f ? f->name : "?", next);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Pixelitl Display starting");

    // NVS init (erase on corrupt)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(kv_store_init());

    fb_init();
    wasm_host_init();
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(wasm_init());

    // Load only store face — all others are installed on-demand via the store
    int store_id = wasm_load_face("store",
        store_wasm_start, store_wasm_end - store_wasm_start);
    if (store_id >= 0) s_active_face = store_id;

    ESP_LOGI(TAG, "Loaded %d WASM face(s), active=%d", wasm_face_count(), s_active_face);
    ESP_LOGI(TAG, "Free internal: %u, largest block: %u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    xTaskCreatePinnedToCore(touch_task, "touch", 4096,  NULL, 5, NULL, 0);

    // Allocate wasm_task stack from PSRAM — internal RAM is too fragmented for 32KB
    static StaticTask_t s_wasm_tcb;
    static StackType_t *s_wasm_stack;
    s_wasm_stack = heap_caps_malloc(32768, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_wasm_stack) {
        xTaskCreateStaticPinnedToCore(wasm_task, "wasm", 32768, NULL, 5,
                                       s_wasm_stack, &s_wasm_tcb, 1);
    } else {
        ESP_LOGE(TAG, "FAILED to allocate wasm_task stack from PSRAM!");
    }

    // Start WiFi + HTTP after LCD is running (avoids PSRAM bus contention during init)
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(http_request_init());

    // Main task: periodic stats logging
    uint32_t last_frames = 0;
    int64_t last_stats_us = esp_timer_get_time();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        int64_t now = esp_timer_get_time();
        int64_t elapsed_us = now - last_stats_us;
        uint32_t cur_frames = s_frame_count;
        uint32_t delta_frames = cur_frames - last_frames;
        float fps = (float)delta_frames / ((float)elapsed_us / 1000000.0f);

        last_frames = cur_frames;
        last_stats_us = now;

        face_t *f = wasm_get_face(s_active_face);
        if (f) {
            ESP_LOGI(TAG, "Face '%s': %.1f FPS, avg_frame=%lld us, total=%u, skipped=%u",
                     f->name, fps, (long long)f->avg_frame_us,
                     f->total_frames, f->skipped_frames);
        }
    }
}
