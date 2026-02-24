#include "pixelitl.h"

// ===== PIXELITL FACE STORE =====
// Ultra resolution (200x120) with 80s demoscene aesthetics

// ---- Catalog ----
#define MAX_CATALOG 20
#define MAX_NAME    24
#define MAX_FILE    32

typedef struct {
    char name[MAX_NAME];
    int  name_len;
    char filename[MAX_FILE];
    int  filename_len;
    int  installed;
} entry_t;

static entry_t catalog[MAX_CATALOG];
static int cat_count;

// ---- State ----
typedef enum { S_WIFI, S_LOADING, S_READY, S_INSTALLING, S_ERROR } state_t;
static state_t state;
static int http_h = -1;
static int scroll_pos;
static float t;

static char base_url[128];
static char full_url[256];
static char status_msg[40];
static int  status_len;
static int  install_idx = -1;
static int  last_ist;
static float install_anim;

// ---- Starfield (3-layer parallax) ----
#define NSTARS 50
static float star_x[NSTARS], star_y[NSTARS];
static float star_sp[NSTARS], star_br[NSTARS];
static int   star_layer[NSTARS];

// ---- Sine scroller ----
static float scroller_off;
static const char SCROLLER[] =
    "     *** PIXELITL FACE STORE ***     "
    "DOWNLOAD FACES FOR YOUR DISPLAY!     "
    "SWIPE UP/DOWN TO BROWSE -- TAP TO INSTALL     "
    "GREETS TO ALL DEMOSCENERS AND PIXEL ARTISTS!     "
    "CODED WITH LOVE FOR ESP32-S3     ";

// ---- Layout ----
#define SW        200
#define SH        120
#define COPPER_H  4
#define LOGO_Y    5
#define LIST_X    4
#define LIST_Y    26
#define LIST_W    112
#define ITEM_H    9
#define VISIBLE   8
#define NAV_X     120
#define NAV_W     14
#define INFO_X    138
#define INFO_Y    26
#define INFO_W    58
#define SCROLL_Y  106

// ---- Buttons ----
static pxl_button_t item_btns[VISIBLE];
static pxl_button_t btn_up, btn_down, btn_refresh;

// ---- Index buffer ----
static char index_buf[2048];

// ---- Helpers ----

static void set_status(const char *msg) {
    status_len = pxl_strcpy(status_msg, sizeof(status_msg), msg);
}

static void build_url(const char *path, int plen) {
    int blen = pxl_strlen(base_url);
    int i = pxl_memcpy(full_url, base_url, blen);
    i += pxl_memcpy(full_url + i, path, plen < 255 - i ? plen : 255 - i);
    full_url[i] = '\0';
}

static void parse_index(const char *data, int len) {
    cat_count = 0;
    int i = 0;
    while (i < len && cat_count < MAX_CATALOG) {
        int ns = i;
        while (i < len && data[i] != '|' && data[i] != '\n' && data[i] != '\r') i++;
        if (i >= len || data[i] != '|') {
            while (i < len && data[i] != '\n') i++;
            if (i < len) i++;
            continue;
        }
        int nl = i - ns;
        i++;
        int fs = i;
        while (i < len && data[i] != '\n' && data[i] != '\r') i++;
        int fl = i - fs;
        while (i < len && (data[i] == '\n' || data[i] == '\r')) i++;
        if (nl > 0 && nl < MAX_NAME && fl > 0 && fl < MAX_FILE) {
            entry_t *e = &catalog[cat_count];
            pxl_memcpy(e->name, data + ns, nl);
            e->name[nl] = '\0';
            e->name_len = nl;
            pxl_memcpy(e->filename, data + fs, fl);
            e->filename[fl] = '\0';
            e->filename_len = fl;
            e->installed = 0;
            cat_count++;
        }
    }
}

static void refresh_installed(void) {
    int n = face_count();
    char buf[MAX_NAME];
    for (int c = 0; c < cat_count; c++) {
        catalog[c].installed = 0;
        for (int f = 0; f < n; f++) {
            if (face_get_name(f, buf, sizeof(buf)) > 0 && pxl_streq(catalog[c].name, buf)) {
                catalog[c].installed = 1;
                break;
            }
        }
    }
}

