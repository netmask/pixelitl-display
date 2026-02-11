#include "graphics.h"
#include "board.h"
#include "font5x7.h"
#include "simd_ops.h"
#include <string.h>
#include <math.h>

// --- Bounds helpers ---

static inline int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline void safe_pixel(uint16_t *fb, int x, int y, uint16_t color) {
    if (x >= 0 && x < VFB_WIDTH && y >= 0 && y < VFB_HEIGHT) {
        fb[y * VFB_WIDTH + x] = color;
    }
}

// --- Core drawing ---

void gfx_clear(uint16_t *fb, uint16_t color) {
    // SIMD: fill 7680 bytes (80×48 RGB565) with 128-bit stores
    // 7680 bytes = 240 × 32 bytes — perfect for SIMD
    simd_fill16(fb, color, VFB_PIXEL_COUNT * sizeof(uint16_t));
}

void gfx_set_pixel(uint16_t *fb, int x, int y, uint16_t color) {
    safe_pixel(fb, x, y, color);
}

void gfx_fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color) {
    int x0 = clamp(x, 0, VFB_WIDTH);
    int y0 = clamp(y, 0, VFB_HEIGHT);
    int x1 = clamp(x + w, 0, VFB_WIDTH);
    int y1 = clamp(y + h, 0, VFB_HEIGHT);

    for (int ry = y0; ry < y1; ry++) {
        for (int rx = x0; rx < x1; rx++) {
            fb[ry * VFB_WIDTH + rx] = color;
        }
    }
}

void gfx_draw_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color) {
    gfx_draw_line_h(fb, x, y, w, color);
    gfx_draw_line_h(fb, x, y + h - 1, w, color);
    gfx_draw_line_v(fb, x, y, h, color);
    gfx_draw_line_v(fb, x + w - 1, y, h, color);
}

void gfx_draw_line_h(uint16_t *fb, int x, int y, int len, uint16_t color) {
    if (y < 0 || y >= VFB_HEIGHT) return;
    int x0 = clamp(x, 0, VFB_WIDTH);
    int x1 = clamp(x + len, 0, VFB_WIDTH);
    for (int rx = x0; rx < x1; rx++) {
        fb[y * VFB_WIDTH + rx] = color;
    }
}

void gfx_draw_line_v(uint16_t *fb, int x, int y, int len, uint16_t color) {
    if (x < 0 || x >= VFB_WIDTH) return;
    int y0 = clamp(y, 0, VFB_HEIGHT);
    int y1 = clamp(y + len, 0, VFB_HEIGHT);
    for (int ry = y0; ry < y1; ry++) {
        fb[ry * VFB_WIDTH + x] = color;
    }
}

void gfx_draw_circle(uint16_t *fb, int cx, int cy, int r, uint16_t color) {
    // Midpoint circle algorithm (Bresenham)
    int x = r, y = 0;
    int d = 1 - r;
    while (x >= y) {
        safe_pixel(fb, cx + x, cy + y, color);
        safe_pixel(fb, cx - x, cy + y, color);
        safe_pixel(fb, cx + x, cy - y, color);
        safe_pixel(fb, cx - x, cy - y, color);
        safe_pixel(fb, cx + y, cy + x, color);
        safe_pixel(fb, cx - y, cy + x, color);
        safe_pixel(fb, cx + y, cy - x, color);
        safe_pixel(fb, cx - y, cy - x, color);
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void gfx_fill_circle(uint16_t *fb, int cx, int cy, int r, uint16_t color) {
    int x = r, y = 0;
    int d = 1 - r;
    while (x >= y) {
        gfx_draw_line_h(fb, cx - x, cy + y, 2 * x + 1, color);
        gfx_draw_line_h(fb, cx - x, cy - y, 2 * x + 1, color);
        gfx_draw_line_h(fb, cx - y, cy + x, 2 * y + 1, color);
        gfx_draw_line_h(fb, cx - y, cy - x, 2 * y + 1, color);
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

// --- Text ---

int gfx_draw_text(uint16_t *fb, int x, int y, const char *str, int len, uint16_t color) {
    int cx = x;
    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)str[i];
        if (ch >= FONT_FIRST_CHAR && ch <= FONT_LAST_CHAR) {
            const uint8_t *glyph = font_5x7[ch - FONT_FIRST_CHAR];
            for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
                uint8_t bits = glyph[col];
                for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
                    if (bits & (1 << row)) {
                        safe_pixel(fb, cx + col, y + row, color);
                    }
                }
            }
        }
        cx += FONT_CHAR_WIDTH + 1; // 5px + 1px gap
    }
    return cx - x;
}

int gfx_draw_text_big(uint16_t *fb, int x, int y, const char *str, int len, uint16_t color) {
    int cx = x;
    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)str[i];
        if (ch >= FONT_FIRST_CHAR && ch <= FONT_LAST_CHAR) {
            const uint8_t *glyph = font_5x7[ch - FONT_FIRST_CHAR];
            for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
                uint8_t bits = glyph[col];
                for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
                    if (bits & (1 << row)) {
                        int px = cx + col * 2;
                        int py = y + row * 2;
                        safe_pixel(fb, px,     py,     color);
                        safe_pixel(fb, px + 1, py,     color);
                        safe_pixel(fb, px,     py + 1, color);
                        safe_pixel(fb, px + 1, py + 1, color);
                    }
                }
            }
        }
        cx += (FONT_CHAR_WIDTH * 2) + 2; // 10px + 2px gap
    }
    return cx - x;
}

// --- Color utilities ---

uint16_t gfx_rgb(int r, int g, int b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint16_t gfx_hsv(float h, float s, float v) {
    // HSV to RGB conversion, then pack to RGB565
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r, g, b;

    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    int ri = (int)((r + m) * 255.0f);
    int gi = (int)((g + m) * 255.0f);
    int bi = (int)((b + m) * 255.0f);
    return gfx_rgb(ri, gi, bi);
}

uint16_t gfx_dim_color(uint16_t color, float factor) {
    int r = (color >> 11) & 0x1F;
    int g = (color >> 5)  & 0x3F;
    int b =  color        & 0x1F;
    r = (int)(r * factor);
    g = (int)(g * factor);
    b = (int)(b * factor);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint16_t gfx_lerp_color(uint16_t c1, uint16_t c2, float t) {
    int r1 = (c1 >> 11) & 0x1F, r2 = (c2 >> 11) & 0x1F;
    int g1 = (c1 >> 5)  & 0x3F, g2 = (c2 >> 5)  & 0x3F;
    int b1 =  c1        & 0x1F, b2 =  c2        & 0x1F;
    int r = r1 + (int)((r2 - r1) * t);
    int g = g1 + (int)((g2 - g1) * t);
    int b = b1 + (int)((b2 - b1) * t);
    return (uint16_t)((r << 11) | (g << 5) | b);
}
