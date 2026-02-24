#include "pixelitl.h"

// ===== RETRO GLUCOSE MONITOR =====
// CRT phosphor oscilloscope aesthetic — 200x120 ultra
// Seven-segment digits, phosphor trace, grid dots, CRT frame

#define POLL_MS  60000
#define HIST_MAX 50

// ---- State ----
static float t;
static int http_h;
static int poll_timer;
static int wifi_ok;
static char server_url[160];
static char resp_buf[256];

// ---- Glucose data ----
static int g_value, g_trend, g_status, g_valid, g_update_ms;
static int hist[HIST_MAX];
static int hist_n;

// ---- Seven-segment display ----
// Digit: 14w x 22h, 2px thick segments
// Bits: A(6) B(5) C(4) D(3) E(2) F(1) G(0)
static const unsigned char SEGS[] = {
    0x7E, 0x30, 0x6D, 0x79, 0x33,
    0x5B, 0x5F, 0x70, 0x7F, 0x7B
};

static void draw_7seg(int x, int y, int d, unsigned int on, unsigned int off) {
    if (d < 0 || d > 9) return;
    unsigned char s = SEGS[d];
    fill_rect(x+2,  y,     10, 2, (s & 0x40) ? on : off);  // A top
    fill_rect(x+12, y+2,   2,  8, (s & 0x20) ? on : off);  // B upper-right
    fill_rect(x+12, y+12,  2,  8, (s & 0x10) ? on : off);  // C lower-right
    fill_rect(x+2,  y+20,  10, 2, (s & 0x08) ? on : off);  // D bottom
    fill_rect(x,    y+12,  2,  8, (s & 0x04) ? on : off);  // E lower-left
    fill_rect(x,    y+2,   2,  8, (s & 0x02) ? on : off);  // F upper-left
    fill_rect(x+2,  y+10,  10, 2, (s & 0x01) ? on : off);  // G middle
}

// ---- JSON parsing ----
static int json_int(const char *js, const char *key) {
    int kl = pxl_strlen(key);
    for (const char *p = js; *p; p++) {
        if (*p == '"') {
            p++;
            int ok = 1;
            for (int i = 0; i < kl; i++) { if (p[i] != key[i]) { ok = 0; break; } }
            if (ok && p[kl] == '"') {
                p += kl + 1;
                while (*p == ':' || *p == ' ') p++;
                int neg = 0, val = 0;
                if (*p == '-') { neg = 1; p++; }
                while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; }
                return neg ? -val : val;
            }
        }
    }
    return -1;
}

static void start_fetch(void) {
    if (http_h >= 0) return;
    http_h = http_get_str(server_url);
}

// ---- Color helpers ----
static unsigned int gcolor(int v) {
    if (v <= 0)    return rgb(0, 60, 40);
    if (v < 54)    return rgb(255, 30, 30);      // critical low — red
    if (v < 70)    return rgb(80, 200, 255);      // low — cyan
    if (v <= 180)  return rgb(0, 255, 80);        // in range — phosphor green
    if (v <= 250)  return rgb(255, 200, 40);      // high — amber
    return rgb(255, 50, 40);                       // very high — red
}

// ---- Oscilloscope grid ----
static void draw_scope_grid(int gx, int gy, int gw, int gh) {
    unsigned int dot_c = rgb(0, 22, 12);

    // Horizontal gridlines at value markers
    int vmin = 40, vmax = 300;
    int markers[] = {70, 120, 180, 250};
    for (int m = 0; m < 4; m++) {
        int my = gy + gh - 1 - ((markers[m] - vmin) * (gh - 1)) / (vmax - vmin);
        if (my < gy || my > gy + gh - 1) continue;
        for (int x = gx; x < gx + gw; x += 5) {
            set_pixel(x, my, dot_c);
        }
    }

    // Vertical gridlines every 24px
    for (int x = gx + 24; x < gx + gw; x += 24) {
        for (int y = gy; y < gy + gh; y += 5) {
            set_pixel(x, y, dot_c);
        }
    }
}

