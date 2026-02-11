#include "wasm_host.h"
#include "wasm_runtime.h"
#include "graphics.h"
#include "framebuffer.h"
#include "touch.h"
#include "board.h"
#include "wasm3.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint32_t s_frame_count = 0;
const char *wasm_host_budget_trap = "budget exceeded";

// --- Fast sine/cosine LUT (512 entries for 0..2π) ---
#define SIN_LUT_SIZE  512
#define SIN_LUT_MASK  (SIN_LUT_SIZE - 1)
#define TWO_PI        6.28318530718f
#define INV_TWO_PI    (1.0f / TWO_PI)

static float s_sin_lut[SIN_LUT_SIZE];

static void init_sin_lut(void) {
    for (int i = 0; i < SIN_LUT_SIZE; i++) {
        s_sin_lut[i] = sinf((float)i * TWO_PI / (float)SIN_LUT_SIZE);
    }
}

static inline float fast_sinf(float x) {
    // Normalize to 0..1 range (one full period)
    float phase = x * INV_TWO_PI;
    phase -= (float)(int)phase;  // fract
    if (phase < 0.0f) phase += 1.0f;

    float idx_f = phase * (float)SIN_LUT_SIZE;
    int idx = (int)idx_f;
    float frac = idx_f - (float)idx;
    idx &= SIN_LUT_MASK;
    int idx_next = (idx + 1) & SIN_LUT_MASK;

    // Linear interpolation
    return s_sin_lut[idx] + frac * (s_sin_lut[idx_next] - s_sin_lut[idx]);
}

static inline float fast_cosf(float x) {
    return fast_sinf(x + 1.5707963f);  // cos(x) = sin(x + π/2)
}

void wasm_host_init(void) {
    init_sin_lut();
    ESP_LOGI("wasm_host", "Sine LUT initialized (%d entries)", SIN_LUT_SIZE);
}

void wasm_host_set_frame(uint32_t f) {
    s_frame_count = f;
}

// Budget check — traps the runtime to abort WASM execution immediately.
#define BUDGET_CHECK() do { \
    if (g_current_face && !g_current_face->over_budget) { \
        int64_t _elapsed = esp_timer_get_time() - g_current_face->frame_start_us; \
        if (_elapsed > g_current_face->budget_us) { \
            g_current_face->over_budget = true; \
            return wasm_host_budget_trap; \
        } \
    } \
    if (g_current_face && g_current_face->over_budget) { \
        return wasm_host_budget_trap; \
    } \
} while(0)

#define BUDGET_CHECK_RET(val) BUDGET_CHECK()

// ===================== Graphics =====================

m3ApiRawFunction(host_clear) {
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_clear(g_current_face->draw_target, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_set_pixel) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_set_pixel(g_current_face->draw_target, x, y, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_fill_rect) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, w);
    m3ApiGetArg(int32_t, h);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_fill_rect(g_current_face->draw_target, x, y, w, h, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_draw_rect) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, w);
    m3ApiGetArg(int32_t, h);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_draw_rect(g_current_face->draw_target, x, y, w, h, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_draw_line_h) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, len);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_draw_line_h(g_current_face->draw_target, x, y, len, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_draw_line_v) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, len);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_draw_line_v(g_current_face->draw_target, x, y, len, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_draw_circle) {
    m3ApiGetArg(int32_t, cx);
    m3ApiGetArg(int32_t, cy);
    m3ApiGetArg(int32_t, r);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_draw_circle(g_current_face->draw_target, cx, cy, r, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_fill_circle) {
    m3ApiGetArg(int32_t, cx);
    m3ApiGetArg(int32_t, cy);
    m3ApiGetArg(int32_t, r);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK();
    gfx_fill_circle(g_current_face->draw_target, cx, cy, r, (uint16_t)color);
    m3ApiSuccess();
}

m3ApiRawFunction(host_draw_text) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, len);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK_RET(0);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || (uint32_t)(ptr + len) > mem_size) {
        m3ApiReturn(0);
    }
    int width = gfx_draw_text(g_current_face->draw_target, x, y,
                              (const char *)(mem + ptr), len, (uint16_t)color);
    m3ApiReturn(width);
}