static void setup_item_buttons(void) {
    int vis = cat_count - scroll_pos;
    if (vis > VISIBLE) vis = VISIBLE;
    for (int i = 0; i < vis; i++) {
        int idx = scroll_pos + i;
        int iy = LIST_Y + i * ITEM_H;
        pxl_button(&item_btns[i], LIST_X, iy, LIST_W, ITEM_H - 1,
                   catalog[idx].name, rgb(12, 10, 28), rgb(180, 185, 210));
    }
}

static void fetch_index(void) {
    build_url("index.txt", 9);
    http_h = http_get_str(full_url);
    state = S_LOADING;
    set_status("CONNECTING...");
    cat_count = 0;
    scroll_pos = 0;
}

static void init_stars(void) {
    for (int i = 0; i < NSTARS; i++) {
        star_x[i] = (float)(rand() % SW);
        star_y[i] = (float)(rand() % SH);
        star_layer[i] = rand() % 3;
        star_sp[i] = 0.3f + (float)star_layer[i] * 0.5f + (float)(rand() % 10) * 0.05f;
        star_br[i] = 0.1f + (float)star_layer[i] * 0.25f + (float)(rand() % 20) * 0.01f;
    }
}

// ======== EXPORTS ========

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(2);
    t = 0.0f;
    srand((int)millis());

    int len = kv_get_str("server", base_url, sizeof(base_url));
    if (len <= 0)
        pxl_strcpy(base_url, sizeof(base_url), "http://192.168.1.124:4000/api/faces/");

    install_idx = -1;
    last_ist = INSTALL_IDLE;
    install_anim = 0.0f;
    scroll_pos = 0;
    scroller_off = 0.0f;

    // Nav buttons — tall side buttons next to list
    unsigned int nav_bg = rgb(15, 13, 32);
    unsigned int nav_fg = rgb(100, 140, 200);
    int list_h = VISIBLE * ITEM_H;
    int half_h = list_h / 2;
    pxl_button(&btn_up,   NAV_X, LIST_Y, NAV_W, half_h, "^", nav_bg, nav_fg);
    pxl_button(&btn_down, NAV_X, LIST_Y + half_h, NAV_W, half_h, "v", nav_bg, nav_fg);
    pxl_button(&btn_refresh, INFO_X, 82, INFO_W, 9, "REFRESH", nav_bg, rgb(100, 160, 100));
    btn_up.pressed_bg = rgb(30, 25, 60);
    btn_down.pressed_bg = rgb(30, 25, 60);
    btn_refresh.pressed_bg = rgb(25, 45, 30);

    init_stars();
    state = S_WIFI;
    set_status("AWAITING SIGNAL...");
}

