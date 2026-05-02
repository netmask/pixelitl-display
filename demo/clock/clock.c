#include "pixelitl.h"

static int seconds;
static int minutes;
static int hours;

__attribute__((export_name("app_init")))
void app_init(void) {
    seconds = 0;
    minutes = 0;
    hours = 12;
}

// Simple integer-to-string for 2-digit numbers (no libc)
static void int_to_str2(int val, char *buf) {
    buf[0] = '0' + (val / 10);
    buf[1] = '0' + (val % 10);
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    // Tick clock based on millis
    long long ms = millis();
    int total_secs = (int)(ms / 1000);
    seconds = total_secs % 60;
    minutes = (total_secs / 60) % 60;
    hours = (total_secs / 3600) % 24;
    if (hours == 0) hours = 12;
    else if (hours > 12) hours -= 12;
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    unsigned int bg  = rgb(0, 0, 20);
    unsigned int fg  = rgb(255, 255, 255);
    unsigned int dim = rgb(80, 80, 120);
    unsigned int acc = rgb(0, 200, 255);

    clear(bg);

    // Draw time HH:MM:SS in big text, centered
    char time_buf[9];
    int_to_str2(hours, &time_buf[0]);
    time_buf[2] = ':';
    int_to_str2(minutes, &time_buf[3]);
    time_buf[5] = ':';
    int_to_str2(seconds, &time_buf[6]);
    time_buf[8] = '\0';

    // Big text: 12px per char, 8 chars + null = ~96px wide -> center at (80-96)/2 = -8? Use small offset
    draw_text_big(4, 16, time_buf, 8, fg);

    // Draw a label
    char label[] = "PIXELITL";
    draw_text(20, 4, label, 8, acc);

    // Decorative line
    draw_line_h(0, 14, 80, dim);
    draw_line_h(0, 33, 80, dim);

    // Seconds bar at bottom
    int bar_width = (seconds * 80) / 60;
    fill_rect(0, 44, bar_width, 4, acc);

    // Blink colon separator
    if (seconds % 2 == 0) {
        // Dim the colons when even
        draw_text_big(4 + 24, 16, ":", 1, dim);
        draw_text_big(4 + 60, 16, ":", 1, dim);
    }
}
