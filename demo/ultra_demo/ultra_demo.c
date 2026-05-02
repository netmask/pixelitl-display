#include "pixelitl.h"

// Ultra resolution (200x120) demo — 4x scale, all SRAM

static int W, H;
static float t;

// Starfield
#define NUM_STARS 60
static float sx[NUM_STARS], sy[NUM_STARS], sz[NUM_STARS];

static void reset_star(int i) {
    sx[i] = (float)(rand() % 2000 - 1000);
    sy[i] = (float)(rand() % 2000 - 1000);
    sz[i] = (float)(rand() % 900 + 100);
}

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(2);  // Ultra: 200x120
    W = width();
    H = height();
    t = 0.0f;
    srand((int)millis());

    for (int i = 0; i < NUM_STARS; i++) {
        reset_star(i);
    }
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    t += (float)dt * 0.001f;

    float speed = (float)dt * 0.4f;
    for (int i = 0; i < NUM_STARS; i++) {
        sz[i] -= speed;
        if (sz[i] <= 1.0f) reset_star(i);
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    clear(rgb(2, 2, 8));

    int cx = W / 2;
    int cy = H / 2;

    // === Starfield ===
    for (int i = 0; i < NUM_STARS; i++) {
        float iz = 400.0f / sz[i];
        int px = cx + (int)(sx[i] * iz);
        int py = cy + (int)(sy[i] * iz);

        if (px < 0 || px >= W || py < 0 || py >= H) continue;

        float bri = 1.0f - sz[i] / 1000.0f;
        if (bri < 0.0f) bri = 0.0f;
        unsigned int c = dim_color(rgb(200, 210, 255), bri);
        set_pixel(px, py, c);
    }

    // === Rotating ring of circles ===
    int num_orbs = 6;
    float ring_r = 35.0f;
    for (int i = 0; i < num_orbs; i++) {
        float angle = t * 0.6f + (float)i * 6.2832f / (float)num_orbs;
        int ox = cx + (int)(cosf(angle) * ring_r);
        int oy = cy + (int)(sinf(angle) * ring_r);
        float hue = fmodf((float)i * 60.0f + t * 30.0f, 360.0f);
        unsigned int oc = hsv(hue, 0.8f, 1.0f);

        fill_circle(ox, oy, 6, oc);
        draw_circle(ox, oy, 7, dim_color(oc, 0.3f));
    }

    // === Center pulsing circle ===
    float pulse = sinf(t * 2.5f) * 0.3f + 0.7f;
    int cr = 12 + (int)(sinf(t * 1.5f) * 3.0f);
    unsigned int cc = dim_color(hsv(fmodf(t * 20.0f, 360.0f), 0.5f, 1.0f), pulse);
    fill_circle(cx, cy, cr, cc);

    // === Color gradient bars at bottom ===
    int bar_y = H - 12;
    int seg_w = 5;
    int num_seg = W / seg_w;
    for (int b = 0; b < 3; b++) {
        for (int s = 0; s < num_seg; s++) {
            float hue = fmodf((float)s * 9.0f + t * 40.0f + (float)b * 40.0f, 360.0f);
            fill_rect(s * seg_w, bar_y + b * 3, seg_w, 2, hsv(hue, 0.9f, 0.8f));
        }
    }

    // === Title ===
    draw_str_big(cx - 48, 4, "200x120", rgb(255, 255, 255));
    draw_str(cx - 36, 20, "ULTRA MODE", dim_color(rgb(150, 160, 200), pulse));

    // === Corner markers ===
    unsigned int mark = rgb(255, 80, 80);
    fill_rect(0, 0, 10, 1, mark);
    fill_rect(0, 0, 1, 6, mark);
    fill_rect(W - 10, 0, 10, 1, mark);
    fill_rect(W - 1, 0, 1, 6, mark);
    fill_rect(0, H - 1, 10, 1, mark);
    fill_rect(0, H - 6, 1, 6, mark);
    fill_rect(W - 10, H - 1, 10, 1, mark);
    fill_rect(W - 1, H - 6, 1, 6, mark);
}