__attribute__((export_name("app_wifi_ready")))
void app_wifi_ready(void) {
    if (state == S_WIFI) fetch_index();
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    float dtf = (float)dt;
    t += dtf * 0.001f;

    // Stars
    for (int i = 0; i < NSTARS; i++) {
        star_x[i] -= star_sp[i] * dtf * 0.015f;
        if (star_x[i] < -2.0f) {
            star_x[i] = (float)SW + (float)(rand() % 10);
            star_y[i] = (float)(rand() % SH);
        }
    }

    // Scroller
    scroller_off += dtf * 0.04f;
    int slen = pxl_strlen(SCROLLER);
    float total_w = (float)(slen * PXL_FONT_W);
    if (scroller_off > total_w) scroller_off -= total_w;

    if (state == S_WIFI) return;

    // Index fetch
    if (state == S_LOADING) {
        int st = http_status(http_h);
        if (st == HTTP_DONE) {
            int resp = http_response_code(http_h);
            if (resp == 200) {
                int blen = http_read(http_h, index_buf, sizeof(index_buf) - 1);
                if (blen > 0) { index_buf[blen] = '\0'; parse_index(index_buf, blen); }
                http_close(http_h); http_h = -1;
                if (cat_count > 0) {
                    state = S_READY;
                    refresh_installed();
                    setup_item_buttons();
                    set_status("");
                } else {
                    state = S_ERROR;
                    set_status("EMPTY CATALOG");
                }
            } else {
                http_close(http_h); http_h = -1;
                state = S_ERROR; set_status("SERVER ERROR");
            }
        } else if (st == HTTP_ERROR) {
            http_close(http_h); http_h = -1;
            state = S_ERROR; set_status("CONNECT FAILED");
        }
        return;
    }

    // Install polling (reset state if we returned from face_switch)
    if (state == S_INSTALLING && install_idx < 0) {
        state = S_READY;
        refresh_installed();
        setup_item_buttons();
        set_status("");
    }
    if (install_idx >= 0) {
        install_anim += dtf * 0.004f;
        if (install_anim > 6.28f) install_anim -= 6.28f;
        int ist = face_install_status();
        if (ist != last_ist) {
            last_ist = ist;
            if (ist == INSTALL_DOWNLOADING) set_status("DOWNLOADING...");
            else if (ist == INSTALL_LOADING) set_status("LOADING...");
            else if (ist == INSTALL_OK) set_status("INSTALLED!");
            else if (ist == INSTALL_ERROR) set_status("FAILED!");
        }
        if (ist == INSTALL_OK && (frame % 60 == 0)) {
            int nf = face_count() - 1;
            face_install_reset();
            install_idx = -1;
            last_ist = INSTALL_IDLE;
            face_switch(nf);
            return;
        }
        if (ist == INSTALL_ERROR && (frame % 90 == 0)) {
            face_install_reset();
            install_idx = -1;
            last_ist = INSTALL_IDLE;
            state = S_READY;
            refresh_installed();
            setup_item_buttons();
            set_status("");
        }
        return;
    }

    // Error: tap to retry
    if (state == S_ERROR) {
        if (get_gesture() == GESTURE_TAP) fetch_index();
        return;
    }
    if (state != S_READY) return;

    // Swipe scroll
    int gesture = get_gesture();
    if (gesture == GESTURE_SWIPE_UP && scroll_pos + VISIBLE < cat_count) {
        scroll_pos++;
        setup_item_buttons();
    }
    if (gesture == GESTURE_SWIPE_DOWN && scroll_pos > 0) {
        scroll_pos--;
        setup_item_buttons();
    }

    // Button scroll
    if (pxl_button_clicked(&btn_up) && scroll_pos > 0) {
        scroll_pos--;
        setup_item_buttons();
    }
    if (pxl_button_clicked(&btn_down) && scroll_pos + VISIBLE < cat_count) {
        scroll_pos++;
        setup_item_buttons();
    }

    // Refresh
    if (pxl_button_clicked(&btn_refresh)) {
        fetch_index();
    }

    // Item clicks
    int vis = cat_count - scroll_pos;
    if (vis > VISIBLE) vis = VISIBLE;
    for (int i = 0; i < vis; i++) {
        if (pxl_button_clicked(&item_btns[i])) {
            int idx = scroll_pos + i;
            if (idx < cat_count && !catalog[idx].installed) {
                build_url(catalog[idx].filename, catalog[idx].filename_len);
                int ret = face_install(full_url, pxl_strlen(full_url),
                                       catalog[idx].name, catalog[idx].name_len);
                if (ret == 0) {
                    install_idx = idx;
                    last_ist = INSTALL_DOWNLOADING;
                    state = S_INSTALLING;
                    install_anim = 0.0f;
                    set_status("STARTING...");
                }
            }
            break;
        }
    }
}

// ======== DRAWING ========

static void draw_stars(void) {
    for (int i = 0; i < NSTARS; i++) {
        int sx = (int)star_x[i], sy = (int)star_y[i];
        if (sx < 0 || sx >= SW || sy < 0 || sy >= SH) continue;
        float tw = sinf(t * 3.0f + star_br[i] * 15.0f) * 0.3f + 0.7f;
        float bri = star_br[i] * tw;
        unsigned int c;
        if (star_layer[i] == 0) c = dim_color(rgb(80, 100, 140), bri);
        else if (star_layer[i] == 1) c = dim_color(rgb(140, 160, 200), bri);
        else c = dim_color(rgb(200, 220, 255), bri);
        set_pixel(sx, sy, c);
        // Cross sparkle on bright near stars
        if (star_layer[i] == 2 && tw > 0.85f) {
            unsigned int dc = dim_color(c, 0.2f);
            if (sx > 0) set_pixel(sx - 1, sy, dc);
            if (sx < SW-1) set_pixel(sx + 1, sy, dc);
            if (sy > 0) set_pixel(sx, sy - 1, dc);
            if (sy < SH-1) set_pixel(sx, sy + 1, dc);
        }
    }
}

