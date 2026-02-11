#include "board.h"
#include "lcd.h"
#include "touch.h"
#include "framebuffer.h"
#include "graphics.h"
#include "wasm_runtime.h"
#include "wasm_host.h"
#include "native_face.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "main";

// Face IDs: 0..N-1 are WASM faces, N is the native plasma face
#define NATIVE_FACE_ID  99  // Sentinel for native face

// Embedded face binaries
extern const uint8_t plasma_wasm_start[] asm("_binary_plasma_wasm_start");
extern const uint8_t plasma_wasm_end[]   asm("_binary_plasma_wasm_end");
extern const uint8_t clock_wasm_start[]  asm("_binary_clock_wasm_start");
extern const uint8_t clock_wasm_end[]    asm("_binary_clock_wasm_end");
extern const uint8_t demo_wasm_start[]   asm("_binary_demo_wasm_start");
extern const uint8_t demo_wasm_end[]     asm("_binary_demo_wasm_end");

static int s_active_face = 0;
static uint32_t s_frame_count = 0;

// --- Core 1: WASM/native executor ---
// Syncs to LCD vsync via fb_wait_vsync() — produces exactly one frame per
// LCD refresh period (~51 Hz at 21 MHz pixel clock).
static void wasm_task(void *arg) {
    ESP_LOGI(TAG, "WASM task on core %d", xPortGetCoreID());
    int64_t last_frame_us = esp_timer_get_time();

    while (true) {
        fb_wait_vsync();

        int64_t now = esp_timer_get_time();
        uint32_t dt_ms = (uint32_t)((now - last_frame_us) / 1000);
        if (dt_ms == 0) dt_ms = 1;
        last_frame_us = now;

        bool frame_ok = false;

        if (s_active_face == NATIVE_FACE_ID) {
            native_face_update(s_frame_count, dt_ms);
            native_face_draw(fb_get_back());
            frame_ok = true;
        } else {
            wasm_host_set_frame(s_frame_count);
            int64_t exec_us = wasm_execute_frame(s_active_face, s_frame_count, dt_ms);
            face_t *f = wasm_get_face(s_active_face);
            frame_ok = (exec_us >= 0 && f && !f->over_budget);
        }

        if (frame_ok) {
            fb_publish();
        }

        s_frame_count++;
    }
}

// --- Core 0: Touch polling + face switching ---
// Scaling is handled by the LCD bounce buffer ISR, so this task
// only needs to poll touch and process gestures.
static void render_task(void *arg) {
    ESP_LOGI(TAG, "Touch task on core %d", xPortGetCoreID());

    while (true) {
        touch_poll();

        int32_t gesture = touch_consume_gesture();
        if (gesture == GESTURE_SWIPE_LEFT || gesture == GESTURE_SWIPE_RIGHT) {
            int wasm_count = wasm_face_count();
            int cur_idx;
            if (s_active_face == NATIVE_FACE_ID) {
                cur_idx = wasm_count;
            } else {
                cur_idx = s_active_face;
            }

            int total = wasm_count + 1;
            int next_idx;
            if (gesture == GESTURE_SWIPE_LEFT) {
                next_idx = (cur_idx + 1) % total;
            } else {
                next_idx = (cur_idx + total - 1) % total;
            }

            if (next_idx >= wasm_count) {
                s_active_face = NATIVE_FACE_ID;
                ESP_LOGI("main", "Switched to face 'NATIVE plasma'");
            } else {
                s_active_face = next_idx;
                face_t *f = wasm_get_face(next_idx);
                ESP_LOGI("main", "Switched to face '%s' (%d)", f ? f->name : "?", next_idx);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Pixelitl Display starting");

    fb_init();
    wasm_host_init();
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(wasm_init());

    // Load embedded WASM faces
    int plasma_id = wasm_load_face("plasma",
        plasma_wasm_start, plasma_wasm_end - plasma_wasm_start);
    int clock_id = wasm_load_face("clock",
        clock_wasm_start, clock_wasm_end - clock_wasm_start);
    int demo_id = wasm_load_face("demo",
        demo_wasm_start, demo_wasm_end - demo_wasm_start);

    // Initialize native face
    native_face_init();

    if (plasma_id >= 0) s_active_face = plasma_id;
    else if (clock_id >= 0) s_active_face = clock_id;
    else if (demo_id >= 0) s_active_face = demo_id;

    ESP_LOGI(TAG, "Loaded %d WASM faces + 1 native face, active=%d",
             wasm_face_count(), s_active_face);

    xTaskCreatePinnedToCore(render_task, "touch",  4096,  NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(wasm_task,   "wasm",   32768, NULL, 5, NULL, 1);

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

        if (s_active_face == NATIVE_FACE_ID) {
            ESP_LOGI(TAG, "Face 'NATIVE plasma': %.1f FPS, avg_frame=%lld us, total=%u",
                     fps, (long long)native_face_get_avg_us(),
                     native_face_get_total_frames());
        } else {
            face_t *f = wasm_get_face(s_active_face);
            if (f) {
                ESP_LOGI(TAG, "Face '%s': %.1f FPS, avg_frame=%lld us, total=%u, skipped=%u, state=%d",
                         f->name, fps, (long long)f->avg_frame_us,
                         f->total_frames, f->skipped_frames, f->state);
            }
        }
    }
}