m3ApiRawFunction(host_draw_text_big) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, len);
    m3ApiGetArg(uint32_t, color);
    BUDGET_CHECK_RET(0);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || (uint32_t)(ptr + len) > mem_size) {
        m3ApiReturn(0);
    }
    int width = gfx_draw_text_big(g_current_face->draw_target, x, y,
                                  (const char *)(mem + ptr), len, (uint16_t)color);
    m3ApiReturn(width);
}

// Blit entire framebuffer from WASM memory in one call (80×48 RGB565 = 7680 bytes)
m3ApiRawFunction(host_blit_framebuffer) {
    m3ApiGetArg(int32_t, ptr);
    BUDGET_CHECK();

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    uint32_t fb_bytes = VFB_WIDTH * VFB_HEIGHT * sizeof(uint16_t);
    if (mem && (uint32_t)(ptr + fb_bytes) <= mem_size) {
        memcpy(g_current_face->draw_target, mem + ptr, fb_bytes);
    }
    m3ApiSuccess();
}

// Blit a rectangular region from WASM memory (row-major RGB565)
m3ApiRawFunction(host_blit_rect) {
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, dx);
    m3ApiGetArg(int32_t, dy);
    m3ApiGetArg(int32_t, w);
    m3ApiGetArg(int32_t, h);
    BUDGET_CHECK();

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    uint32_t src_bytes = w * h * sizeof(uint16_t);
    if (!mem || (uint32_t)(ptr + src_bytes) > mem_size) {
        m3ApiSuccess();
    }

    const uint16_t *src = (const uint16_t *)(mem + ptr);
    uint16_t *dst = g_current_face->draw_target;

    for (int row = 0; row < h; row++) {
        int sy = dy + row;
        if (sy < 0 || sy >= VFB_HEIGHT) continue;
        for (int col = 0; col < w; col++) {
            int sx = dx + col;
            if (sx < 0 || sx >= VFB_WIDTH) continue;
            dst[sy * VFB_WIDTH + sx] = src[row * w + col];
        }
    }
    m3ApiSuccess();
}

// ===================== Color =====================

m3ApiRawFunction(host_rgb) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, r);
    m3ApiGetArg(int32_t, g);
    m3ApiGetArg(int32_t, b);
    m3ApiReturn((uint32_t)gfx_rgb(r, g, b));
}

m3ApiRawFunction(host_hsv) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(float, h);
    m3ApiGetArg(float, s);
    m3ApiGetArg(float, v);
    m3ApiReturn((uint32_t)gfx_hsv(h, s, v));
}

m3ApiRawFunction(host_dim_color) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, color);
    m3ApiGetArg(float, factor);
    m3ApiReturn((uint32_t)gfx_dim_color((uint16_t)color, factor));
}

m3ApiRawFunction(host_lerp_color) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, c1);
    m3ApiGetArg(uint32_t, c2);
    m3ApiGetArg(float, t);
    m3ApiReturn((uint32_t)gfx_lerp_color((uint16_t)c1, (uint16_t)c2, t));
}

// ===================== Math =====================

m3ApiRawFunction(host_sinf) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, x);
    m3ApiReturn(fast_sinf(x));
}

m3ApiRawFunction(host_cosf) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, x);
    m3ApiReturn(fast_cosf(x));
}

m3ApiRawFunction(host_sqrtf) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, x);
    m3ApiReturn(sqrtf(x));
}

m3ApiRawFunction(host_atan2f) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, y);
    m3ApiGetArg(float, x);
    m3ApiReturn(atan2f(y, x));
}

m3ApiRawFunction(host_fmodf) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, x);
    m3ApiGetArg(float, y);
    m3ApiReturn(fmodf(x, y));
}

m3ApiRawFunction(host_absf) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, x);
    m3ApiReturn(fabsf(x));
}

// ===================== Easing =====================

m3ApiRawFunction(host_ease_in_quad) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    m3ApiReturn(t * t);
}