static void draw_copper(void) {
    for (int i = 0; i < COPPER_H; i++) {
        float hue = t * 60.0f + (float)i * 45.0f;
        float bri = 0.5f + sinf(t * 2.5f + (float)i * 1.2f) * 0.2f;
        draw_line_h(0, i, SW, hsv(hue, 0.7f, bri));
    }
}

static void draw_logo(void) {
    const char *logo = "PIXELITL";
    int nch = 8;
    int bx = (SW - nch * PXL_FONT_BIG_W) / 2;
    for (int i = 0; i < nch; i++) {
        float wave = sinf(t * 3.0f + (float)i * 0.8f) * 2.0f;
        float hue = t * 40.0f + (float)i * 35.0f;
        int cx = bx + i * PXL_FONT_BIG_W;
        int cy = LOGO_Y + (int)wave;
        draw_text_big(cx + 1, cy + 1, &logo[i], 1, rgb(8, 4, 20));
        draw_text_big(cx, cy, &logo[i], 1, hsv(hue, 0.35f, 1.0f));
    }
    // Subtitle
    float sp = sinf(t * 1.5f) * 0.15f + 0.55f;
    draw_str_centered(0, SW, 21, ">> FACE STORE <<", dim_color(rgb(0, 255, 200), sp));
}

static void draw_sep(int y, unsigned int c) {
    draw_line_h(0, y, SW, c);
    draw_line_h(0, y + 1, SW, dim_color(c, 0.4f));
}

static void draw_list(void) {
    // Panel
    fill_rect(LIST_X - 1, LIST_Y - 1, LIST_W + 2, VISIBLE * ITEM_H + 2, rgb(8, 6, 18));
    draw_rect(LIST_X - 1, LIST_Y - 1, LIST_W + 2, VISIBLE * ITEM_H + 2, rgb(30, 40, 70));

    int vis = cat_count - scroll_pos;
    if (vis > VISIBLE) vis = VISIBLE;

    // Raster highlight (subtle moving bar)
    float rp = sinf(t * 0.8f) * 0.5f + 0.5f;
    int raster_slot = (int)(rp * (float)(VISIBLE - 1));

    for (int i = 0; i < vis; i++) {
        int idx = scroll_pos + i;
        entry_t *e = &catalog[idx];
        int iy = LIST_Y + i * ITEM_H;
        int inst = e->installed;

        int hover = !inst && is_pressed() && _pxl_btn_inside(&item_btns[i]);

        // Background
        unsigned int bg;
        if (hover) bg = rgb(30, 25, 55);
        else if (i == raster_slot) bg = rgb(14, 12, 34);
        else bg = (i & 1) ? rgb(10, 8, 24) : rgb(12, 10, 28);
        fill_rect(LIST_X, iy, LIST_W, ITEM_H - 1, bg);

        // Left accent bar (color-cycling)
        float hue = (float)(idx * 47) + t * 10.0f;
        unsigned int accent = hsv(hue, 0.7f, inst ? 0.25f : 0.75f);
        fill_rect(LIST_X, iy, 2, ITEM_H - 1, accent);

        // Name
        unsigned int fg;
        if (inst) fg = rgb(50, 130, 70);
        else if (hover) fg = rgb(255, 255, 255);
        else fg = rgb(170, 175, 200);
        draw_text(LIST_X + 5, iy + 1, e->name, e->name_len, fg);

        // Status tag
        int tag_x = LIST_X + LIST_W - 24;
        if (inst) {
            draw_str(tag_x, iy + 1, "[OK]", rgb(40, 150, 60));
        } else {
            unsigned int ac = hover ? rgb(255, 220, 100) : dim_color(accent, 0.5f);
            draw_str(tag_x, iy + 1, "[>>]", ac);
        }
    }

    // Empty slots
    for (int i = vis; i < VISIBLE; i++) {
        int iy = LIST_Y + i * ITEM_H;
        fill_rect(LIST_X, iy, LIST_W, ITEM_H - 1, rgb(6, 5, 14));
        for (int d = 0; d < 3; d++)
            set_pixel(LIST_X + LIST_W/2 - 4 + d * 4, iy + 4, rgb(20, 18, 35));
    }

    // Scrollbar
    if (cat_count > VISIBLE) {
        int sbx = LIST_X + LIST_W + 1;
        int sbh = VISIBLE * ITEM_H;
        fill_rect(sbx, LIST_Y, 2, sbh, rgb(12, 10, 25));
        int th = (VISIBLE * sbh) / cat_count;
        if (th < 4) th = 4;
        int maxs = cat_count - VISIBLE;
        int thumb_y = LIST_Y + (maxs > 0 ? (scroll_pos * (sbh - th)) / maxs : 0);
        fill_rect(sbx, thumb_y, 2, th, hsv(t * 30.0f, 0.5f, 0.6f));
    }
}

