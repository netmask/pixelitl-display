#include "pixelitl.h"

static float t;

__attribute__((export_name("app_init")))
void app_init(void) {
    t = 0.0f;
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    t += (float)dt * 0.001f;
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    int w = width();
    int h = height();

    // Plasma background
    for (int y = 0; y < h; y += 2) {
        for (int x = 0; x < w; x += 2) {
            float fx = (float)x * 0.15f;
            float fy = (float)y * 0.2f;
            float v1 = sinf(fx + t * 2.0f);
            float v2 = sinf(fy + t * 1.5f);
            float v3 = sinf(fx + fy + t);
            float v4 = sinf(sqrtf(fx * fx + fy * fy) - t * 3.0f);
            float val = (v1 + v2 + v3 + v4) * 0.25f;
            float hue = val * 180.0f + t * 80.0f;
            unsigned int c = hsv(hue, 0.9f, 0.15f);
            fill_rect(x, y, 2, 2, c);
        }
    }

    // "HOLI" - draw each character with its own rainbow offset and wave
    const char *text = "HOLI";
    int nchars = 4;
    int char_w = 11;  // big font ~11px wide
    int total_w = nchars * char_w;
    int base_x = (w - total_w) / 2;
    int base_y = h / 2 - 7;

    for (int i = 0; i < nchars; i++) {
        // Per-character wave offset
        float phase = t * 3.0f + (float)i * 1.2f;
        int wave_y = (int)(sinf(phase) * 5.0f);
        int wave_x = (int)(cosf(phase * 0.7f) * 2.0f);

        // Rainbow hue per character, cycling
        float hue = t * 120.0f + (float)i * 90.0f;

        // Pulsing brightness
        float pulse = sinf(t * 4.0f + (float)i * 0.8f) * 0.3f + 0.7f;

        int cx = base_x + i * char_w + wave_x;
        int cy = base_y + wave_y;

        // Shadow/glow layers
        unsigned int glow1 = hsv(hue + 30.0f, 1.0f, pulse * 0.3f);
        unsigned int glow2 = hsv(hue - 20.0f, 1.0f, pulse * 0.15f);
        char ch[2] = { text[i], 0 };
        draw_text_big(cx - 1, cy + 1, ch, 1, glow2);
        draw_text_big(cx + 1, cy - 1, ch, 1, glow1);

        // Main character
        unsigned int col = hsv(hue, 1.0f, pulse);
        draw_text_big(cx, cy, ch, 1, col);
    }

    // Sparkle particles
    for (int i = 0; i < 8; i++) {
        float angle = t * (2.0f + (float)i * 0.3f) + (float)i * 0.785f;
        float radius = 18.0f + sinf(t * 2.5f + (float)i) * 6.0f;
        int sx = w / 2 + (int)(cosf(angle) * radius);
        int sy = h / 2 + (int)(sinf(angle) * radius * 0.6f);
        if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
            float sparkle_hue = t * 200.0f + (float)i * 45.0f;
            float bri = sinf(t * 6.0f + (float)i * 1.5f);
            if (bri > 0.0f) {
                unsigned int sc = hsv(sparkle_hue, 0.8f, bri);
                set_pixel(sx, sy, sc);
            }
        }
    }
}