// ---- Phosphor trace with glow + persistence ----
static void draw_trace(int gx, int gy, int gw, int gh) {
    if (hist_n < 1) return;

    int vmin = 40, vmax = 300;
    unsigned int phosphor = rgb(0, 255, 80);

    // Range lines — dashed, dim
    int y180 = gy + gh - 1 - ((180 - vmin) * (gh - 1)) / (vmax - vmin);
    int y70  = gy + gh - 1 - ((70 - vmin) * (gh - 1)) / (vmax - vmin);
    if (y180 >= gy && y180 <= gy + gh - 1) {
        for (int x = gx; x < gx + gw; x += 6) {
            set_pixel(x, y180, rgb(0, 45, 25));
            set_pixel(x + 1, y180, rgb(0, 45, 25));
        }
        draw_str(gx + 1, y180 - 8, "180", rgb(0, 40, 22));
    }
    if (y70 >= gy && y70 <= gy + gh - 1) {
        for (int x = gx; x < gx + gw; x += 6) {
            set_pixel(x, y70, rgb(0, 45, 25));
            set_pixel(x + 1, y70, rgb(0, 45, 25));
        }
        draw_str(gx + 1, y70 + 2, "70", rgb(0, 40, 22));
    }

    // In-range zone — faint tint (sparse dots)
    if (y180 >= gy && y70 <= gy + gh - 1) {
        for (int y = y180 + 1; y < y70; y += 2) {
            for (int x = gx; x < gx + gw; x += 6) {
                set_pixel(x, y, rgb(0, 8, 4));
            }
        }
    }

    // Plot trace
    float step = (float)(gw - 4) / (float)(hist_n > 1 ? hist_n - 1 : 1);
    int prev_px = -1, prev_py = -1;

    for (int i = 0; i < hist_n; i++) {
        int px = gx + 2 + (int)((float)i * step);
        int val = hist[i];
        int py = gy + gh - 1 - ((val - vmin) * (gh - 1)) / (vmax - vmin);
        if (py < gy) py = gy;
        if (py > gy + gh - 1) py = gy + gh - 1;

        // Phosphor persistence: older readings fade
        float age = (hist_n > 1) ? (float)i / (float)(hist_n - 1) : 1.0f;
        float brightness = 0.25f + age * 0.75f;
        unsigned int tc = dim_color(phosphor, brightness);

        // Connecting line
        if (prev_px >= 0) {
            draw_line(prev_px, prev_py, px, py, dim_color(tc, 0.55f));
        }

        // 1px bloom/glow around each point
        unsigned int glow_c = dim_color(tc, 0.2f);
        if (px > gx) set_pixel(px - 1, py, glow_c);
        if (px < gx + gw - 1) set_pixel(px + 1, py, glow_c);
        if (py > gy) set_pixel(px, py - 1, glow_c);
        if (py < gy + gh - 1) set_pixel(px, py + 1, glow_c);

        // Bright center
        set_pixel(px, py, tc);

        // Latest point — pulsing crosshair
        if (i == hist_n - 1) {
            float pulse = sinf(t * 4.0f) * 0.3f + 0.7f;
            unsigned int pc = dim_color(phosphor, pulse);
            set_pixel(px, py, pc);
            // Inner glow
            unsigned int ig = dim_color(pc, 0.5f);
            set_pixel(px-1,py,ig); set_pixel(px+1,py,ig);
            set_pixel(px,py-1,ig); set_pixel(px,py+1,ig);
            // Outer glow
            unsigned int og = dim_color(pc, 0.15f);
            set_pixel(px-2,py,og); set_pixel(px+2,py,og);
            set_pixel(px,py-2,og); set_pixel(px,py+2,og);
            set_pixel(px-1,py-1,og); set_pixel(px+1,py-1,og);
            set_pixel(px-1,py+1,og); set_pixel(px+1,py+1,og);
            // Crosshair lines (short)
            for (int cx = px - 5; cx <= px + 5; cx++) {
                if (cx >= gx && cx < gx + gw && cx != px)
                    set_pixel(cx, py, dim_color(pc, 0.08f));
            }
            for (int cy = py - 5; cy <= py + 5; cy++) {
                if (cy >= gy && cy < gy + gh && cy != py)
                    set_pixel(px, cy, dim_color(pc, 0.08f));
            }
        }

        prev_px = px;
        prev_py = py;
    }
}