static void draw_info(void) {
    fill_rect(INFO_X - 1, INFO_Y - 1, INFO_W + 2, 54, rgb(8, 6, 18));
    draw_rect(INFO_X - 1, INFO_Y - 1, INFO_W + 2, 54, rgb(30, 40, 70));

    // Header
    unsigned int hc = dim_color(rgb(0, 200, 180), 0.6f + sinf(t * 2.0f) * 0.15f);
    draw_str(INFO_X + 2, INFO_Y + 1, "STATUS", hc);
    draw_line_h(INFO_X, INFO_Y + 9, INFO_W, rgb(25, 35, 60));

    draw_str(INFO_X + 2, INFO_Y + 12, "FACES", rgb(80, 75, 120));
    draw_int(INFO_X + 38, INFO_Y + 12, face_count(), rgb(255, 200, 50));

    draw_str(INFO_X + 2, INFO_Y + 21, "AVAIL", rgb(80, 75, 120));
    draw_int(INFO_X + 38, INFO_Y + 21, cat_count, rgb(0, 230, 180));

    if (cat_count > 0) {
        draw_str(INFO_X + 2, INFO_Y + 30, "POS", rgb(80, 75, 120));
        char pg[8];
        int pl = pxl_itoa(scroll_pos + 1, pg, sizeof(pg));
        draw_text(INFO_X + 26, INFO_Y + 30, pg, pl, rgb(160, 130, 220));
    }

    // Connection dot
    int dy = INFO_Y + 42;
    float pulse = sinf(t * 4.0f) * 0.3f + 0.7f;
    if (state == S_READY || state == S_INSTALLING) {
        fill_rect(INFO_X + 2, dy + 1, 3, 3, dim_color(rgb(0, 255, 100), pulse));
        draw_str(INFO_X + 8, dy, "ONLINE", rgb(0, 180, 80));
    } else if (state == S_ERROR) {
        fill_rect(INFO_X + 2, dy + 1, 3, 3, dim_color(rgb(255, 50, 50), pulse));
        draw_str(INFO_X + 8, dy, "ERROR", rgb(255, 60, 60));
    } else {
        fill_rect(INFO_X + 2, dy + 1, 3, 3, dim_color(rgb(255, 200, 50), pulse));
        draw_str(INFO_X + 8, dy, "WAIT", rgb(200, 180, 100));
    }
}

