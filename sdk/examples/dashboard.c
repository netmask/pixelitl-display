#include "pixelitl.h"
#include "dota_sprite.h"

// Masked blit — skip palette index 0 (transparent). Color is RGB565 from the
// sprite palette, dimmed uniformly so the whole sprite fades with the alert.
static void blit_sprite(int dx, int dy, const unsigned char *data,
                        const unsigned short *pal, int w, int h, float alpha) {
    int vw = width(), vh = height();
    for (int row = 0; row < h; row++) {
        int py = dy + row;
        if (py < 0 || py >= vh) continue;
        for (int col = 0; col < w; col++) {
            unsigned char idx = data[row * w + col];
            if (idx == 0) continue;
            int px = dx + col;
            if (px < 0 || px >= vw) continue;
            set_pixel(px, py, dim_color(pal[idx], alpha));
        }
    }
}

// ============================================================
// PIXELITL DASHBOARD — glucose + Hacker News + DOTA alert
// Ultra 200x120
// ============================================================

// ---- Layout ----
#define SW        200
#define SH        120
#define GLU_H     30
#define BOT_H     18
#define HN_Y      (GLU_H + 2)
#define HN_H      (SH - GLU_H - BOT_H - 4)
#define HN_ROW_H  (HN_H / 3)

// ---- Timings ----
#define GLU_POLL_MS        60000
#define HN_POLL_MS        600000     // 10 min
#define DOTA_POLL_MS        5000     // poll the proxy's /dota/status every 5s
#define STALE_MS         (15 * 60 * 1000)
#define DOTA_ALERT_MS      12000
#define HN_PAGE_MS          6000     // dwell 6s per story (≈60s full cycle of 10)
#define HN_MAX              10       // stories pulled from the proxy

// ---- Colors (Wii U–ish pastel, dark-on-light) ----
// Initialized in app_init(); rgb() is a host call, can't use in static inits.
static unsigned int C_BG, C_PANEL, C_BORDER, C_BORDER_SOFT;
static unsigned int C_TEXT, C_TEXT_DIM, C_SEPARATOR;
static unsigned int C_HDR_GLU, C_HDR_HN;
static unsigned int C_ACCENT;        // muted orange for ranks/scores
static unsigned int C_WIFI_OK, C_WIFI_BAD;
static unsigned int C_ACTIVITY;
static unsigned int C_MGDL_DIM;

// ---- HN state machine ----
// Uses Algolia's HN Search API — single request returns front_page with title+points
// inline, no per-item fetch. Much more robust than Firebase from embedded TLS clients.
#define HN_IDLE   0
#define HN_FETCH  1
#define HN_DONE   2
#define HN_ERROR  3

// ---- State ----
static float t;
static int wifi_ok;

// Glucose
static char glu_url[160];
static char glu_resp[256];
static int  glu_http = -1;
static int  glu_timer;
static int  glu_value;
static int  glu_trend;
static int  glu_valid;
static int  glu_update_ms;

// Proxy URL — the pixelitl-service endpoint. All HN + Dota traffic goes here.
static char proxy_url[128];

// Hacker News (via local proxy)
static char hn_buf[4096];
static int  hn_http = -1;
static int  hn_timer;
static int  hn_state = HN_IDLE;
static char hn_summaries[HN_MAX][128];  // GPT-written sentence (≤110 chars + slack)
static int  hn_summary_lens[HN_MAX];
static int  hn_scores[HN_MAX];
static int  hn_valid;
static int  hn_count;
static int  hn_page;
static int  hn_page_timer;

// Diagnostics — shown on-screen when the panel is in HN_ERROR so we can see
// exactly where the fetch failed without needing serial logs.
static int hn_dbg_http_status;   // last value from http_status()
static int hn_dbg_response_code; // HTTP status code (e.g. 200, 403)
static int hn_dbg_read_len;      // bytes read
static int hn_dbg_parse_n;       // hits parsed

// Dota — poll proxy's /dota/status, trigger alert when true, then POST /dota/clear
static int  dota_clear_http = -1;

// Per-HN-story sprite cache — lazy-fetched from /image/sprite/hn/<index> as binary.
#define ICON_W          48
#define ICON_H          48
#define ICON_PAL_MAX    16

#define SPR_ABSENT   0
#define SPR_FETCHING 1
#define SPR_READY    2
#define SPR_ERROR    3

static unsigned short icon_pal[HN_MAX][ICON_PAL_MAX];
static unsigned char  icon_data[HN_MAX][ICON_W * ICON_H];
static unsigned char  icon_pal_n[HN_MAX];
static unsigned char  icon_state[HN_MAX];
static int            icon_http = -1;
static int            icon_fetching_idx = -1;
// Diagnostics from the most recent fetch, surfaced in the placeholder:
static int            icon_dbg_rl = -999;   // bytes read (or -1 on error)
static int            icon_dbg_w = 0, icon_dbg_h = 0, icon_dbg_n = 0;
static int            icon_dbg_rc = -1;     // HTTP response code
static int            icon_retry_timer = 0; // allow SPR_ERROR retry after cooldown
// Binary response buffer: 2+2+2 + 16*2 + 48*48 = 2342 bytes. Round up.
static unsigned char  icon_buf[2500];

// Dota
static char dota_url[160];
static char dota_resp[128];
static int  dota_http = -1;
static int  dota_timer;
static int  dota_alert_active;
static int  dota_alert_elapsed;
static int  dota_has_endpoint;

