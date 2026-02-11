#include "pixelitl.h"

static pxl_button_t btn_r, btn_g, btn_b, btn_reset;
static int clicks;
static int bg_r, bg_g, bg_b;

__attribute__((export_name("app_init")))
void app_init(void) {
    clicks = 0;
    bg_r = 0;
    bg_g = 0;
    bg_b = 0;

    // 4 buttons across the middle: each 16x10, spaced evenly
    pxl_button(&btn_r,     2, 18, 16, 10, "R", rgb(180, 0, 0), rgb(255, 255, 255));
    pxl_button(&btn_g,    22, 18, 16, 10, "G", rgb(0, 140, 0), rgb(255, 255, 255));
    pxl_button(&btn_b,    42, 18, 16, 10, "B", rgb(0, 0, 180), rgb(255, 255, 255));
    pxl_button(&btn_reset, 62, 18, 16, 10, "X", rgb(80, 80, 80), rgb(255, 255, 255));
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    if (pxl_button_clicked(&btn_r)) {
        bg_r += 40;
        if (bg_r > 255) bg_r = 255;
        clicks++;
    }
    if (pxl_button_clicked(&btn_g)) {
        bg_g += 40;
        if (bg_g > 255) bg_g = 255;
        clicks++;
    }
    if (pxl_button_clicked(&btn_b)) {
        bg_b += 40;
        if (bg_b > 255) bg_b = 255;
        clicks++;
    }
    if (pxl_button_clicked(&btn_reset)) {
        bg_r = 0;
        bg_g = 0;
        bg_b = 0;
        clicks = 0;
    }
}

// Simple int-to-string (no libc). Returns length written.
static int itoa_simple(int val, char *buf, int buf_sz) {
    if (buf_sz < 2) return 0;
    if (val == 0) { buf[0] = '0'; return 1; }
    int neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    char tmp[12];
    int n = 0;
    while (val > 0 && n < 11) {
        tmp[n++] = '0' + (val % 10);
        val /= 10;
    }
    int out = 0;
    if (neg && out < buf_sz) buf[out++] = '-';
    int i;
    for (i = n - 1; i >= 0 && out < buf_sz; i--)
        buf[out++] = tmp[i];
    return out;
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    clear(rgb(bg_r, bg_g, bg_b));

    draw_str(2, 2, "BUTTONS", rgb(255, 255, 255));

    // Click counter
    char cbuf[16];
    int clen = itoa_simple(clicks, cbuf, 16);
    draw_str(50, 2, "TAP:", rgb(200, 200, 200));
    draw_text(74, 2, cbuf, clen, rgb(255, 255, 0));

    // Draw buttons
    pxl_button_draw(&btn_r);
    pxl_button_draw(&btn_g);
    pxl_button_draw(&btn_b);
    pxl_button_draw(&btn_reset);

    // Color values at bottom
    draw_str(2, 34, "R:", rgb(200, 200, 200));
    char rbuf[4]; int rlen = itoa_simple(bg_r, rbuf, 4);
    draw_text(14, 34, rbuf, rlen, rgb(255, 100, 100));

    draw_str(28, 34, "G:", rgb(200, 200, 200));
    char gbuf[4]; int glen = itoa_simple(bg_g, gbuf, 4);
    draw_text(40, 34, gbuf, glen, rgb(100, 255, 100));

    draw_str(54, 34, "B:", rgb(200, 200, 200));
    char bbuf[4]; int blen = itoa_simple(bg_b, bbuf, 4);
    draw_text(66, 34, bbuf, blen, rgb(100, 100, 255));

    draw_str(2, 42, "TAP TO MIX COLOR", rgb(180, 180, 180));
}