m3ApiRawFunction(host_ease_out_quad) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    m3ApiReturn(1.0f - (1.0f - t) * (1.0f - t));
}

m3ApiRawFunction(host_ease_in_out_quad) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    m3ApiReturn(t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f);
}

m3ApiRawFunction(host_ease_in_cubic) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    m3ApiReturn(t * t * t);
}

m3ApiRawFunction(host_ease_out_cubic) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    float u = 1.0f - t;
    m3ApiReturn(1.0f - u * u * u);
}

m3ApiRawFunction(host_ease_in_out_cubic) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    if (t < 0.5f) {
        m3ApiReturn(4.0f * t * t * t);
    }
    float u = -2.0f * t + 2.0f;
    m3ApiReturn(1.0f - u * u * u * 0.5f);
}

m3ApiRawFunction(host_ease_out_elastic) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    if (t <= 0.0f) { m3ApiReturn(0.0f); }
    if (t >= 1.0f) { m3ApiReturn(1.0f); }
    float p = 0.3f;
    m3ApiReturn(powf(2.0f, -10.0f * t) * sinf((t - p / 4.0f) * (2.0f * 3.14159265f) / p) + 1.0f);
}

m3ApiRawFunction(host_ease_out_bounce) {
    m3ApiReturnType(float);
    m3ApiGetArg(float, t);
    if (t < 1.0f / 2.75f) {
        m3ApiReturn(7.5625f * t * t);
    } else if (t < 2.0f / 2.75f) {
        t -= 1.5f / 2.75f;
        m3ApiReturn(7.5625f * t * t + 0.75f);
    } else if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        m3ApiReturn(7.5625f * t * t + 0.9375f);
    }
    t -= 2.625f / 2.75f;
    m3ApiReturn(7.5625f * t * t + 0.984375f);
}

m3ApiRawFunction(host_rand) {
    m3ApiReturnType(int32_t);
    m3ApiReturn((int32_t)(rand() & 0x7FFF));
}

m3ApiRawFunction(host_srand) {
    m3ApiGetArg(int32_t, seed);
    srand((unsigned)seed);
    m3ApiSuccess();
}

// ===================== System =====================

m3ApiRawFunction(host_millis) {
    m3ApiReturnType(int64_t);
    m3ApiReturn((int64_t)(esp_timer_get_time() / 1000));
}

m3ApiRawFunction(host_width) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(VFB_WIDTH);
}

m3ApiRawFunction(host_height) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(VFB_HEIGHT);
}

m3ApiRawFunction(host_log_print) {
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (mem && len > 0 && len < 256 && (uint32_t)(ptr + len) <= mem_size) {
        char buf[257];
        memcpy(buf, mem + ptr, len);
        buf[len] = '\0';
        ESP_LOGI("wasm", "[%s] %s", g_current_face->name, buf);
    }
    m3ApiSuccess();
}

m3ApiRawFunction(host_get_frame) {
    m3ApiReturnType(int32_t);
    m3ApiReturn((int32_t)s_frame_count);
}

// ===================== Touch =====================

m3ApiRawFunction(host_get_gesture) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(touch_consume_gesture());
}

m3ApiRawFunction(host_get_touch_x) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(touch_get_state()->virtual_x);
}

m3ApiRawFunction(host_get_touch_y) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(touch_get_state()->virtual_y);
}

m3ApiRawFunction(host_is_pressed) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(touch_get_state()->pressed ? 1 : 0);
}

// ===================== Link all =====================

#define MOD "env"

