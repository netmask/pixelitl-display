// Drawing primitives for the 80x48 virtual framebuffer (RGB565)
#pragma once

#include <stdint.h>

// All coordinates are in virtual pixel space (0-79 x, 0-47 y).
// Out-of-bounds writes are silently ignored.

void gfx_clear(uint16_t *fb, uint16_t color);
void gfx_set_pixel(uint16_t *fb, int x, int y, uint16_t color);
void gfx_fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color);
void gfx_draw_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color);
void gfx_draw_line_h(uint16_t *fb, int x, int y, int len, uint16_t color);
void gfx_draw_line_v(uint16_t *fb, int x, int y, int len, uint16_t color);
void gfx_draw_circle(uint16_t *fb, int cx, int cy, int r, uint16_t color);
void gfx_fill_circle(uint16_t *fb, int cx, int cy, int r, uint16_t color);
int  gfx_draw_text(uint16_t *fb, int x, int y, const char *str, int len, uint16_t color);
int  gfx_draw_text_big(uint16_t *fb, int x, int y, const char *str, int len, uint16_t color);

// Color utilities
uint16_t gfx_rgb(int r, int g, int b);
uint16_t gfx_hsv(float h, float s, float v);
uint16_t gfx_dim_color(uint16_t color, float factor);
uint16_t gfx_lerp_color(uint16_t c1, uint16_t c2, float t);