static pxl_button_t btn_test_dota;

// ============================================================
// JSON helpers
// ============================================================

// Both helpers scan for "key" then inspect the value — if the value isn't of
// the requested type (e.g. "title":{...} when we wanted a string), they keep
// scanning instead of bailing. This lets them skip nested key collisions.
static int json_int(const char *js, const char *key) {
    int kl = pxl_strlen(key);
    const char *p = js;
    while (*p) {
        if (*p != '"') { p++; continue; }
        p++;
        int ok = 1;
        for (int i = 0; i < kl; i++) { if (p[i] != key[i]) { ok = 0; break; } }
        if (!ok || p[kl] != '"') continue;
        const char *q = p + kl + 1;
        while (*q == ':' || *q == ' ' || *q == '\t' || *q == '\n') q++;
        int neg = 0, val = 0, digits = 0;
        if (*q == '-') { neg = 1; q++; }
        while (*q >= '0' && *q <= '9') { val = val * 10 + (*q - '0'); q++; digits = 1; }
        if (digits) return neg ? -val : val;
        // Not a number at this key occurrence (e.g. nested object) — keep looking
    }
    return -1;
}

static int json_str(const char *js, const char *key, char *out, int out_sz) {
    int kl = pxl_strlen(key);
    const char *p = js;
    while (*p) {
        if (*p != '"') { p++; continue; }
        p++;
        int ok = 1;
        for (int i = 0; i < kl; i++) { if (p[i] != key[i]) { ok = 0; break; } }
        if (!ok || p[kl] != '"') continue;
        const char *q = p + kl + 1;
        while (*q == ':' || *q == ' ' || *q == '\t' || *q == '\n') q++;
        if (*q != '"') continue;   // value isn't a string; keep searching
        q++;
        int n = 0;
        while (*q && n < out_sz - 1) {
            if (*q == '\\') {
                q++;
                if (*q == '"')       { out[n++] = '"'; q++; }
                else if (*q == '\\') { out[n++] = '\\'; q++; }
                else if (*q == '/')  { out[n++] = '/'; q++; }
                else if (*q == 'n' || *q == 'r' || *q == 't') { out[n++] = ' '; q++; }
                else if (*q == 'u') {
                    q++;
                    for (int i = 0; i < 4 && *q; i++) q++;
                    out[n++] = '?';
                }
                else { if (*q) q++; }
            } else if (*q == '"') {
                break;
            } else {
                unsigned char c = (unsigned char)*q;
                out[n++] = (c >= 32 && c <= 126) ? *q : '?';
                q++;
            }
        }
        out[n] = '\0';
        return n;
    }
    return 0;
}

// Parse the proxy's /hn response — shape: {"stories":[{"title":"..","summary":"..","points":123}, ...]}
// Scans the "stories" array object-by-object (bracket-matched, string-aware) and
// extracts summary + points per story. Falls back to title when summary missing.
static int parse_proxy_hn(const char *js, int max,
                          char summaries[][128], int *summary_lens, int *scores) {
    const char *p = js;
    // Locate "stories":[
    while (*p) {
        if (p[0] == '"' && p[1] == 's' && p[2] == 't' && p[3] == 'o' &&
            p[4] == 'r' && p[5] == 'i' && p[6] == 'e' && p[7] == 's' && p[8] == '"') {
            p += 9;
            while (*p && *p != '[') p++;
            if (*p != '[') return 0;
            p++;
            break;
        }
        p++;
    }
    if (!*p) return 0;

    int count = 0;
    while (*p && count < max) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (*p == ']' || !*p) break;
        if (*p != '{') break;

        // Find matching closing brace, string-aware
        const char *start = p;
        int depth = 0, in_str = 0;
        while (*p) {
            if (in_str) {
                if (*p == '\\' && p[1]) p++;
                else if (*p == '"') in_str = 0;
            } else {
                if (*p == '"') in_str = 1;
                else if (*p == '{') depth++;
                else if (*p == '}') { depth--; if (depth == 0) { p++; break; } }
            }
            p++;
        }

        char slice[512];
        int hit_len = (int)(p - start);
        int slen = hit_len < (int)sizeof(slice) - 1 ? hit_len : (int)sizeof(slice) - 1;
        pxl_memcpy(slice, start, slen);
        slice[slen] = '\0';

        int sl = json_str(slice, "summary", summaries[count], 128);
        if (sl <= 0) {
            // No summary — fall back to title
            sl = json_str(slice, "title", summaries[count], 128);
        }
        int sc = json_int(slice, "points");
        summary_lens[count] = (sl > 0) ? sl : 0;
        scores[count] = (sc > 0) ? sc : 0;
        count++;
    }
    return count;
}

// ============================================================
// Glucose fetch
// ============================================================

static void glu_start(void) {
    if (glu_http >= 0) return;
    glu_http = http_get_str(glu_url);
}

static void glu_poll(int dt) {
    if (!wifi_ok) return;
    glu_timer += dt;
    if (glu_http < 0 && glu_timer >= GLU_POLL_MS) {
        glu_timer = 0;
        glu_start();
    }
    if (glu_http < 0) return;
    int st = http_status(glu_http);
    if (st == HTTP_DONE) {
        if (http_response_code(glu_http) == 200) {
            int rl = http_read(glu_http, glu_resp, sizeof(glu_resp) - 1);
            if (rl > 0) {
                glu_resp[rl] = '\0';
                int v = json_int(glu_resp, "v");
                int tr = json_int(glu_resp, "t");
                if (v > 0) {
                    glu_value = v;
                    glu_trend = (tr >= 1 && tr <= 7) ? tr : 4;
                    glu_valid = 1;
                    glu_update_ms = (int)millis();
                }
            }
        }
        http_close(glu_http); glu_http = -1;
    } else if (st == HTTP_ERROR) {
        http_close(glu_http); glu_http = -1;
    }
}