void wasm_host_link_all(IM3Module module) {
    // Graphics
    m3_LinkRawFunction(module, MOD, "clear",         "v(i)",     host_clear);
    m3_LinkRawFunction(module, MOD, "set_pixel",     "v(iii)",   host_set_pixel);
    m3_LinkRawFunction(module, MOD, "fill_rect",     "v(iiiii)", host_fill_rect);
    m3_LinkRawFunction(module, MOD, "draw_rect",     "v(iiiii)", host_draw_rect);
    m3_LinkRawFunction(module, MOD, "draw_line_h",   "v(iiii)",  host_draw_line_h);
    m3_LinkRawFunction(module, MOD, "draw_line_v",   "v(iiii)",  host_draw_line_v);
    m3_LinkRawFunction(module, MOD, "draw_circle",   "v(iiii)",  host_draw_circle);
    m3_LinkRawFunction(module, MOD, "fill_circle",   "v(iiii)",  host_fill_circle);
    m3_LinkRawFunction(module, MOD, "draw_text",     "i(iiiii)", host_draw_text);
    m3_LinkRawFunction(module, MOD, "draw_text_big", "i(iiiii)", host_draw_text_big);
    m3_LinkRawFunction(module, MOD, "blit_framebuffer", "v(i)",     host_blit_framebuffer);
    m3_LinkRawFunction(module, MOD, "blit_rect",        "v(iiiii)", host_blit_rect);

    // Color
    m3_LinkRawFunction(module, MOD, "rgb",           "i(iii)",   host_rgb);
    m3_LinkRawFunction(module, MOD, "hsv",           "i(fff)",   host_hsv);
    m3_LinkRawFunction(module, MOD, "dim_color",     "i(if)",    host_dim_color);
    m3_LinkRawFunction(module, MOD, "lerp_color",    "i(iif)",   host_lerp_color);

    // Math
    m3_LinkRawFunction(module, MOD, "sinf",          "f(f)",     host_sinf);
    m3_LinkRawFunction(module, MOD, "cosf",          "f(f)",     host_cosf);
    m3_LinkRawFunction(module, MOD, "sqrtf",         "f(f)",     host_sqrtf);
    m3_LinkRawFunction(module, MOD, "atan2f",        "f(ff)",    host_atan2f);
    m3_LinkRawFunction(module, MOD, "fmodf",         "f(ff)",    host_fmodf);
    m3_LinkRawFunction(module, MOD, "absf",          "f(f)",     host_absf);
    m3_LinkRawFunction(module, MOD, "rand",          "i()",      host_rand);

    // Easing
    m3_LinkRawFunction(module, MOD, "ease_in_quad",       "f(f)", host_ease_in_quad);
    m3_LinkRawFunction(module, MOD, "ease_out_quad",      "f(f)", host_ease_out_quad);
    m3_LinkRawFunction(module, MOD, "ease_in_out_quad",   "f(f)", host_ease_in_out_quad);
    m3_LinkRawFunction(module, MOD, "ease_in_cubic",      "f(f)", host_ease_in_cubic);
    m3_LinkRawFunction(module, MOD, "ease_out_cubic",     "f(f)", host_ease_out_cubic);
    m3_LinkRawFunction(module, MOD, "ease_in_out_cubic",  "f(f)", host_ease_in_out_cubic);
    m3_LinkRawFunction(module, MOD, "ease_out_elastic",   "f(f)", host_ease_out_elastic);
    m3_LinkRawFunction(module, MOD, "ease_out_bounce",    "f(f)", host_ease_out_bounce);
    m3_LinkRawFunction(module, MOD, "srand",         "v(i)",     host_srand);

    // System
    m3_LinkRawFunction(module, MOD, "millis",        "I()",      host_millis);
    m3_LinkRawFunction(module, MOD, "width",         "i()",      host_width);
    m3_LinkRawFunction(module, MOD, "height",        "i()",      host_height);
    m3_LinkRawFunction(module, MOD, "log_print",     "v(ii)",    host_log_print);
    m3_LinkRawFunction(module, MOD, "get_frame",     "i()",      host_get_frame);

    // Touch
    m3_LinkRawFunction(module, MOD, "get_gesture",   "i()",      host_get_gesture);
    m3_LinkRawFunction(module, MOD, "get_touch_x",   "i()",      host_get_touch_x);
    m3_LinkRawFunction(module, MOD, "get_touch_y",   "i()",      host_get_touch_y);
    m3_LinkRawFunction(module, MOD, "is_pressed",    "i()",      host_is_pressed);
}
