#include "pixelitl.h"

static int frame;
static int page;
static int page_timer;
static float anim_t;  // 0..1 animation progress per page

#define PAGE_DURATION 180  // frames per page (~6 seconds at 30fps)
#define NUM_PAGES 5

__attribute__((export_name("app_init")))
void app_init(void) {
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
    if (g == 1) {
        page_timer = 0;
        page = (page + 1) % NUM_PAGES;
    }
}

// Page 0: Rectangles and lines
static void draw_page_rects(void) {
    clear(rgb(0, 0, 20));

    // Animated rectangles sliding in with easing
    float t1 = ease_out_cubic(anim_t);
    float t2 = ease_out_elastic(anim_t);

    int x1 = (int)(-15.0f + t1 * 17.0f);
    int x2 = (int)(90.0f - t1 * 48.0f);

    fill_rect(x1, 2, 12, 8, rgb(255, 0, 0));
    fill_rect(x1 + 14, 2, 12, 8, rgb(0, 255, 0));
    fill_rect(x2, 2, 12, 8, rgb(0, 0, 255));
    fill_rect(x2 - 14, 2, 12, 8, rgb(255, 255, 0));

    // Outlined rect that bounces in size
    int sz = (int)(4.0f + t2 * 16.0f);
    int cx = 40 - sz / 2;
    int cy = 20 - sz / 2;
    draw_rect(cx, cy, sz, sz, rgb(255, 128, 0));

    // Rainbow horizontal lines with staggered entrance
    int i;
    for (i = 0; i < 8; i++) {
        float delay = (float)i * 0.1f;
        float lt = anim_t - delay;
        if (lt < 0.0f) lt = 0.0f;
        if (lt > 1.0f) lt = 1.0f;
        float et = ease_out_quad(lt);
        int len = (int)(et * 50.0f);
        unsigned int c = hsv((float)(i * 45), 1.0f, 1.0f);
        draw_line_h(2, 30 + i, len, c);
    }

    draw_str(2, 42, "RECTS & LINES", rgb(200, 200, 200));
}

// Page 1: Circles with easing
static void draw_page_circles(void) {
    clear(rgb(0, 0, 0));

    // Pulsing filled circles with ease_out_elastic
    float t = ease_out_elastic(anim_t);
    int r = (int)(t * 12.0f);
    fill_circle(20, 18, r, rgb(80, 0, 0));
    fill_circle(20, 18, r > 3 ? r - 3 : 0, rgb(160, 0, 0));
    fill_circle(20, 18, r > 6 ? r - 6 : 0, rgb(255, 0, 0));

    // Concentric rings that expand with bounce
    int i;
    for (i = 0; i < 5; i++) {
        float delay = (float)i * 0.12f;
        float ct = anim_t - delay;
        if (ct < 0.0f) ct = 0.0f;
        if (ct > 1.0f) ct = 1.0f;
        float bt = ease_out_bounce(ct);
        int cr = (int)(bt * (float)(3 + i * 3));
        unsigned int c = hsv((float)(i * 60 + frame * 2), 1.0f, 1.0f);
        draw_circle(58, 18, cr, c);
    }

    // Orbiting dot
    float angle = anim_t * 6.28f * 3.0f;
    int ox = 40 + (int)(sinf(angle) * 12.0f);
    int oy = 38 + (int)(cosf(angle) * 6.0f);
    fill_circle(ox, oy, 3, hsv(anim_t * 360.0f, 1.0f, 1.0f));

    draw_str(2, 42, "CIRCLES", rgb(200, 200, 200));
}

// Page 2: Text with easing
static void draw_page_text(void) {
    clear(rgb(10, 0, 20));

    // Title drops in from top with bounce
    float bt = ease_out_bounce(anim_t);
    int ty = (int)(-14.0f + bt * 16.0f);
    draw_str_big(2, ty, "HELLO!", rgb(255, 255, 0));

    // Lines of text fade in sequentially
    float t1 = anim_t * 3.0f;
    if (t1 > 1.0f) t1 = 1.0f;
    float t2 = anim_t * 3.0f - 0.5f;
    if (t2 < 0.0f) t2 = 0.0f;
    if (t2 > 1.0f) t2 = 1.0f;
    float t3 = anim_t * 3.0f - 1.0f;
    if (t3 < 0.0f) t3 = 0.0f;
    if (t3 > 1.0f) t3 = 1.0f;

    unsigned int c1 = dim_color(rgb(255, 100, 100), ease_out_quad(t1));
    unsigned int c2 = dim_color(rgb(100, 255, 100), ease_out_quad(t2));
    unsigned int c3 = dim_color(rgb(100, 100, 255), ease_out_quad(t3));

    draw_str(2, 18, "ABCDEFGHIJKLM", c1);
    draw_str(2, 26, "NOPQRSTUVWXYZ", c2);
    draw_str(2, 34, "0123456789!@#", c3);
}

