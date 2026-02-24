// Pixelitl Face SDK — include this in your face .c file
// Compile: clang --target=wasm32 -O2 -nostdlib -Wl,--no-entry
//   -Wl,--export=app_init -Wl,--export=app_update -Wl,--export=app_draw
//   -Wl,--initial-memory=65536 -Wl,--max-memory=262144
//   -I path/to/sdk -o myface.wasm myface.c
#pragma once

// Gesture constants
#define GESTURE_NONE        0
#define GESTURE_TAP         1
#define GESTURE_SWIPE_LEFT  2
#define GESTURE_SWIPE_RIGHT 3
#define GESTURE_SWIPE_UP    4
#define GESTURE_SWIPE_DOWN  5
#define GESTURE_LONG_PRESS  6

// --- Graphics ---
__attribute__((import_module("env"), import_name("clear")))
extern void clear(unsigned int color);

__attribute__((import_module("env"), import_name("set_pixel")))
extern void set_pixel(int x, int y, unsigned int color);

__attribute__((import_module("env"), import_name("fill_rect")))
extern void fill_rect(int x, int y, int w, int h, unsigned int color);

__attribute__((import_module("env"), import_name("draw_rect")))
extern void draw_rect(int x, int y, int w, int h, unsigned int color);

__attribute__((import_module("env"), import_name("draw_line_h")))
extern void draw_line_h(int x, int y, int len, unsigned int color);

__attribute__((import_module("env"), import_name("draw_line_v")))
extern void draw_line_v(int x, int y, int len, unsigned int color);

__attribute__((import_module("env"), import_name("draw_circle")))
extern void draw_circle(int cx, int cy, int r, unsigned int color);

__attribute__((import_module("env"), import_name("fill_circle")))
extern void fill_circle(int cx, int cy, int r, unsigned int color);

__attribute__((import_module("env"), import_name("draw_text")))
extern int draw_text(int x, int y, const char *str, int len, unsigned int color);

__attribute__((import_module("env"), import_name("draw_text_big")))
extern int draw_text_big(int x, int y, const char *str, int len, unsigned int color);

// --- Color ---
__attribute__((import_module("env"), import_name("rgb")))
extern unsigned int rgb(int r, int g, int b);

__attribute__((import_module("env"), import_name("hsv")))
extern unsigned int hsv(float h, float s, float v);

__attribute__((import_module("env"), import_name("dim_color")))
extern unsigned int dim_color(unsigned int color, float factor);

__attribute__((import_module("env"), import_name("lerp_color")))
extern unsigned int lerp_color(unsigned int c1, unsigned int c2, float t);

// --- Math ---
__attribute__((import_module("env"), import_name("sinf")))
extern float sinf(float x);

__attribute__((import_module("env"), import_name("cosf")))
extern float cosf(float x);

__attribute__((import_module("env"), import_name("sqrtf")))
extern float sqrtf(float x);

__attribute__((import_module("env"), import_name("atan2f")))
extern float atan2f(float y, float x);

__attribute__((import_module("env"), import_name("fmodf")))
extern float fmodf(float x, float y);

__attribute__((import_module("env"), import_name("absf")))
extern float absf(float x);

__attribute__((import_module("env"), import_name("rand")))
extern int rand(void);

__attribute__((import_module("env"), import_name("srand")))
extern void srand(int seed);

// --- System ---
__attribute__((import_module("env"), import_name("set_resolution")))
extern void set_resolution(int mode);  // 0=standard (80x48), 1=hd (160x96), 2=ultra (200x120)

__attribute__((import_module("env"), import_name("millis")))
extern long long millis(void);

__attribute__((import_module("env"), import_name("width")))
extern int width(void);

__attribute__((import_module("env"), import_name("height")))
extern int height(void);

__attribute__((import_module("env"), import_name("log_print")))
extern void log_print(const char *str, int len);

__attribute__((import_module("env"), import_name("get_frame")))
extern int get_frame(void);

