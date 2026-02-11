#include "pixelitl.h"

// HD Demo — showcases 160x96 resolution with fine detail
// Draws concentric circles, diagonal lines, gradient fills, and small text
// that benefit from the higher pixel density.

static int frame;
static int page;
static int page_timer;
static float anim_t;

#define PAGE_DURATION 200
#define NUM_PAGES 4

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(1);  // HD mode: 160x96
    frame = 0;
    page = 0;
    page_timer = 0;
    anim_t = 0.0f;
}

__attribute__((export_name("app_update")))
void app_update(int f, int dt) {
    frame = f;
    page_timer++;
    anim_t = (float)page_timer / (float)PAGE_DURATION;
    if (anim_t > 1.0f) anim_t = 1.0f;

    if (page_timer >= PAGE_DURATION) {
        page_timer = 0;
        page = (page + 1) % NUM_PAGES;
    }

    int g = get_gesture();
    if (g == GESTURE_TAP) {
        page_timer = 0;
        page = (page + 1) % NUM_PAGES;
    }
}

// Page 0: Concentric circles — shows precision rings impossible at 80x48
static void draw_page_circles(void) {
    clear(rgb(0, 0, 0));

    int cx = 80, cy = 48;
    int max_rings = (int)(ease_out_cubic(anim_t) * 24.0f);
    int i;
    for (i = 1; i <= max_rings; i++) {
        float hue = fmodf((float)i * 15.0f + (float)frame * 2.0f, 360.0f);
        unsigned int c = hsv(hue, 1.0f, 0.8f);
        draw_circle(cx, cy, i * 2, c);
    }

    draw_str(2, 2, "HD 160x96", rgb(255, 255, 255));
    draw_str(2, 88, "CONCENTRIC RINGS", rgb(150, 150, 150));
}

// Page 1: Fine diagonal lines and crosshatch
static void draw_page_lines(void) {
    clear(rgb(5, 5, 15));

    float t = ease_in_out_quad(anim_t);
    int num_lines = (int)(t * 32.0f);
    int i;

    // Diagonal lines from top-left to bottom-right
    for (i = 0; i < num_lines; i++) {
        float hue = (float)i * 11.25f;
        unsigned int c = hsv(hue, 0.8f, 0.9f);
        int x0 = i * 5;
        int y0 = 0;
        int x1 = 0;
        int y1 = i * 3;
        // Draw diagonal using pixel stepping
        int dx = x1 - x0;
        int dy = y1 - y0;
        int steps = dx < 0 ? -dx : dx;
        if ((dy < 0 ? -dy : dy) > steps) steps = dy < 0 ? -dy : dy;
        if (steps == 0) steps = 1;
        int j;
        for (j = 0; j <= steps; j++) {
            int px = x0 + (dx * j) / steps;
            int py = y0 + (dy * j) / steps;
            set_pixel(px, py, c);
        }
    }

    // Cross-hatch grid
    float t2 = ease_out_quad(anim_t);
    int grid_lines = (int)(t2 * 16.0f);
    for (i = 0; i < grid_lines; i++) {
        unsigned int gc = dim_color(rgb(0, 200, 255), 0.4f);
        draw_line_h(80, 6 + i * 5, 78, gc);
        draw_line_v(82 + i * 5, 4, 78, gc);
    }

    draw_str(2, 88, "DIAGONALS & GRID", rgb(150, 150, 150));
}

// Page 2: Smooth gradient fills
static void draw_page_gradients(void) {
    clear(rgb(0, 0, 0));

    int w = width();
    float t = ease_in_out_cubic(anim_t);
    int max_x = (int)(t * (float)w);

    // Full-screen horizontal HSV gradient — 160 distinct hue steps
    int x, y;
    for (x = 0; x < max_x; x++) {
        float hue = (float)x * 360.0f / (float)w;
        unsigned int c = hsv(hue, 1.0f, 1.0f);
        draw_line_v(x, 0, 20, c);
    }

    // Vertical brightness gradient
    float t2 = ease_in_out_cubic(anim_t > 0.3f ? (anim_t - 0.3f) / 0.7f : 0.0f);
    if (t2 > 1.0f) t2 = 1.0f;
    int max_y = (int)(t2 * 30.0f);
    for (y = 0; y < max_y; y++) {
        float val = (float)y / 30.0f;
        unsigned int c = hsv(220.0f, 1.0f, val);
        draw_line_h(0, 22 + y, w, c);
    }

    // Red-blue lerp
    unsigned int ca = rgb(255, 0, 0);
    unsigned int cb = rgb(0, 0, 255);
    float t3 = ease_in_out_cubic(anim_t > 0.5f ? (anim_t - 0.5f) / 0.5f : 0.0f);
    if (t3 > 1.0f) t3 = 1.0f;
    max_x = (int)(t3 * (float)w);
    for (x = 0; x < max_x; x++) {
        float lt = (float)x / (float)w;
        unsigned int c = lerp_color(ca, cb, lt);
        draw_line_v(x, 54, 20, c);
    }

    // 2D gradient patch — shows both axes
    float t4 = ease_out_quad(anim_t > 0.6f ? (anim_t - 0.6f) / 0.4f : 0.0f);
    if (t4 > 1.0f) t4 = 1.0f;
    int patch_w = (int)(t4 * 60.0f);
    for (y = 0; y < 12; y++) {
        for (x = 0; x < patch_w; x++) {
            float hue = (float)x * 6.0f;
            float sat = (float)y / 12.0f;
            unsigned int c = hsv(hue, sat, 1.0f);
            set_pixel(50 + x, 76 + y, c);
        }
    }

    draw_str(2, 88, "160-STEP GRADIENTS", rgb(150, 150, 150));
}

// Page 3: Dense small text — showcases readability at 5x7 on HD
static void draw_page_text(void) {
    clear(rgb(0, 0, 10));

    float t = ease_out_cubic(anim_t);
    int lines = (int)(t * 12.0f);
    int i;

    // Header in large text
    draw_str_big(4, 2, "HD MODE", hsv(fmodf((float)frame * 3.0f, 360.0f), 1.0f, 1.0f));

    // Many lines of small text — visible thanks to 160 pixels wide
    const char *text_lines[] = {
        "The quick brown fox jumps",
        "over the lazy dog. 0123456",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "abcdefghijklmnopqrstuvwxyz",
        "!@#$%^&*()-=_+[]{}|;':\"",
        "160x96 pixels at 5x scale",
        "Double the standard 80x48",
        "Fine detail, smooth curves",
        "Tiny text still readable!",
        "Pixel art in high def :)",
        "4x more pixels per frame",
        "Half the pixel size on LCD",
    };

    for (i = 0; i < lines && i < 12; i++) {
        float delay = (float)i * 0.06f;
        float lt = anim_t - delay;
        if (lt < 0.0f) lt = 0.0f;
        if (lt > 1.0f) lt = 1.0f;
        float alpha = ease_out_quad(lt);
        unsigned int c = dim_color(rgb(200, 220, 255), alpha);
        draw_str(2, 20 + i * 6, text_lines[i], c);
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    switch (page) {
    case 0: draw_page_circles(); break;
    case 1: draw_page_lines(); break;
    case 2: draw_page_gradients(); break;
    case 3: draw_page_text(); break;
    }
}
