// Native C plasma face — same algorithm as the WASM plasma face,
// but runs directly in native code without the wasm3 interpreter.
// Used for benchmarking WASM interpreter overhead.

#include "native_face.h"
#include "graphics.h"
#include "board.h"
#include "esp_timer.h"
#include <math.h>

static float s_time_offset;
static int64_t s_avg_frame_us;
static uint32_t s_total_frames;

void native_face_init(void) {
    s_time_offset = 0.0f;
    s_avg_frame_us = 0;
    s_total_frames = 0;
}

void native_face_update(uint32_t frame, uint32_t dt_ms) {
    (void)frame;
    s_time_offset += (float)dt_ms * 0.001f;
}

void native_face_draw(uint16_t *fb) {
    int64_t start = esp_timer_get_time();

    int w = VFB_WIDTH;
    int h = VFB_HEIGHT;

    // Same algorithm as WASM plasma: quarter resolution (4×4 blocks)
    for (int y = 0; y < h; y += 4) {
        for (int x = 0; x < w; x += 4) {
            float fx = (float)x / (float)w;
            float fy = (float)y / (float)h;

            float v = sinf(fx * 10.0f + s_time_offset);
            v += sinf(fy * 8.0f + s_time_offset * 0.7f);

            float hue = fmodf((v + 2.0f) * 90.0f + s_time_offset * 30.0f, 360.0f);
            uint16_t color = gfx_hsv(hue, 1.0f, 1.0f);
            gfx_fill_rect(fb, x, y, 4, 4, color);
        }
    }

    int64_t elapsed = esp_timer_get_time() - start;
    s_total_frames++;
    if (s_avg_frame_us == 0) {
        s_avg_frame_us = elapsed;
    } else {
        s_avg_frame_us = (s_avg_frame_us * 9 + elapsed) / 10;
    }
}

int64_t native_face_get_avg_us(void) {
    return s_avg_frame_us;
}

uint32_t native_face_get_total_frames(void) {
    return s_total_frames;
}