// ============================================================
// Hacker News fetch
// ============================================================

static void hn_start(void) {
    if (hn_http >= 0) return;
    if (proxy_url[0] == 0) { hn_state = HN_ERROR; return; }
    char url[192];
    pxl_strcpy(url, sizeof(url), proxy_url);
    pxl_strcat(url, sizeof(url), "/hn");
    hn_http = http_get_str(url);
    hn_state = (hn_http >= 0) ? HN_FETCH : HN_ERROR;
}

static void hn_poll(int dt) {
    if (!wifi_ok) return;
    hn_timer += dt;
    if (hn_state == HN_IDLE || hn_state == HN_DONE || hn_state == HN_ERROR) {
        if (hn_timer >= HN_POLL_MS) {
            hn_timer = 0;
            hn_start();
        }
        return;
    }
    if (hn_http < 0) return;
    int st = http_status(hn_http);
    hn_dbg_http_status = st;
    if (st == HTTP_DONE) {
        int rc = http_response_code(hn_http);
        hn_dbg_response_code = rc;
        if (rc == 200) {
            int rl = http_read(hn_http, hn_buf, sizeof(hn_buf) - 1);
            hn_dbg_read_len = rl;
            if (rl > 0) {
                hn_buf[rl] = '\0';
                int n = parse_proxy_hn(hn_buf, HN_MAX,
                                        hn_summaries, hn_summary_lens, hn_scores);
                hn_dbg_parse_n = n;
                http_close(hn_http); hn_http = -1;
                if (n >= 1) {
                    hn_count = n;
                    hn_state = HN_DONE;
                    hn_valid = 1;
                    hn_page = 0;
                    hn_page_timer = 0;
                    // Invalidate icon cache — headlines may have changed
                    for (int i = 0; i < HN_MAX; i++) {
                        icon_state[i] = SPR_ABSENT;
                        icon_pal_n[i] = 0;
                    }
                } else {
                    hn_state = HN_ERROR;
                }
                return;
            }
        }
        http_close(hn_http); hn_http = -1;
        hn_state = HN_ERROR;
    } else if (st == HTTP_ERROR) {
        http_close(hn_http); hn_http = -1;
        hn_state = HN_ERROR;
    }
}

// ============================================================
// Dota
// ============================================================

static void dota_trigger(void) {
    dota_alert_active = 1;
    dota_alert_elapsed = 0;
}

static void icon_start_fetch(int idx) {
    if (icon_http >= 0) return;
    if (idx < 0 || idx >= HN_MAX) return;
    if (icon_state[idx] != SPR_ABSENT) return;
    if (proxy_url[0] == 0 || !wifi_ok) return;

    char url[192];
    pxl_strcpy(url, sizeof(url), proxy_url);
    pxl_strcat(url, sizeof(url), "/image/sprite/hn/");
    char ib[4]; int il = pxl_itoa(idx, ib, sizeof(ib));
    ib[il] = 0;
    pxl_strcat(url, sizeof(url), ib);
    icon_http = http_get_str(url);
    if (icon_http >= 0) {
        icon_state[idx] = SPR_FETCHING;
        icon_fetching_idx = idx;
    }
}

static void icon_poll(void) {
    if (icon_http < 0) return;
    int st = http_status(icon_http);
    if (st != HTTP_DONE && st != HTTP_ERROR) return;

    int idx = icon_fetching_idx;
    icon_dbg_rl = -1;
    icon_dbg_w = icon_dbg_h = icon_dbg_n = 0;
    icon_dbg_rc = (st == HTTP_DONE) ? http_response_code(icon_http) : -1;

    if (st == HTTP_DONE && icon_dbg_rc == 200) {
        int rl = http_read(icon_http, (char *)icon_buf, sizeof(icon_buf));
        icon_dbg_rl = rl;
        if (rl >= 6 && idx >= 0 && idx < HN_MAX) {
            unsigned short w = (unsigned short)(icon_buf[0] | (icon_buf[1] << 8));
            unsigned short h = (unsigned short)(icon_buf[2] | (icon_buf[3] << 8));
            unsigned short n = (unsigned short)(icon_buf[4] | (icon_buf[5] << 8));
            icon_dbg_w = w; icon_dbg_h = h; icon_dbg_n = n;
            int need = 6 + n * 2 + w * h;
            if (w == ICON_W && h == ICON_H && n > 0 && n <= ICON_PAL_MAX && rl >= need) {
                for (int i = 0; i < n; i++) {
                    icon_pal[idx][i] = (unsigned short)(icon_buf[6 + i * 2] |
                                                        (icon_buf[6 + i * 2 + 1] << 8));
                }
                pxl_memcpy(icon_data[idx], &icon_buf[6 + n * 2], w * h);
                icon_pal_n[idx] = (unsigned char)n;
                icon_state[idx] = SPR_READY;
            } else {
                icon_state[idx] = SPR_ERROR;
            }
        } else {
            if (idx >= 0 && idx < HN_MAX) icon_state[idx] = SPR_ERROR;
        }
    } else {
        if (idx >= 0 && idx < HN_MAX) icon_state[idx] = SPR_ERROR;
    }
    http_close(icon_http); icon_http = -1;
    icon_fetching_idx = -1;
}