// ---- Decorative EKG blip at bottom ----
static void draw_ekg_line(int lx, int ly, int lw) {
    unsigned int lc = dim_color(rgb(0, 255, 80), 0.2f);
    // Flat baseline
    draw_line_h(lx, ly, lw, dim_color(lc, 0.5f));
    // Animated blip position
    int blip_x = lx + ((int)(t * 30.0f) % lw);
    // Draw spike: small QRS-like waveform
    if (blip_x > lx + 5 && blip_x < lx + lw - 10) {
        set_pixel(blip_x - 2, ly - 1, lc);
        set_pixel(blip_x - 1, ly - 2, lc);
        set_pixel(blip_x,     ly - 5, dim_color(lc, 2.0f));
        set_pixel(blip_x + 1, ly + 2, lc);
        set_pixel(blip_x + 2, ly - 1, lc);
        set_pixel(blip_x + 3, ly, lc);
    }
}

// ======== EXPORTS ========

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(2);
    t = 0.0f;
    srand((int)millis());

    int len = kv_get_str("glucose_url", server_url, sizeof(server_url));
    if (len <= 0)
        pxl_strcpy(server_url, sizeof(server_url),
            "https://glucose.garay.mx/api/glucose/esp32?token=sugaryesp32123");

    g_valid = 0; g_value = 0; g_trend = 4; g_status = 1;
    hist_n = 0; poll_timer = 0; g_update_ms = 0;
    wifi_ok = 0; http_h = -1;
}

