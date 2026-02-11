#include "pixelitl.h"

static float time_offset;

__attribute__((export_name("app_init")))
void app_init(void) {
    time_offset = 0.0f;
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    time_offset += (float)dt * 0.001f;
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    int w = width();
    int h = height();

    // Render at quarter resolution (4x4 blocks) for interpreter performance
    for (int y = 0; y < h; y += 4) {
        for (int x = 0; x < w; x += 4) {
            float fx = (float)x / (float)w;
            float fy = (float)y / (float)h;

            float v = sinf(fx * 10.0f + time_offset);
            v += sinf(fy * 8.0f + time_offset * 0.7f);

            float hue = fmodf((v + 2.0f) * 90.0f + time_offset * 30.0f, 360.0f);
            unsigned int color = hsv(hue, 1.0f, 1.0f);
            fill_rect(x, y, 4, 4, color);
        }
    }
}