// Ask the proxy to clear its flag — fire-and-forget.
static void dota_post_clear(void) {
    if (proxy_url[0] == 0 || dota_clear_http >= 0) return;
    char url[160];
    pxl_strcpy(url, sizeof(url), proxy_url);
    pxl_strcat(url, sizeof(url), "/dota/clear");
    dota_clear_http = http_post(url, pxl_strlen(url),
                                "application/json", 16,
                                "{}", 2);
}

static void dota_poll(int dt) {
    if (dota_alert_active) {
        dota_alert_elapsed += dt;
        if (dota_alert_elapsed >= DOTA_ALERT_MS) dota_alert_active = 0;
    }

    // Reap in-flight /dota/clear so the slot frees up
    if (dota_clear_http >= 0) {
        int cst = http_status(dota_clear_http);
        if (cst == HTTP_DONE || cst == HTTP_ERROR) {
            http_close(dota_clear_http);
            dota_clear_http = -1;
        }
    }

    if (!wifi_ok || proxy_url[0] == 0) return;
    dota_timer += dt;
    if (dota_http < 0 && dota_timer >= DOTA_POLL_MS && !dota_alert_active) {
        dota_timer = 0;
        char url[160];
        pxl_strcpy(url, sizeof(url), proxy_url);
        pxl_strcat(url, sizeof(url), "/dota/status");
        dota_http = http_get_str(url);
    }
    if (dota_http < 0) return;
    int st = http_status(dota_http);
    if (st == HTTP_DONE) {
        if (http_response_code(dota_http) == 200) {
            int rl = http_read(dota_http, dota_resp, sizeof(dota_resp) - 1);
            if (rl > 0) {
                dota_resp[rl] = '\0';
                // Look for "alert":true inside the JSON
                const char *p = dota_resp;
                int alert = 0;
                while (*p) {
                    if (p[0] == '"' && p[1] == 'a' && p[2] == 'l' && p[3] == 'e' &&
                        p[4] == 'r' && p[5] == 't' && p[6] == '"') {
                        const char *q = p + 7;
                        while (*q == ':' || *q == ' ') q++;
                        if (q[0] == 't' && q[1] == 'r' && q[2] == 'u' && q[3] == 'e') {
                            alert = 1;
                        }
                        break;
                    }
                    p++;
                }
                if (alert && !dota_alert_active) {
                    dota_trigger();
                    dota_post_clear();
                }
            }
        }
        http_close(dota_http); dota_http = -1;
    } else if (st == HTTP_ERROR) {
        http_close(dota_http); dota_http = -1;
    }
}

// ============================================================
// UI helpers
// ============================================================

// Pastel color-coding for glucose ranges. Still distinguishable, but softer
// and tuned for dark text on a light background rather than glowing phosphor.
static unsigned int glucose_color(int v) {
    if (v <= 0)   return rgb(140, 150, 175);
    if (v < 54)   return rgb(220, 100, 110);   // critical low
    if (v < 70)   return rgb(130, 175, 235);   // low
    if (v <= 180) return rgb(90, 175, 120);    // in range
    if (v <= 250) return rgb(235, 165, 80);    // high
    return rgb(220, 100, 110);                  // critical high
}

static void draw_trend_arrow(int x, int y, int trend, unsigned int c) {
    if (trend < 1 || trend > 7) return;
    if (trend == 4) {
        for (int i = 0; i < 7; i++) set_pixel(x + i, y + 3, c);
        set_pixel(x + 4, y + 1, c); set_pixel(x + 5, y + 2, c);
        set_pixel(x + 5, y + 4, c); set_pixel(x + 4, y + 5, c);
        return;
    }
    int rising = trend < 4;
    int steep = (trend == 1 || trend == 7) ? 2 :
                (trend == 2 || trend == 6) ? 1 : 0;
    for (int i = 0; i < 7; i++) {
        int dy;
        if (steep == 2) dy = rising ? (6 - i) : i;
        else if (steep == 1) dy = rising ? (5 - i * 3 / 4) : (i * 3 / 4 + 1);
        else                 dy = rising ? (4 - i / 2) : (i / 2 + 2);
        if (dy < 0) dy = 0; if (dy > 6) dy = 6;
        set_pixel(x + i, y + dy, c);
    }
}

static void draw_panel(int x, int y, int w, int h) {
    fill_rect(x, y, w, h, C_PANEL);
    draw_rect(x, y, w, h, C_BORDER);
    // Knock out corners for a soft-rounded tile
    set_pixel(x,         y,         C_BG);
    set_pixel(x + w - 1, y,         C_BG);
    set_pixel(x,         y + h - 1, C_BG);
    set_pixel(x + w - 1, y + h - 1, C_BG);
}

// ============================================================
// Panels
// ============================================================