// Page 3: Color gradients
static void draw_page_colors(void) {
    clear(rgb(0, 0, 0));

    // HSV hue sweep that wipes in
    float wipe = ease_in_out_cubic(anim_t);
    int max_x = (int)(wipe * 80.0f);
    int x;
    for (x = 0; x < max_x; x += 2) {
        float hue = (float)x * 4.5f;
        unsigned int c = hsv(hue, 1.0f, 1.0f);
        fill_rect(x, 0, 2, 6, c);
    }

    // Saturation gradient
    float wipe2 = ease_in_out_cubic(anim_t > 0.2f ? (anim_t - 0.2f) * 1.25f : 0.0f);
    if (wipe2 > 1.0f) wipe2 = 1.0f;
    max_x = (int)(wipe2 * 80.0f);
    for (x = 0; x < max_x; x += 2) {
        float sat = (float)x / 80.0f;
        unsigned int c = hsv(200.0f, sat, 1.0f);
        fill_rect(x, 8, 2, 6, c);
    }

    // Value gradient
    float wipe3 = ease_in_out_cubic(anim_t > 0.4f ? (anim_t - 0.4f) * 1.67f : 0.0f);
    if (wipe3 > 1.0f) wipe3 = 1.0f;
    max_x = (int)(wipe3 * 80.0f);
    for (x = 0; x < max_x; x += 2) {
        float val = (float)x / 80.0f;
        unsigned int c = hsv(0.0f, 1.0f, val);
        fill_rect(x, 16, 2, 6, c);
    }

    // lerp_color gradient
    unsigned int ca = rgb(255, 0, 0);
    unsigned int cb = rgb(0, 0, 255);
    float wipe4 = ease_in_out_cubic(anim_t > 0.6f ? (anim_t - 0.6f) * 2.5f : 0.0f);
    if (wipe4 > 1.0f) wipe4 = 1.0f;
    max_x = (int)(wipe4 * 80.0f);
    for (x = 0; x < max_x; x += 2) {
        float t = (float)x / 80.0f;
        unsigned int c = lerp_color(ca, cb, t);
        fill_rect(x, 24, 2, 6, c);
    }

    draw_str(2, 42, "HSV RGB LERP", rgb(200, 200, 200));
}

// Page 4: Easing showcase
static void draw_page_easing(void) {
    clear(rgb(5, 5, 15));

    // Show different easing curves as animated dots
    float t = fmodf(anim_t * 2.0f, 1.0f);  // loop twice per page

    // Labels on left
    draw_str(1, 1, "QUAD", rgb(100, 100, 100));
    draw_str(1, 9, "CUBIC", rgb(100, 100, 100));
    draw_str(1, 17, "ELAST", rgb(100, 100, 100));
    draw_str(1, 25, "BOUNC", rgb(100, 100, 100));

    int left = 32;
    int track_w = 44;

    // Draw tracks
    int i;
    for (i = 0; i < 4; i++) {
        draw_line_h(left, 4 + i * 8, track_w, rgb(40, 40, 40));
    }

    // Animated dots using different easings
    float e0 = ease_in_out_quad(t);
    float e1 = ease_in_out_cubic(t);
    float e2 = ease_out_elastic(t);
    float e3 = ease_out_bounce(t);

    int x0 = left + (int)(e0 * (float)(track_w - 1));
    int x1 = left + (int)(e1 * (float)(track_w - 1));
    int x2 = left + (int)(e2 * (float)(track_w - 1));
    int x3 = left + (int)(e3 * (float)(track_w - 1));

    fill_circle(x0, 4, 2, rgb(255, 80, 80));
    fill_circle(x1, 12, 2, rgb(80, 255, 80));
    fill_circle(x2, 20, 2, rgb(80, 80, 255));
    fill_circle(x3, 28, 2, rgb(255, 255, 80));

    // Bouncing bar at bottom
    float bar_t = ease_out_bounce(t);
    int bar_w = (int)(bar_t * 76.0f);
    fill_rect(2, 36, bar_w, 4, hsv(t * 360.0f, 1.0f, 1.0f));

    draw_str(2, 42, "EASING", rgb(200, 200, 200));
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    switch (page) {
    case 0: draw_page_rects(); break;
    case 1: draw_page_circles(); break;
    case 2: draw_page_text(); break;
    case 3: draw_page_colors(); break;
    case 4: draw_page_easing(); break;
    }
}