// --- Touch ---
__attribute__((import_module("env"), import_name("get_gesture")))
extern int get_gesture(void);

__attribute__((import_module("env"), import_name("get_touch_x")))
extern int get_touch_x(void);

__attribute__((import_module("env"), import_name("get_touch_y")))
extern int get_touch_y(void);

__attribute__((import_module("env"), import_name("is_pressed")))
extern int is_pressed(void);

// --- Easing (t: 0.0 to 1.0) ---
__attribute__((import_module("env"), import_name("ease_in_quad")))
extern float ease_in_quad(float t);

__attribute__((import_module("env"), import_name("ease_out_quad")))
extern float ease_out_quad(float t);

__attribute__((import_module("env"), import_name("ease_in_out_quad")))
extern float ease_in_out_quad(float t);

__attribute__((import_module("env"), import_name("ease_in_cubic")))
extern float ease_in_cubic(float t);

__attribute__((import_module("env"), import_name("ease_out_cubic")))
extern float ease_out_cubic(float t);

__attribute__((import_module("env"), import_name("ease_in_out_cubic")))
extern float ease_in_out_cubic(float t);

__attribute__((import_module("env"), import_name("ease_out_elastic")))
extern float ease_out_elastic(float t);

__attribute__((import_module("env"), import_name("ease_out_bounce")))
extern float ease_out_bounce(float t);

// --- WiFi ---
__attribute__((import_module("env"), import_name("wifi_status")))
extern int wifi_status(void);

// --- HTTP (async) ---
// Status: FREE=0, PENDING=1, IN_PROGRESS=2, DONE=3, ERROR=4
#define HTTP_FREE        0
#define HTTP_PENDING     1
#define HTTP_IN_PROGRESS 2
#define HTTP_DONE        3
#define HTTP_ERROR       4

__attribute__((import_module("env"), import_name("http_get")))
extern int http_get(const char *url, int url_len);

__attribute__((import_module("env"), import_name("http_post")))
extern int http_post(const char *url, int url_len,
                     const char *content_type, int ct_len,
                     const char *body, int body_len);

__attribute__((import_module("env"), import_name("http_status")))
extern int http_status(int handle);

__attribute__((import_module("env"), import_name("http_response_code")))
extern int http_response_code(int handle);

__attribute__((import_module("env"), import_name("http_response_len")))
extern int http_response_len(int handle);

__attribute__((import_module("env"), import_name("http_read")))
extern int http_read(int handle, char *dst, int max_len);

__attribute__((import_module("env"), import_name("http_close")))
extern void http_close(int handle);

// --- KV Store (per-face namespace) ---
__attribute__((import_module("env"), import_name("kv_set")))
extern int kv_set(const char *key, int key_len, const char *value, int value_len);

__attribute__((import_module("env"), import_name("kv_get")))
extern int kv_get(const char *key, int key_len, char *dst, int max_len);

__attribute__((import_module("env"), import_name("kv_del")))
extern int kv_del(const char *key, int key_len);

// --- Face Install (download + load WASM faces at runtime) ---
#define INSTALL_IDLE         0
#define INSTALL_DOWNLOADING  1
#define INSTALL_LOADING      2
#define INSTALL_OK           3
#define INSTALL_ERROR       -1

__attribute__((import_module("env"), import_name("face_install")))
extern int face_install(const char *url, int url_len, const char *name, int name_len);

__attribute__((import_module("env"), import_name("face_install_status")))
extern int face_install_status(void);

__attribute__((import_module("env"), import_name("face_install_reset")))
extern void face_install_reset(void);

__attribute__((import_module("env"), import_name("face_count")))
extern int face_count(void);

__attribute__((import_module("env"), import_name("face_get_name")))
extern int face_get_name(int index, char *dst, int max_len);

__attribute__((import_module("env"), import_name("face_switch")))
extern void face_switch(int index);

// ============================================================
// Convenience helpers (no libc available)
// ============================================================

// --- String / Memory ---