__attribute__((export_name("app_wifi_ready")))
void app_wifi_ready(void) {
    wifi_ok = 1;
    start_fetch();
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    t += (float)dt * 0.001f;
    if (!wifi_ok) return;

    poll_timer += dt;
    if (poll_timer >= POLL_MS && http_h < 0) {
        poll_timer = 0;
        start_fetch();
    }

    if (http_h >= 0) {
        int st = http_status(http_h);
        if (st == HTTP_DONE) {
            int resp = http_response_code(http_h);
            if (resp == 200) {
                int rlen = http_read(http_h, resp_buf, sizeof(resp_buf) - 1);
                if (rlen > 0) {
                    resp_buf[rlen] = '\0';
                    int v = json_int(resp_buf, "v");
                    int tr = json_int(resp_buf, "t");
                    int s = json_int(resp_buf, "s");
                    if (v > 0) {
                        g_value = v;
                        g_trend = (tr >= 1 && tr <= 7) ? tr : 4;
                        g_status = s;
                        g_valid = 1;
                        g_update_ms = (int)millis();
                        if (hist_n < HIST_MAX) {
                            hist[hist_n++] = v;
                        } else {
                            for (int i = 0; i < HIST_MAX - 1; i++) hist[i] = hist[i + 1];
                            hist[HIST_MAX - 1] = v;
                        }
                    }
                }
            }
            http_close(http_h); http_h = -1;
        } else if (st == HTTP_ERROR) {
            http_close(http_h); http_h = -1;
        }
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    clear(rgb(1, 2, 1));

    unsigned int gc = gcolor(g_value);
    unsigned int phosphor = rgb(0, 255, 80);
    unsigned int frame_c = rgb(10, 25, 15);
    unsigned int bracket_c = dim_color(phosphor, 0.35f);

    // ======== CRT FRAME ========
    // Outer border
    draw_line_h(2, 0, 196, frame_c);
    draw_line_h(2, 119, 196, frame_c);
    draw_line_v(0, 2, 116, frame_c);
    draw_line_v(199, 2, 116, frame_c);
    // Rounded corners (cut pixels)
    set_pixel(1, 0, frame_c); set_pixel(0, 1, frame_c);
    set_pixel(198, 0, frame_c); set_pixel(199, 1, frame_c);
    set_pixel(1, 119, frame_c); set_pixel(0, 118, frame_c);
    set_pixel(198, 119, frame_c); set_pixel(199, 118, frame_c);

    // Corner brackets
    draw_line_h(3, 2, 12, bracket_c); draw_line_v(3, 2, 8, bracket_c);
    draw_line_h(185, 2, 12, bracket_c); draw_line_v(196, 2, 8, bracket_c);
    draw_line_h(3, 117, 12, bracket_c); draw_line_v(3, 110, 8, bracket_c);
    draw_line_h(185, 117, 12, bracket_c); draw_line_v(196, 110, 8, bracket_c);

    // ======== TITLE BAR (Y 2-8) ========
    draw_str(16, 3, "GLUCOSE MONITOR", dim_color(phosphor, 0.45f));

    // Status indicator
    if (http_h >= 0) {
        // Fetching — spinning dots
        int dot = ((int)(t * 6.0f)) % 3;
        for (int d = 0; d < 3; d++) {
            float br = (d == dot) ? 0.7f : 0.1f;
            fill_rect(180 + d * 4, 4, 2, 2, dim_color(phosphor, br));
        }
    } else if (g_valid) {
        float gb = sinf(t * 1.5f) * 0.15f + 0.75f;
        fill_rect(188, 4, 3, 3, dim_color(phosphor, gb));
    } else {
        fill_rect(188, 4, 3, 3, dim_color(phosphor, 0.1f));
    }

    // Accent line
    for (int x = 3; x < 197; x++) {
        float xf = (float)(x - 3) / 194.0f;
        float wave = sinf(xf * 12.56f + t * 1.5f) * 0.03f;
        set_pixel(x, 9, dim_color(phosphor, 0.12f + wave));
    }

    // ======== PRE-DATA STATES ========
    if (!wifi_ok) {
        float p = sinf(t * 3.0f) * 0.3f + 0.7f;
        draw_str_centered(0, 200, 45, "CONNECTING", dim_color(phosphor, p));
        // Animated arcs
        for (int r = 5; r <= 15; r += 5) {
            float a = sinf(t * 2.5f + (float)r * 0.4f) * 0.3f + 0.4f;
            draw_circle(100, 68, r, dim_color(phosphor, a * 0.25f));
        }
        // EKG flatline
        draw_ekg_line(6, 115, 188);
        return;
    }
    if (!g_valid) {
        float p = sinf(t * 2.0f) * 0.3f + 0.7f;
        draw_str_centered(0, 200, 50, "ACQUIRING SIGNAL", dim_color(phosphor, p));
        int nd = ((int)(t * 3.0f)) % 4;
        char dots[5];
        for (int d = 0; d < nd; d++) dots[d] = '.';
        dots[nd] = '\0';
        draw_str_centered(0, 200, 60, dots, dim_color(phosphor, 0.5f));
        draw_ekg_line(6, 115, 188);
        return;
    }

    // ======== OSCILLOSCOPE AREA (Y 10-104) ========
    int sx = 4, sy = 10, sw = 192, sh = 95;

    // Scope border — subtle
    draw_rect(sx - 1, sy - 1, sw + 2, sh + 2, dim_color(phosphor, 0.06f));

    // Grid
    draw_scope_grid(sx, sy, sw, sh);

    // Phosphor trace
    draw_trace(sx, sy, sw, sh);

    // ======== SEVEN-SEGMENT PANEL (overlaid top-left) ========
    int panel_x = sx + 3, panel_y = sy + 2;
    int panel_w = 62, panel_h = 38;

    // Dark backing (so digits are readable over trace)
    fill_rect(panel_x, panel_y, panel_w, panel_h, rgb(1, 3, 2));
    draw_rect(panel_x, panel_y, panel_w, panel_h, dim_color(phosphor, 0.05f));

    // Value digits
    float pulse = 1.0f;
    if (g_value < 70 || g_value > 250)
        pulse = 0.55f + sinf(t * 5.0f) * 0.45f;

    unsigned int d_on = dim_color(gc, pulse);
    unsigned int d_off = dim_color(gc, 0.04f);

    char vbuf[8];
    int vlen = pxl_itoa(g_value, vbuf, sizeof(vbuf));

    int dx = panel_x + 4;
    int dy = panel_y + 3;
    for (int i = 0; i < vlen && i < 3; i++) {
        draw_7seg(dx + i * 17, dy, vbuf[i] - '0', d_on, d_off);
    }

    // mg/dL label
    draw_str(dx + 1, dy + 25, "mg/dL", dim_color(phosphor, 0.25f));

    // ======== STATUS (overlaid top-right of scope) ========
    // Trend text
    const char *tnames[] = {"", "RISE ++", "RISING", "RISE +",
                            "STEADY", "FALL +", "FALLING", "FALL ++"};
    if (g_trend >= 1 && g_trend <= 7) {
        int tl = pxl_strlen(tnames[g_trend]);
        draw_str(sx + sw - tl * PXL_FONT_W - 4, sy + 3, tnames[g_trend],
                 dim_color(gc, 0.55f));
    }

    // Status label
    const char *stxt;
    if (g_value < 54)       stxt = "! CRITICAL";
    else if (g_value < 70)  stxt = "LOW";
    else if (g_value <= 180) stxt = "IN RANGE";
    else if (g_value <= 250) stxt = "HIGH";
    else                     stxt = "! CRITICAL";

    int sl = pxl_strlen(stxt);
    draw_str(sx + sw - sl * PXL_FONT_W - 4, sy + 13, stxt, dim_color(gc, 0.45f));

    // Time since update (top-right, below status)
    if (g_update_ms > 0) {
        int elapsed = ((int)millis() - g_update_ms) / 1000;
        int mins = elapsed / 60;
        char tbuf[16];
        int tl;
        if (mins > 0) {
            tl = pxl_itoa(mins, tbuf, sizeof(tbuf));
            tbuf[tl++] = 'm';
        } else {
            tl = pxl_itoa(elapsed, tbuf, sizeof(tbuf));
            tbuf[tl++] = 's';
        }
        tbuf[tl++] = ' '; tbuf[tl++] = 'A'; tbuf[tl++] = 'G';
        tbuf[tl++] = 'O'; tbuf[tl] = '\0';
        draw_str(sx + sw - tl * PXL_FONT_W - 4, sy + 23, tbuf,
                 dim_color(phosphor, 0.3f));
    }

    // ======== BOTTOM BAR (Y 106-117) ========
    // Accent line
    for (int x = 3; x < 197; x++) {
        set_pixel(x, 106, dim_color(phosphor, 0.1f));
    }

    // Range bar (visual: where value sits in 40-300)
    int bar_x = 6, bar_w = 188;
    fill_rect(bar_x, 109, bar_w, 3, rgb(2, 5, 3));

    // Zone coloring
    int low_end  = bar_x + (30 * bar_w) / 260;   // 40-70
    int high_start = bar_x + (140 * bar_w) / 260; // 180+
    fill_rect(bar_x, 109, low_end - bar_x, 3, rgb(5, 10, 15));          // low zone — blue tint
    fill_rect(low_end, 109, high_start - low_end, 3, rgb(2, 10, 5));    // in-range — green tint
    fill_rect(high_start, 109, bar_x + bar_w - high_start, 3, rgb(12, 8, 2)); // high zone — amber tint

    // Current value marker
    int mk_pos = bar_x + ((g_value - 40) * bar_w) / 260;
    if (mk_pos < bar_x) mk_pos = bar_x;
    if (mk_pos > bar_x + bar_w - 2) mk_pos = bar_x + bar_w - 2;
    fill_rect(mk_pos - 1, 108, 3, 5, dim_color(gc, pulse * 0.7f));
    // Marker glow
    set_pixel(mk_pos - 2, 110, dim_color(gc, pulse * 0.15f));
    set_pixel(mk_pos + 2, 110, dim_color(gc, pulse * 0.15f));

    // Zone labels
    draw_str(bar_x, 113, "40", dim_color(phosphor, 0.15f));
    draw_str(low_end - 4, 113, "70", dim_color(phosphor, 0.15f));
    draw_str(high_start - 6, 113, "180", dim_color(phosphor, 0.15f));
    draw_str(bar_x + bar_w - 18, 113, "300", dim_color(phosphor, 0.15f));

    // ======== EKG DECORATION (Y 115) ========
    draw_ekg_line(6, 115, 188);

    // ======== CRITICAL ALERT ========
    if (g_value < 54 || g_value > 300) {
        float flash = sinf(t * 6.0f);
        if (flash > 0.3f) {
            unsigned int alert = dim_color(gc, 0.5f);
            draw_line_h(3, 1, 194, alert);
            draw_line_h(3, 118, 194, alert);
            draw_line_v(1, 3, 114, alert);
            draw_line_v(198, 3, 114, alert);
        }
    }
}