static void draw_glucose_panel(void) {
    int x = 2, y = 1, w = SW - 4, h = GLU_H - 2;
    draw_panel(x, y, w, h);
    draw_str(x + 4, y + 3, "GLUCOSE", C_HDR_GLU);

    if (!glu_valid) {
        float p = sinf(t * 2.0f) * 0.25f + 0.75f;
        const char *msg = wifi_ok ? "ACQUIRING..." : "NO WIFI";
        draw_str(x + 4, y + 14, msg, dim_color(C_HDR_GLU, p));
        return;
    }

    unsigned int gc = glucose_color(glu_value);
    int age = (int)millis() - glu_update_ms;
    int stale = age > STALE_MS;

    float pulse = 1.0f;
    if (glu_value < 70 || glu_value > 250) pulse = 0.7f + sinf(t * 5.0f) * 0.3f;
    if (stale) pulse *= 0.45f;

    // Big value
    char buf[6];
    int bl = pxl_itoa(glu_value, buf, sizeof(buf));
    int val_x = x + 50;
    int val_y = y + 10;
    draw_text_big(val_x, val_y, buf, bl, dim_color(gc, pulse));
    draw_str(val_x + bl * PXL_FONT_BIG_W + 2, val_y + 8, "mg/dL", C_MGDL_DIM);

    // Trend
    int arrow_x = x + w - 60;
    draw_trend_arrow(arrow_x, y + 5, glu_trend, dim_color(gc, stale ? 0.5f : 1.0f));
    const char *tnames[] = {"", "++", " +", "up", " =", "dn", " -", "--"};
    if (glu_trend >= 1 && glu_trend <= 7) {
        draw_str(arrow_x + 10, y + 5, tnames[glu_trend], dim_color(gc, 0.85f));
    }

    // Status + age
    const char *stxt;
    if (stale)                 stxt = "STALE";
    else if (glu_value < 54)   stxt = "CRIT";
    else if (glu_value < 70)   stxt = "LOW";
    else if (glu_value <= 180) stxt = "OK";
    else if (glu_value <= 250) stxt = "HIGH";
    else                       stxt = "CRIT";
    unsigned int sc = stale ? dim_color(rgb(200, 140, 60), 0.85f) : dim_color(gc, 0.85f);
    draw_str(arrow_x, y + 15, stxt, sc);

    int mins = age / 60000;
    char abuf[10];
    int al;
    if (mins > 0) { al = pxl_itoa(mins, abuf, sizeof(abuf)); abuf[al++] = 'm'; }
    else          { al = pxl_itoa(age / 1000, abuf, sizeof(abuf)); abuf[al++] = 's'; }
    abuf[al] = 0;
    draw_str(arrow_x + 30, y + 15, abuf, C_TEXT_DIM);
}