static inline int pxl_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static inline int pxl_memcpy(void *dst, const void *src, int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    int c = n;
    while (n-- > 0) *d++ = *s++;
    return c;
}

static inline void pxl_memset(void *dst, unsigned char val, int n) {
    unsigned char *d = (unsigned char *)dst;
    while (n-- > 0) *d++ = val;
}

static inline int pxl_streq(const char *a, const char *b) {
    while (*a && *b) { if (*a++ != *b++) return 0; }
    return *a == *b;
}

static inline int pxl_strcpy(char *dst, int dst_sz, const char *src) {
    int i = 0;
    while (src[i] && i < dst_sz - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
    return i;
}

static inline void pxl_strcat(char *dst, int dst_sz, const char *src) {
    int i = pxl_strlen(dst);
    while (*src && i < dst_sz - 1) dst[i++] = *src++;
    dst[i] = '\0';
}

// --- Number formatting ---

// Integer to string. Returns number of chars written (excluding NUL).
static inline int pxl_itoa(int val, char *buf, int buf_sz) {
    if (buf_sz <= 0) return 0;
    char tmp[12];
    int neg = 0, i = 0;
    if (val < 0) { neg = 1; val = -val; }
    do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val && i < 11);
    if (neg && i < 11) tmp[i++] = '-';
    int len = (i < buf_sz) ? i : buf_sz - 1;
    for (int j = 0; j < len; j++) buf[j] = tmp[i - 1 - j];
    buf[len] = '\0';
    return len;
}

// Float to string with N decimal places. Returns chars written.
static inline int pxl_ftoa(float val, int decimals, char *buf, int buf_sz) {
    int neg = 0, pos = 0;
    if (val < 0) { neg = 1; val = -val; }
    // Round
    float r = 0.5f;
    for (int d = 0; d < decimals; d++) r *= 0.1f;
    val += r;
    int whole = (int)val;
    float frac = val - (float)whole;
    if (neg && buf_sz > 1) buf[pos++] = '-';
    pos += pxl_itoa(whole, buf + pos, buf_sz - pos);
    if (decimals > 0 && pos < buf_sz - 1) {
        buf[pos++] = '.';
        for (int d = 0; d < decimals && pos < buf_sz - 1; d++) {
            frac *= 10.0f;
            int digit = (int)frac;
            buf[pos++] = '0' + digit;
            frac -= (float)digit;
        }
    }
    buf[pos] = '\0';
    return pos;
}

// --- Text drawing helpers ---

#define PXL_FONT_W  6  // 5px glyph + 1px gap
#define PXL_FONT_H  7
#define PXL_FONT_BIG_W  12  // 10px glyph + 2px gap
#define PXL_FONT_BIG_H  14

static inline int draw_str(int x, int y, const char *s, unsigned int color) {
    return draw_text(x, y, s, pxl_strlen(s), color);
}

static inline int draw_str_big(int x, int y, const char *s, unsigned int color) {
    return draw_text_big(x, y, s, pxl_strlen(s), color);
}

// Draw text centered horizontally within [x, x+w).
static inline void draw_str_centered(int x, int w, int y, const char *s, unsigned int color) {
    int tw = pxl_strlen(s) * PXL_FONT_W;
    draw_str(x + (w - tw) / 2, y, s, color);
}

static inline void draw_str_big_centered(int x, int w, int y, const char *s, unsigned int color) {
    int tw = pxl_strlen(s) * PXL_FONT_BIG_W;
    draw_str_big(x + (w - tw) / 2, y, s, color);
}

// Draw an integer value as text. Returns pixel width drawn.
static inline int draw_int(int x, int y, int val, unsigned int color) {
    char buf[12];
    pxl_itoa(val, buf, sizeof(buf));
    return draw_str(x, y, buf, color);
}

static inline int draw_int_big(int x, int y, int val, unsigned int color) {
    char buf[12];
    pxl_itoa(val, buf, sizeof(buf));
    return draw_str_big(x, y, buf, color);
}

