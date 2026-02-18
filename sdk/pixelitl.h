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
extern void set_resolution(int mode);  // 0=standard (80x48), 1=hd (160x96)

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

// --- Convenience helpers (no libc available) ---

static inline int pxl_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static inline int draw_str(int x, int y, const char *s, unsigned int color) {
    return draw_text(x, y, s, pxl_strlen(s), color);
}

static inline int draw_str_big(int x, int y, const char *s, unsigned int color) {
    return draw_text_big(x, y, s, pxl_strlen(s), color);
}

// HTTP helpers
static inline int http_get_str(const char *url) {
    return http_get(url, pxl_strlen(url));
}

// Face install helpers
static inline int face_install_url(const char *url, const char *name) {
    return face_install(url, pxl_strlen(url), name, pxl_strlen(name));
}

// KV helpers
static inline int kv_set_str(const char *key, const char *value) {
    return kv_set(key, pxl_strlen(key), value, pxl_strlen(value));
}

static inline int kv_get_str(const char *key, char *dst, int max_len) {
    return kv_get(key, pxl_strlen(key), dst, max_len);
}

static inline int kv_del_str(const char *key) {
    return kv_del(key, pxl_strlen(key));
}

// --- Button widget ---

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

    // Center text: small font is 6px wide per char, 7px tall
    int text_w = btn->label_len * 6;
    int tx = btn->x + (btn->w - text_w) / 2;
    int ty = btn->y + (btn->h - 7) / 2;
    draw_text(tx, ty, btn->label, btn->label_len, fg_c);
}

// Returns 1 on release-inside (standard click). Call once per frame.
static inline int pxl_button_clicked(pxl_button_t *btn) {
    int inside = is_pressed() && _pxl_btn_inside(btn);
    int clicked = !inside && btn->_prev_inside;
    btn->_prev_inside = inside;
    return clicked;
}