static void draw_hn_panel(void) {
    int x = 2, y = HN_Y, w = SW - 4, h = HN_H;
    draw_panel(x, y, w, h);
    draw_str(x + 4, y + 2, "HACKER NEWS", C_HDR_HN);

    // Fetching dots
    if (hn_state == HN_FETCH) {
        int dot = ((int)(t * 5.0f)) % 3;
        for (int d = 0; d < 3; d++) {
            float br = (d == dot) ? 1.0f : 0.3f;
            fill_rect(x + 80 + d * 4, y + 3, 2, 2, dim_color(C_HDR_HN, br));
        }
    }

    if (!hn_valid) {
        const char *msg = !wifi_ok             ? "NO WIFI" :
                          hn_state == HN_ERROR ? "LOAD FAILED" :
                          hn_state == HN_IDLE  ? "CONNECTING..." : "LOADING...";
        float p = sinf(t * 2.0f) * 0.25f + 0.75f;
        draw_str_centered(x, w, y + h / 2 - 8, msg, dim_color(C_HDR_HN, p));
        // Diagnostics when failed — shows "st=X code=Y len=Z n=W"
        // st: 3=DONE 4=ERROR · code: HTTP status · len: bytes · n: parsed hits
        if (hn_state == HN_ERROR) {
            char dbg[48];
            int dl = 0;
            dbg[dl++] = 's'; dbg[dl++] = 't'; dbg[dl++] = '=';
            dl += pxl_itoa(hn_dbg_http_status, dbg + dl, 40);
            dbg[dl++] = ' '; dbg[dl++] = 'r'; dbg[dl++] = 'c'; dbg[dl++] = '=';
            dl += pxl_itoa(hn_dbg_response_code, dbg + dl, 40 - dl);
            dbg[dl++] = ' '; dbg[dl++] = 'l'; dbg[dl++] = '=';
            dl += pxl_itoa(hn_dbg_read_len, dbg + dl, 40 - dl);
            dbg[dl++] = ' '; dbg[dl++] = 'n'; dbg[dl++] = '=';
            dl += pxl_itoa(hn_dbg_parse_n, dbg + dl, 40 - dl);
            dbg[dl] = 0;
            draw_str_centered(x, w, y + h / 2 + 3, dbg, C_TEXT_DIM);
        }
        return;
    }

    // Single-story view: cycle through hn_count stories, one at a time.
    if (hn_page >= hn_count) hn_page = 0;
    int i = hn_page;

    // Header right side: rank + "N/10" indicator
    char pg[10];
    int pl = 0;
    int rn = i + 1;
    if (rn >= 10) pg[pl++] = '1' + (rn / 10 - 1);
    pg[pl++] = '0' + (rn % 10);
    pg[pl++] = '/';
    int tot = hn_count;
    if (tot >= 10) pg[pl++] = '1' + (tot / 10 - 1);
    pg[pl++] = '0' + (tot % 10);
    pg[pl] = 0;
    int pgx = x + w - pl * PXL_FONT_W - 4;
    draw_str(pgx, y + 2, pg, dim_color(C_HDR_HN, 0.85f));

    // ---- Icon zone (left ~1/3) ----
    int icon_x = x + 4;
    int icon_y = y + 14;
    int icon_zone_w = w / 3;   // ~65 for a 196-wide panel
    if (icon_state[i] == SPR_READY && icon_pal_n[i] > 0) {
        blit_sprite(icon_x, icon_y, icon_data[i], icon_pal[i], ICON_W, ICON_H, 1.0f);
    } else {
        // Placeholder — dim tile with state info
        unsigned int tile_bg = (icon_state[i] == SPR_ERROR)
            ? dim_color(rgb(250, 200, 200), 0.85f)
            : dim_color(C_BORDER_SOFT, 0.6f);
        unsigned int tile_border = (icon_state[i] == SPR_ERROR)
            ? rgb(220, 90, 90) : C_BORDER;
        fill_rect(icon_x, icon_y, ICON_W, ICON_H, tile_bg);
        draw_rect(icon_x, icon_y, ICON_W, ICON_H, tile_border);

        char rb[3]; int rl = 0;
        if (rn >= 10) rb[rl++] = '0' + (rn / 10);
        rb[rl++] = '0' + (rn % 10);
        rb[rl] = 0;
        int big_w = rl * PXL_FONT_BIG_W;
        draw_text_big(icon_x + (ICON_W - big_w) / 2,
                      icon_y + (ICON_H - PXL_FONT_BIG_H) / 2 - 6,
                      rb, rl, dim_color(C_HDR_HN, 0.7f));

        if (icon_state[i] == SPR_FETCHING) {
            int dot = ((int)(t * 6.0f)) % 3;
            for (int d = 0; d < 3; d++) {
                float br = (d == dot) ? 0.9f : 0.2f;
                fill_rect(icon_x + 16 + d * 6, icon_y + ICON_H - 6, 3, 3,
                          dim_color(C_ACCENT, br));
            }
        }

        // Show diagnostics on the current slot when last fetch failed.
        // Format: "rc=X rl=Y w=W,H n=N" (whatever fits on 3 tiny rows)
        if (icon_state[i] == SPR_ERROR || i == icon_fetching_idx) {
            char db[24]; int dl;
            int by = icon_y + ICON_H - 22;
            dl = 0;
            db[dl++] = 'r'; db[dl++] = 'c'; db[dl++] = '=';
            dl += pxl_itoa(icon_dbg_rc, db + dl, 20);
            db[dl] = 0;
            draw_str(icon_x + 2, by, db, rgb(60, 20, 20));

            dl = 0;
            db[dl++] = 'l'; db[dl++] = '=';
            dl += pxl_itoa(icon_dbg_rl, db + dl, 20);
            db[dl] = 0;
            draw_str(icon_x + 2, by + 8, db, rgb(60, 20, 20));

            if (icon_dbg_w || icon_dbg_h || icon_dbg_n) {
                dl = 0;
                dl += pxl_itoa(icon_dbg_w, db + dl, 22);
                db[dl++] = 'x';
                dl += pxl_itoa(icon_dbg_h, db + dl, 22 - dl);
                db[dl++] = '/';
                dl += pxl_itoa(icon_dbg_n, db + dl, 22 - dl);
                db[dl] = 0;
                draw_str(icon_x + 24, by + 4, db, rgb(60, 20, 20));
            }
        }
    }

    // ---- Text zone (right 2/3) ----
    int tx = icon_x + ICON_W + 6;
    int text_w_px = w - (tx - x) - 6;
    int top_line_y = y + 14;
    int top_line_chars = text_w_px / PXL_FONT_W;

    // Score on the very top-right; leave space for it on line 1
    char sb[8];
    int sll = pxl_itoa(hn_scores[i], sb, sizeof(sb));
    int score_x = x + w - sll * PXL_FONT_W - 4;
    draw_text(score_x, top_line_y, sb, sll, C_ACCENT);
    if (score_x - tx - 8 > 0) {
        draw_str(score_x - 8, top_line_y, "^", dim_color(C_ACCENT, 0.6f));
    }

    // Lines 2..5 are the wrapped summary under the score row
    int text_w_chars = top_line_chars - sll - 2;   // first line shares with score
    if (text_w_chars < 8) text_w_chars = 8;
    int full_w_chars = top_line_chars;

    int line_h = 9;
    int max_lines = 5;
    const char *src = hn_summaries[i];
    int src_len = hn_summary_lens[i];
    int pos = 0, line = 0;

    while (pos < src_len && line < max_lines) {
        while (pos < src_len && src[pos] == ' ') pos++;
        if (pos >= src_len) break;

        int line_chars = (line == 0) ? text_w_chars : full_w_chars;
        int line_start = pos;
        int last_space = -1;
        int chars = 0;
        int scan = pos;
        while (scan < src_len && chars < line_chars) {
            if (src[scan] == ' ') last_space = scan;
            if (src[scan] == '\n') break;
            scan++;
            chars++;
        }

        int line_end;
        if (scan < src_len && src[scan] != ' ' && src[scan] != '\n' && last_space > line_start) {
            line_end = last_space;
            pos = last_space + 1;
        } else {
            line_end = scan;
            pos = scan;
            if (pos < src_len && (src[pos] == ' ' || src[pos] == '\n')) pos++;
        }
        int ll = line_end - line_start;

        int truncate = (line == max_lines - 1 && pos < src_len);
        int line_y = top_line_y + line * line_h;

        if (truncate) {
            int room = line_chars - 3;
            if (ll > room) ll = room;
            draw_text(tx, line_y, &src[line_start], ll, C_TEXT);
            draw_text(tx + ll * PXL_FONT_W, line_y, "...", 3, dim_color(C_TEXT, 0.7f));
        } else {
            draw_text(tx, line_y, &src[line_start], ll, C_TEXT);
        }
        line++;
    }

    // Progress bar under header showing time until next story
    int bar_x = x + 4;
    int bar_w = w - 8;
    int filled = (hn_page_timer * bar_w) / HN_PAGE_MS;
    if (filled > bar_w) filled = bar_w;
    set_pixel(bar_x + filled, y + 10, dim_color(C_HDR_HN, 0.5f));
    if (filled > 0) {
        for (int bx = bar_x; bx < bar_x + filled; bx += 2) {
            set_pixel(bx, y + 10, dim_color(C_HDR_HN, 0.25f));
        }
    }
}