// --- API string helpers ---

static inline int http_get_str(const char *url) {
    return http_get(url, pxl_strlen(url));
}

static inline int face_install_url(const char *url, const char *name) {
    return face_install(url, pxl_strlen(url), name, pxl_strlen(name));
}

static inline int kv_set_str(const char *key, const char *value) {
    return kv_set(key, pxl_strlen(key), value, pxl_strlen(value));
}

static inline int kv_get_str(const char *key, char *dst, int max_len) {
    return kv_get(key, pxl_strlen(key), dst, max_len);
}

static inline int kv_del_str(const char *key) {
    return kv_del(key, pxl_strlen(key));
}

// ============================================================
// Graphics helpers
// ============================================================

// Bresenham diagonal line
static inline void draw_line(int x0, int y0, int x1, int y1, unsigned int color) {
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = dx - dy;
    while (1) {
        set_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

// Draw a sprite from a palette-indexed byte array. transparent_idx = -1 for none.
static inline void pxl_draw_sprite(int x, int y, const unsigned char *data,
                                    int w, int h, const unsigned int *palette,
                                    int transparent_idx) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int idx = data[row * w + col];
            if (idx != transparent_idx) {
                set_pixel(x + col, y + row, palette[idx]);
            }
        }
    }
}

// Fill a horizontal gradient with HSV hue sweep.
static inline void pxl_fill_hsv_gradient_h(int x, int y, int w, int h,
                                            float hue_start, float hue_end,
                                            float s, float v) {
    for (int col = 0; col < w; col++) {
        float t = (float)col / (float)(w > 1 ? w - 1 : 1);
        float hue = hue_start + (hue_end - hue_start) * t;
        fill_rect(x + col, y, 1, h, hsv(hue, s, v));
    }
}

// ============================================================
// Timer
// ============================================================

typedef struct {
    int elapsed;
    int duration;
} pxl_timer_t;

static inline void pxl_timer_start(pxl_timer_t *t, int ms) {
    t->elapsed = 0;
    t->duration = ms;
}

// Advance timer. Returns 1 when it fires (auto-resets for repeat).
static inline int pxl_timer_tick(pxl_timer_t *t, int dt) {
    t->elapsed += dt;
    if (t->elapsed >= t->duration) {
        t->elapsed -= t->duration;
        return 1;
    }
    return 0;
}

// Returns 0.0..1.0 progress.
static inline float pxl_timer_progress(pxl_timer_t *t) {
    if (t->duration <= 0) return 1.0f;
    float p = (float)t->elapsed / (float)t->duration;
    return p > 1.0f ? 1.0f : p;
}

// ============================================================
// Animation
// ============================================================

typedef struct {
    float t;        // current time (seconds)
    float duration; // total duration (seconds)
    int   done;
} pxl_anim_t;

// Start/restart an animation.
static inline void pxl_anim_start(pxl_anim_t *a, float duration_s) {
    a->t = 0.0f;
    a->duration = duration_s;
    a->done = 0;
}

// Advance and return raw progress (0.0..1.0). Clamps at 1.0.
static inline float pxl_anim_step(pxl_anim_t *a, int dt_ms) {
    if (a->done) return 1.0f;
    a->t += (float)dt_ms * 0.001f;
    if (a->t >= a->duration) { a->t = a->duration; a->done = 1; }
    return a->t / a->duration;
}

// ============================================================
// HTTP request wrapper
// ============================================================

#define PXL_HTTP_IDLE    0
#define PXL_HTTP_PENDING 1
#define PXL_HTTP_DONE    2
#define PXL_HTTP_ERROR   3

typedef struct {
    int handle;
    int state;       // PXL_HTTP_*
    int status_code;
    char *buf;       // user-provided buffer
    int buf_sz;
    int len;         // bytes received
} pxl_http_t;

