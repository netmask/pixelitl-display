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