static void draw_bottom_bar(void) {
    int y = SH - BOT_H, x = 2, w = SW - 4, h = BOT_H - 2;
    draw_panel(x, y, w, h);

    // WiFi glyph
    unsigned int wc = wifi_ok ? C_WIFI_OK : C_WIFI_BAD;
    fill_rect(x + 3, y + 7, 2, 3, wc);
    fill_rect(x + 6, y + 5, 2, 5, wc);
    fill_rect(x + 9, y + 3, 2, 7, wc);
    draw_str(x + 14, y + 4, wifi_ok ? "WIFI" : "OFFL", dim_color(wc, 0.95f));

    // Activity spinner
    int activity = (glu_http >= 0) + (hn_http >= 0) + (dota_http >= 0);
    if (activity > 0) {
        float br = 0.55f + sinf(t * 6.0f) * 0.3f;
        fill_rect(x + 44, y + 6, 3, 3, dim_color(C_ACTIVITY, br));
        draw_str(x + 50, y + 4, "FETCH", dim_color(C_ACTIVITY, 0.9f));
    }

    pxl_button_draw(&btn_test_dota);
}

// ============================================================
// DOTA ALERT OVERLAY
// ============================================================

static void draw_dota_overlay(void) {
    float pulse = sinf(t * 7.0f) * 0.5f + 0.5f;

    // Fade in/out envelope
    float fi = (float)dota_alert_elapsed / 400.0f;
    if (fi > 1.0f) fi = 1.0f;
    float fo = 1.0f;
    int rem = DOTA_ALERT_MS - dota_alert_elapsed;
    if (rem < 400) fo = (float)rem / 400.0f;
    if (fo < 0.0f) fo = 0.0f;
    float a = fi * fo;

    // Soft pastel-red wash instead of saturated red. Still unmistakable as an alert.
    clear(dim_color(rgb(245, 160, 160), (0.75f + pulse * 0.2f) * a));

    // Faint cream scanlines
    unsigned int scan_c = dim_color(rgb(255, 240, 230), a * 0.25f);
    for (int sy = 0; sy < SH; sy += 4) draw_line_h(0, sy, SW, scan_c);

    // Framed border — cream outer, gold inner
    unsigned int border = dim_color(rgb(255, 250, 240), a);
    draw_rect(0, 0, SW, SH, border);
    draw_rect(1, 1, SW - 2, SH - 2, border);
    draw_rect(4, 4, SW - 8, SH - 8, dim_color(rgb(245, 195, 110), a * 0.85f));

    // Sprite at center — replace via tools/rasterize_sprite.py on any image
    int cx = SW / 2, cy = SH / 2;
    int sx = cx - DOTA_SPRITE_W / 2;
    int sy = cy - DOTA_SPRITE_H / 2;

    // Soft cream glow behind the sprite so dark/transparent edges read well
    fill_circle(cx, cy, DOTA_SPRITE_W / 2 + 4,
                dim_color(rgb(255, 250, 240), a * 0.7f));
    draw_circle(cx, cy, DOTA_SPRITE_W / 2 + 4,
                dim_color(rgb(225, 155, 90), a));
    draw_circle(cx, cy, DOTA_SPRITE_W / 2 + 3,
                dim_color(rgb(225, 155, 90), a * 0.6f));

    blit_sprite(sx, sy, DOTA_SPRITE_DATA, DOTA_SPRITE_PAL,
                DOTA_SPRITE_W, DOTA_SPRITE_H, a);

    // Text — dark on cream with soft bounce, Wii-U-ish badge vibe
    float bounce = sinf(t * 5.0f) * 0.6f;
    unsigned int title_fg    = dim_color(rgb(60, 50, 70), a);
    unsigned int title_shade = dim_color(rgb(255, 230, 170), a * 0.85f);

    int tx = (SW - 4 * PXL_FONT_BIG_W) / 2;
    draw_text_big(tx + 1, 11 + (int)bounce, "DOTA", 4, title_shade);
    draw_text_big(tx,     10 + (int)bounce, "DOTA", 4, title_fg);

    int t2x = (SW - 6 * PXL_FONT_BIG_W) / 2;
    draw_text_big(t2x + 1, SH - 29 - (int)bounce, "TIME!!", 6, title_shade);
    draw_text_big(t2x,     SH - 30 - (int)bounce, "TIME!!", 6, title_fg);

    // Progress-to-dismiss bar
    int bar_w = SW - 20;
    int rem_px = (rem * bar_w) / DOTA_ALERT_MS;
    if (rem_px < 0) rem_px = 0; if (rem_px > bar_w) rem_px = bar_w;
    fill_rect(10, SH - 14, bar_w, 1, dim_color(rgb(255, 220, 200), a * 0.6f));
    fill_rect(10, SH - 14, rem_px, 1, dim_color(rgb(220, 145, 75), a));

    draw_str_centered(0, SW, SH - 10, "TAP TO DISMISS",
                      dim_color(rgb(90, 70, 90), a * 0.9f));
}