// Initialize (call once, provide a buffer).
static inline void pxl_http_init(pxl_http_t *r, char *buf, int buf_sz) {
    r->handle = -1;
    r->state = PXL_HTTP_IDLE;
    r->status_code = 0;
    r->buf = buf;
    r->buf_sz = buf_sz;
    r->len = 0;
}

// Start a GET request. Returns 0 on success, -1 if busy.
static inline int pxl_http_get(pxl_http_t *r, const char *url) {
    if (r->state == PXL_HTTP_PENDING) return -1;
    r->len = 0;
    r->status_code = 0;
    r->handle = http_get_str(url);
    r->state = (r->handle >= 0) ? PXL_HTTP_PENDING : PXL_HTTP_ERROR;
    return r->state == PXL_HTTP_PENDING ? 0 : -1;
}

// Poll. Call each frame while state == PXL_HTTP_PENDING.
// Returns state after poll (PENDING / DONE / ERROR).
static inline int pxl_http_poll(pxl_http_t *r) {
    if (r->state != PXL_HTTP_PENDING) return r->state;
    int st = http_status(r->handle);
    if (st == HTTP_DONE) {
        r->status_code = http_response_code(r->handle);
        int rlen = http_response_len(r->handle);
        int max = (rlen < r->buf_sz - 1) ? rlen : r->buf_sz - 1;
        r->len = http_read(r->handle, r->buf, max);
        if (r->len > 0) r->buf[r->len] = '\0';
        http_close(r->handle);
        r->state = PXL_HTTP_DONE;
    } else if (st == HTTP_ERROR) {
        http_close(r->handle);
        r->state = PXL_HTTP_ERROR;
    }
    return r->state;
}

// Reset to idle for reuse.
static inline void pxl_http_reset(pxl_http_t *r) {
    r->state = PXL_HTTP_IDLE;
    r->handle = -1;
    r->len = 0;
}

// ============================================================
// Button widget
// ============================================================

typedef struct {
    int x, y, w, h;
    const char *label;
    int label_len;
    unsigned int bg, fg;
    unsigned int pressed_bg, pressed_fg;
    int _prev_inside;
} pxl_button_t;

// Initialize a button. pressed colors default to dimmed bg / brightened fg.
static inline void pxl_button(pxl_button_t *btn, int x, int y, int w, int h,
                               const char *label, unsigned int bg, unsigned int fg) {
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
    btn->label = label;
    btn->label_len = pxl_strlen(label);
    btn->bg = bg;
    btn->fg = fg;
    btn->pressed_bg = dim_color(bg, 0.5f);
    btn->pressed_fg = fg;
    btn->_prev_inside = 0;
}

// Returns 1 if touch point is inside the button bounds.
static inline int _pxl_btn_inside(pxl_button_t *btn) {
    int tx = get_touch_x();
    int ty = get_touch_y();
    return tx >= btn->x && tx < btn->x + btn->w &&
           ty >= btn->y && ty < btn->y + btn->h;
}

// Draw the button with pressed/normal visual state.
static inline void pxl_button_draw(pxl_button_t *btn) {
    int inside = is_pressed() && _pxl_btn_inside(btn);
    unsigned int bg_c = inside ? btn->pressed_bg : btn->bg;
    unsigned int fg_c = inside ? btn->pressed_fg : btn->fg;

    fill_rect(btn->x, btn->y, btn->w, btn->h, bg_c);
    draw_rect(btn->x, btn->y, btn->w, btn->h, fg_c);

    int text_w = btn->label_len * PXL_FONT_W;
    int tx = btn->x + (btn->w - text_w) / 2;
    int ty = btn->y + (btn->h - PXL_FONT_H) / 2;
    draw_text(tx, ty, btn->label, btn->label_len, fg_c);
}

// Returns 1 on release-inside (standard click). Call once per frame.
static inline int pxl_button_clicked(pxl_button_t *btn) {
    int inside = is_pressed() && _pxl_btn_inside(btn);
    int clicked = !inside && btn->_prev_inside;
    btn->_prev_inside = inside;
    return clicked;
}
