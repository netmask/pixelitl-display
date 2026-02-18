#include "wasm_host.h"
#include "wasm_runtime.h"
#include "graphics.h"
#include "framebuffer.h"
#include "touch.h"
#include "board.h"
#include "wifi.h"
#include "http_request.h"
#include "kv_store.h"
#include "wasm3.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern void wasm_set_active_face(int id);

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

// Blit entire framebuffer from WASM memory in one call
m3ApiRawFunction(host_blit_framebuffer) {
    m3ApiGetArg(int32_t, ptr);
    BUDGET_CHECK();

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    uint32_t fb_bytes = fb_width() * fb_height() * sizeof(uint16_t);
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
    int fw = fb_width(), fh = fb_height();

    for (int row = 0; row < h; row++) {
        int sy = dy + row;
        if (sy < 0 || sy >= fh) continue;
        for (int col = 0; col < w; col++) {
            int sx = dx + col;
            if (sx < 0 || sx >= fw) continue;
            dst[sy * fw + sx] = src[row * w + col];
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
    m3ApiReturn(fb_width());
}

m3ApiRawFunction(host_height) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(fb_height());
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

// ===================== Resolution =====================

m3ApiRawFunction(host_set_resolution) {
    m3ApiGetArg(int32_t, mode);
    bool hd = (mode == 1);
    fb_set_resolution(hd);
    if (g_current_face) {
        g_current_face->hd = hd;
    }
    m3ApiSuccess();
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

// ===================== WiFi =====================

m3ApiRawFunction(host_wifi_status) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(wifi_is_connected() ? 1 : 0);
}

// ===================== HTTP =====================

m3ApiRawFunction(host_http_get) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || len <= 0 || (uint32_t)(ptr + len) > mem_size) {
        m3ApiReturn(-1);
    }
    m3ApiReturn(http_request_start_get((const char *)(mem + ptr), len));
}

m3ApiRawFunction(host_http_post) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, url_ptr);
    m3ApiGetArg(int32_t, url_len);
    m3ApiGetArg(int32_t, ct_ptr);
    m3ApiGetArg(int32_t, ct_len);
    m3ApiGetArg(int32_t, body_ptr);
    m3ApiGetArg(int32_t, body_len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || url_len <= 0 || ct_len <= 0 || body_len < 0) {
        m3ApiReturn(-1);
    }
    if ((uint32_t)(url_ptr + url_len) > mem_size ||
        (uint32_t)(ct_ptr + ct_len) > mem_size ||
        (uint32_t)(body_ptr + body_len) > mem_size) {
        m3ApiReturn(-1);
    }
    m3ApiReturn(http_request_start_post(
        (const char *)(mem + url_ptr), url_len,
        (const char *)(mem + ct_ptr), ct_len,
        mem + body_ptr, body_len));
}

m3ApiRawFunction(host_http_status) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, handle);
    m3ApiReturn((int32_t)http_request_status(handle));
}

m3ApiRawFunction(host_http_response_code) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, handle);
    m3ApiReturn(http_request_response_code(handle));
}

m3ApiRawFunction(host_http_response_len) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, handle);
    m3ApiReturn(http_request_response_len(handle));
}

m3ApiRawFunction(host_http_read) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, handle);
    m3ApiGetArg(int32_t, dst_ptr);
    m3ApiGetArg(int32_t, max_len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || max_len <= 0 || (uint32_t)(dst_ptr + max_len) > mem_size) {
        m3ApiReturn(-1);
    }
    m3ApiReturn(http_request_read(handle, mem + dst_ptr, max_len));
}

m3ApiRawFunction(host_http_close) {
    m3ApiGetArg(int32_t, handle);
    http_request_close(handle);
    m3ApiSuccess();
}

// ===================== KV Store =====================

m3ApiRawFunction(host_kv_set) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, key_ptr);
    m3ApiGetArg(int32_t, key_len);
    m3ApiGetArg(int32_t, val_ptr);
    m3ApiGetArg(int32_t, val_len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || key_len <= 0 || val_len < 0 ||
        (uint32_t)(key_ptr + key_len) > mem_size ||
        (uint32_t)(val_ptr + val_len) > mem_size) {
        m3ApiReturn(-1);
    }
    m3ApiReturn(kv_store_set(g_current_face->name,
                             mem + key_ptr, key_len,
                             mem + val_ptr, val_len));
}

m3ApiRawFunction(host_kv_get) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, key_ptr);
    m3ApiGetArg(int32_t, key_len);
    m3ApiGetArg(int32_t, dst_ptr);
    m3ApiGetArg(int32_t, max_len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || key_len <= 0 || max_len <= 0 ||
        (uint32_t)(key_ptr + key_len) > mem_size ||
        (uint32_t)(dst_ptr + max_len) > mem_size) {
        m3ApiReturn(-1);
    }
    m3ApiReturn(kv_store_get(g_current_face->name,
                             mem + key_ptr, key_len,
                             mem + dst_ptr, max_len));
}