static void draw_scroller(void) {
    int slen = pxl_strlen(SCROLLER);
    int start_ch = (int)(scroller_off / (float)PXL_FONT_W);
    float sub_off = scroller_off - (float)(start_ch * PXL_FONT_W);
    int nvis = SW / PXL_FONT_W + 2;

    for (int i = 0; i < nvis; i++) {
        int ci = (start_ch + i) % slen;
        int cx = (int)((float)(i * PXL_FONT_W) - sub_off);
        float wave = sinf(t * 4.0f + (float)(start_ch + i) * 0.25f) * 3.0f;
        int cy = SCROLL_Y + (int)wave;
        if (cx >= -PXL_FONT_W && cx < SW && cy >= 100 && cy < SH) {
            float hue = (float)(start_ch + i) * 7.0f + t * 50.0f;
            draw_text(cx, cy, &SCROLLER[ci], 1, hsv(hue, 0.45f, 0.85f));
        }
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    clear(rgb(5, 3, 14));
    draw_stars();
    draw_copper();
    draw_logo();
    draw_sep(24, rgb(35, 50, 85));

    if (state == S_WIFI) {
        fill_rect(35, 42, 130, 28, rgb(8, 6, 20));
        draw_rect(35, 42, 130, 28, rgb(35, 50, 85));
        float p = sinf(t * 3.0f) * 0.3f + 0.7f;
        unsigned int wc = dim_color(rgb(0, 200, 255), p);
        draw_str_centered(35, 130, 50, "AWAITING SIGNAL", wc);
        char dots[5];
        int nd = ((int)(t * 2.0f)) % 4;
        for (int d = 0; d < nd; d++) dots[d] = '.';
        dots[nd] = '\0';
        draw_str_centered(35, 130, 58, dots, wc);
    }
    else if (state == S_LOADING) {
        fill_rect(35, 42, 130, 32, rgb(8, 6, 20));
        draw_rect(35, 42, 130, 32, rgb(35, 50, 85));
        draw_str_centered(35, 130, 46, "LOADING INDEX", rgb(0, 200, 255));
        // Animated rainbow bar
        int bx = 45, bw = 110, by = 58;
        fill_rect(bx, by, bw, 5, rgb(12, 10, 25));
        draw_rect(bx, by, bw, 5, rgb(30, 40, 65));
        float pos = sinf(t * 5.0f) * 0.5f + 0.5f;
        int pw = 25;
        int px = bx + 1 + (int)(pos * (float)(bw - 2 - pw));
        for (int x = 0; x < pw; x++) {
            fill_rect(px + x, by + 1, 1, 3, hsv(t * 80.0f + (float)x * 5.0f, 0.6f, 0.7f));
        }
    }
    else if (state == S_ERROR) {
        fill_rect(30, 38, 140, 42, rgb(8, 6, 20));
        draw_rect(30, 38, 140, 42, rgb(70, 25, 25));
        // Glitch effect
        float glitch = sinf(t * 12.0f);
        int gx = (glitch > 0.9f) ? (rand() % 3 - 1) : 0;
        draw_str_big_centered(30 + gx, 140, 40, "ERROR", rgb(255, 50, 50));
        draw_text((SW - status_len * PXL_FONT_W) / 2, 58,
                  status_msg, status_len, rgb(160, 100, 100));
        float p = sinf(t * 2.0f) * 0.3f + 0.7f;
        draw_str_centered(30, 140, 68, "TAP TO RETRY", dim_color(rgb(0, 220, 200), p));
    }
    else if (state == S_INSTALLING) {
        fill_rect(20, 32, 160, 52, rgb(8, 6, 20));
        draw_rect(20, 32, 160, 52, rgb(45, 40, 85));
        if (install_idx >= 0 && install_idx < cat_count) {
            entry_t *e = &catalog[install_idx];
            float hue = (float)(install_idx * 47);
            // Raster bars
            for (int i = 0; i < 3; i++) {
                draw_line_h(21, 33 + i, 158,
                    hsv(hue + (float)i * 20.0f + t * 40.0f, 0.6f, 0.3f - (float)i * 0.08f));
            }
            draw_str_centered(20, 160, 40, e->name, rgb(255, 255, 255));
            // Progress bar
            int bx = 35, bw = 130, by = 54;
            fill_rect(bx, by, bw, 7, rgb(10, 8, 22));
            draw_rect(bx, by, bw, 7, rgb(35, 30, 65));
            if (last_ist == INSTALL_OK) {
                fill_rect(bx + 1, by + 1, bw - 2, 5, rgb(0, 255, 100));
            } else if (last_ist == INSTALL_ERROR) {
                fill_rect(bx + 1, by + 1, bw - 2, 5, rgb(200, 40, 40));
            } else {
                float apos = sinf(install_anim) * 0.5f + 0.5f;
                int pw = bw / 3;
                int px = bx + 1 + (int)(apos * (float)(bw - 2 - pw));
                for (int x = 0; x < pw; x++) {
                    float xf = (float)x / (float)pw;
                    fill_rect(px + x, by + 1, 1, 5,
                        hsv(hue + xf * 60.0f + t * 100.0f, 0.55f, 0.75f));
                }
            }
            unsigned int sc;
            if (last_ist == INSTALL_OK) sc = rgb(0, 255, 120);
            else if (last_ist == INSTALL_ERROR) sc = rgb(255, 50, 50);
            else sc = rgb(150, 140, 190);
            draw_text((SW - status_len * PXL_FONT_W) / 2, 68,
                      status_msg, status_len, sc);
        }
    }
    else {
        draw_list();
        draw_info();
        pxl_button_draw(&btn_up);
        pxl_button_draw(&btn_down);
        pxl_button_draw(&btn_refresh);
    }

    // Bottom separator + sine scroller
    draw_sep(100, rgb(35, 50, 85));
    draw_scroller();
}