// ============================================================
// Exports
// ============================================================

static void init_palette(void) {
    C_BG           = rgb(245, 247, 252);   // off-white
    C_PANEL        = rgb(255, 255, 255);   // pure white tiles
    C_BORDER       = rgb(195, 205, 225);   // soft blue-gray
    C_BORDER_SOFT  = rgb(220, 225, 235);
    C_TEXT         = rgb(45, 55, 80);      // dark slate
    C_TEXT_DIM     = rgb(140, 150, 175);
    C_SEPARATOR    = rgb(225, 230, 240);
    C_HDR_GLU      = rgb(130, 145, 200);   // pastel indigo
    C_HDR_HN       = rgb(230, 145, 175);   // pastel pink
    C_ACCENT       = rgb(230, 170, 90);    // muted orange
    C_WIFI_OK      = rgb(130, 200, 155);   // mint
    C_WIFI_BAD     = rgb(230, 140, 140);   // pastel red
    C_ACTIVITY     = rgb(235, 205, 120);   // pastel amber
    C_MGDL_DIM     = rgb(155, 165, 190);
}

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(2);
    t = 0.0f;
    srand((int)millis());
    init_palette();

    int len = kv_get_str("glucose_url", glu_url, sizeof(glu_url));
    if (len <= 0) {
        pxl_strcpy(glu_url, sizeof(glu_url),
            "https://glucose.garay.mx/api/glucose/esp32?token=sugaryesp32123");
    }

    // proxy_url — pixelitl-service LAN endpoint. Default to the user's Mac IP.
    // Override via `kv_set proxy_url http://other:4001` from the Studio KV UI.
    len = kv_get_str("proxy_url", proxy_url, sizeof(proxy_url));
    if (len <= 0) {
        pxl_strcpy(proxy_url, sizeof(proxy_url), "http://192.168.1.124:4001");
    }
    dota_has_endpoint = (proxy_url[0] != 0);

    // Soft pastel tile button, dark text
    pxl_button(&btn_test_dota, SW - 46, SH - BOT_H + 3, 42, 12,
               "DOTA", rgb(250, 220, 225), rgb(175, 70, 90));
    btn_test_dota.pressed_bg = rgb(235, 195, 205);
    btn_test_dota.pressed_fg = rgb(130, 40, 55);

    glu_valid = 0; glu_value = 0; glu_trend = 4;
    glu_http = -1; glu_timer = 0; glu_update_ms = 0;
    hn_http = -1; hn_timer = 0; hn_valid = 0; hn_state = HN_IDLE; hn_count = 0;
    hn_page = 0; hn_page_timer = 0;
    for (int i = 0; i < HN_MAX; i++) {
        hn_summaries[i][0] = 0; hn_summary_lens[i] = 0; hn_scores[i] = 0;
    }
    dota_clear_http = -1;
    dota_http = -1; dota_timer = 0;
    dota_alert_active = 0; dota_alert_elapsed = 0;
    wifi_ok = 0;
}

__attribute__((export_name("app_wifi_ready")))
void app_wifi_ready(void) {
    wifi_ok = 1;
    glu_start();
    hn_start();
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    t += (float)dt * 0.001f;

    if (dota_alert_active) {
        int g = get_gesture();
        if (g == GESTURE_TAP || g == GESTURE_LONG_PRESS) {
            dota_alert_active = 0;
        }
    } else {
        if (pxl_button_clicked(&btn_test_dota)) dota_trigger();
    }

    glu_poll(dt);
    hn_poll(dt);
    dota_poll(dt);
    icon_poll();

    // Retry SPR_ERROR entries every 15s so a single blip doesn't lock a slot
    icon_retry_timer += dt;
    if (icon_retry_timer >= 15000) {
        icon_retry_timer = 0;
        for (int i = 0; i < HN_MAX; i++) {
            if (icon_state[i] == SPR_ERROR) icon_state[i] = SPR_ABSENT;
        }
    }

    // Lazy-fetch icon for the current story, then pre-fetch the next one.
    if (hn_valid && icon_http < 0) {
        if (hn_page >= 0 && hn_page < hn_count && icon_state[hn_page] == SPR_ABSENT) {
            icon_start_fetch(hn_page);
        } else {
            int nxt = (hn_page + 1) % (hn_count < 1 ? 1 : hn_count);
            if (nxt != hn_page && icon_state[nxt] == SPR_ABSENT) {
                icon_start_fetch(nxt);
            }
        }
    }

    // Auto-advance one story at a time through all 10
    if (hn_valid && hn_count > 1) {
        hn_page_timer += dt;
        if (hn_page_timer >= HN_PAGE_MS) {
            hn_page_timer = 0;
            hn_page = (hn_page + 1) % hn_count;
        }
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    if (dota_alert_active) {
        draw_dota_overlay();
        return;
    }
    clear(C_BG);
    draw_glucose_panel();
    draw_hn_panel();
    draw_bottom_bar();
}