m3ApiRawFunction(host_kv_del) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, key_ptr);
    m3ApiGetArg(int32_t, key_len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || key_len <= 0 || (uint32_t)(key_ptr + key_len) > mem_size) {
        m3ApiReturn(-1);
    }
    m3ApiReturn(kv_store_del(g_current_face->name, mem + key_ptr, key_len));
}

// ===================== Face Install =====================

m3ApiRawFunction(host_face_install) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, url_ptr);
    m3ApiGetArg(int32_t, url_len);
    m3ApiGetArg(int32_t, name_ptr);
    m3ApiGetArg(int32_t, name_len);

    uint32_t mem_size;
    uint8_t *mem = m3_GetMemory(g_current_face->runtime, &mem_size, 0);
    if (!mem || url_len <= 0 || name_len <= 0 ||
        (uint32_t)(url_ptr + url_len) > mem_size ||
        (uint32_t)(name_ptr + name_len) > mem_size) {
        m3ApiReturn(-1);
    }
    m3ApiReturn(wasm_install_start((const char *)(mem + url_ptr), url_len,
                                    (const char *)(mem + name_ptr), name_len));
}

m3ApiRawFunction(host_face_install_status) {
    m3ApiReturnType(int32_t);
    m3ApiReturn((int32_t)wasm_install_status());
}

m3ApiRawFunction(host_face_install_reset) {
    wasm_install_reset();
    m3ApiSuccess();
}

m3ApiRawFunction(host_face_count) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(wasm_face_count());
}

m3ApiRawFunction(host_face_switch) {
    m3ApiGetArg(int32_t, index);
    wasm_set_active_face(index);
    m3ApiSuccess();
}

m3ApiRawFunction(host_face_get_name) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, index);
    m3ApiGetArg(int32_t, dst_ptr);
    m3ApiGetArg(int32_t, max_len);
    m3ApiGetArgMem(uint8_t *, mem);

    face_t *f = wasm_get_face(index);
    if (!f || max_len <= 0) m3ApiReturn(0);

    int name_len = strlen(f->name);
    if (name_len >= max_len) name_len = max_len - 1;
    memcpy(mem + dst_ptr, f->name, name_len);
    mem[dst_ptr + name_len] = '\0';
    m3ApiReturn(name_len);
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
    m3_LinkRawFunction(module, MOD, "millis",          "I()",      host_millis);
    m3_LinkRawFunction(module, MOD, "width",           "i()",      host_width);
    m3_LinkRawFunction(module, MOD, "height",          "i()",      host_height);
    m3_LinkRawFunction(module, MOD, "log_print",       "v(ii)",    host_log_print);
    m3_LinkRawFunction(module, MOD, "get_frame",       "i()",      host_get_frame);
    m3_LinkRawFunction(module, MOD, "set_resolution",  "v(i)",     host_set_resolution);

    // Touch
    m3_LinkRawFunction(module, MOD, "get_gesture",   "i()",      host_get_gesture);
    m3_LinkRawFunction(module, MOD, "get_touch_x",   "i()",      host_get_touch_x);
    m3_LinkRawFunction(module, MOD, "get_touch_y",   "i()",      host_get_touch_y);
    m3_LinkRawFunction(module, MOD, "is_pressed",    "i()",      host_is_pressed);

    // WiFi
    m3_LinkRawFunction(module, MOD, "wifi_status",   "i()",      host_wifi_status);

    // HTTP (async)
    m3_LinkRawFunction(module, MOD, "http_get",           "i(ii)",      host_http_get);
    m3_LinkRawFunction(module, MOD, "http_post",          "i(iiiiii)",  host_http_post);
    m3_LinkRawFunction(module, MOD, "http_status",        "i(i)",       host_http_status);
    m3_LinkRawFunction(module, MOD, "http_response_code", "i(i)",       host_http_response_code);
    m3_LinkRawFunction(module, MOD, "http_response_len",  "i(i)",       host_http_response_len);
    m3_LinkRawFunction(module, MOD, "http_read",          "i(iii)",     host_http_read);
    m3_LinkRawFunction(module, MOD, "http_close",         "v(i)",       host_http_close);

    // KV Store
    m3_LinkRawFunction(module, MOD, "kv_set",        "i(iiii)",  host_kv_set);
    m3_LinkRawFunction(module, MOD, "kv_get",        "i(iiii)",  host_kv_get);
    m3_LinkRawFunction(module, MOD, "kv_del",        "i(ii)",    host_kv_del);

    // Face Install
    m3_LinkRawFunction(module, MOD, "face_install",        "i(iiii)", host_face_install);
    m3_LinkRawFunction(module, MOD, "face_install_status", "i()",     host_face_install_status);
    m3_LinkRawFunction(module, MOD, "face_install_reset",  "v()",     host_face_install_reset);
    m3_LinkRawFunction(module, MOD, "face_count",          "i()",     host_face_count);
    m3_LinkRawFunction(module, MOD, "face_get_name",      "i(iii)",  host_face_get_name);
    m3_LinkRawFunction(module, MOD, "face_switch",        "v(i)",    host_face_switch);
}
