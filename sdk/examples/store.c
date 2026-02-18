#include "pixelitl.h"

// ---- Utility ----

static int itoa_simple(int val, char *buf, int buf_sz) {
    if (buf_sz < 2) return 0;
    if (val == 0) { buf[0] = '0'; return 1; }
    int neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    char tmp[12];
    int n = 0;
    while (val > 0 && n < 11) { tmp[n++] = '0' + (val % 10); val /= 10; }
    int out = 0;
    if (neg && out < buf_sz) buf[out++] = '-';
    for (int i = n - 1; i >= 0 && out < buf_sz; i--) buf[out++] = tmp[i];
    return out;
}

static int pxl_strcpy(char *dst, int dst_sz, const char *src) {
    int i = 0;
    while (src[i] && i < dst_sz - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
    return i;
}

static int pxl_memcpy(char *dst, const char *src, int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
    return n;
}

// ---- Catalog ----

#define MAX_CATALOG 20
#define MAX_NAME    20
#define MAX_FILE    32

typedef struct {
    char name[MAX_NAME];
    int  name_len;
    char filename[MAX_FILE];
    int  filename_len;
    int  installed;
} catalog_entry_t;

static catalog_entry_t catalog[MAX_CATALOG];
static int catalog_count;

// ---- List layout ----

#define LIST_TOP    20
#define LIST_LEFT   16
#define LIST_W      128
#define ITEM_H      13
#define VISIBLE     5

// ---- Stars background ----

#define NUM_STARS 25
static float star_x[NUM_STARS];
static float star_y[NUM_STARS];
static float star_speed[NUM_STARS];
static float star_bri[NUM_STARS];

// ---- State ----

typedef enum {
    STATE_WAITING_WIFI,
    STATE_LOADING_INDEX,
    STATE_READY,
    STATE_INSTALLING,
    STATE_INDEX_ERROR,
} store_state_t;

static store_state_t state;
static int http_handle;
static int scroll;
static float t;

static int install_active;
static int last_install_status;
static char status_msg[32];
static int status_len;
static float install_anim;

// Accent colors for items
static unsigned int item_colors[6];

// Buttons
static pxl_button_t item_btns[VISIBLE];
static pxl_button_t btn_up;
static pxl_button_t btn_down;
static pxl_button_t btn_refresh;

// Base URL
static char base_url[128];
static char full_url[256];

static int screenW, screenH;

static void set_status(const char *msg) {
    status_len = pxl_strcpy(status_msg, sizeof(status_msg), msg);
}

static void build_url(const char *path, int path_len) {
    int base_len = pxl_strlen(base_url);
    int i = pxl_memcpy(full_url, base_url, base_len);
    i += pxl_memcpy(full_url + i, path, path_len < 255 - i ? path_len : 255 - i);
    full_url[i] = '\0';
}

// ---- Index parsing ----

static void parse_index(const char *data, int len) {
    catalog_count = 0;
    int i = 0;
    while (i < len && catalog_count < MAX_CATALOG) {
        int name_start = i;
        while (i < len && data[i] != '|' && data[i] != '\n' && data[i] != '\r') i++;
        if (i >= len || data[i] != '|') {
            while (i < len && data[i] != '\n') i++;
            if (i < len) i++;
            continue;
        }
        int name_len = i - name_start;
        i++;
        int file_start = i;
        while (i < len && data[i] != '\n' && data[i] != '\r') i++;
        int file_len = i - file_start;
        while (i < len && (data[i] == '\n' || data[i] == '\r')) i++;

        if (name_len > 0 && name_len < MAX_NAME && file_len > 0 && file_len < MAX_FILE) {
            catalog_entry_t *e = &catalog[catalog_count];
            pxl_memcpy(e->name, data + name_start, name_len);
            e->name[name_len] = '\0';
            e->name_len = name_len;
            pxl_memcpy(e->filename, data + file_start, file_len);
            e->filename[file_len] = '\0';
            e->filename_len = file_len;
            catalog_count++;
        }
    }
}

static int pxl_streq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void refresh_installed(void) {
    int n = face_count();
    char name_buf[MAX_NAME];
    for (int c = 0; c < catalog_count; c++) {
        catalog[c].installed = 0;
        for (int f = 0; f < n; f++) {
            int len = face_get_name(f, name_buf, sizeof(name_buf));
            if (len > 0 && pxl_streq(catalog[c].name, name_buf)) {
                catalog[c].installed = 1;
                break;
            }
        }
    }
}

static void setup_item_buttons(void) {
    int visible = catalog_count - scroll;
    if (visible > VISIBLE) visible = VISIBLE;

    for (int i = 0; i < VISIBLE; i++) {
        int iy = LIST_TOP + i * ITEM_H;
        if (i < visible) {
            int idx = scroll + i;
            unsigned int ac = item_colors[idx % 6];
            pxl_button(&item_btns[i], LIST_LEFT, iy, LIST_W, ITEM_H - 1,
                       catalog[idx].name, dim_color(ac, 0.15f), rgb(200, 205, 230));
            item_btns[i].pressed_bg = dim_color(ac, 0.35f);
            item_btns[i].pressed_fg = rgb(255, 255, 255);
        }
    }
}

static void fetch_index(void) {
    build_url("index.txt", 9);
    http_handle = http_get_str(full_url);
    state = STATE_LOADING_INDEX;
    set_status("CONNECTING...");
    catalog_count = 0;
    scroll = 0;
}

static void init_stars(void) {
    for (int i = 0; i < NUM_STARS; i++) {
        star_x[i] = (float)(rand() % screenW);
        star_y[i] = (float)(rand() % screenH);
        star_speed[i] = 0.002f + (float)(rand() % 10) * 0.002f;
        star_bri[i] = 0.2f + (float)(rand() % 60) * 0.01f;
    }
}

// ---- Exports ----

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(1);  // HD 160x96
    t = 0.0f;
    screenW = width();
    screenH = height();
    srand((int)millis());

    int len = kv_get_str("server", base_url, sizeof(base_url));
    if (len <= 0) {
        pxl_strcpy(base_url, sizeof(base_url), "http://192.168.1.124:4000/api/faces/");
    }

    item_colors[0] = rgb(80, 160, 255);
    item_colors[1] = rgb(255, 120, 70);
    item_colors[2] = rgb(80, 220, 130);
    item_colors[3] = rgb(220, 100, 240);
    item_colors[4] = rgb(255, 210, 50);
    item_colors[5] = rgb(70, 210, 230);

    // Scroll buttons (left of list)
    unsigned int nav_bg = rgb(25, 22, 48);
    unsigned int nav_fg = rgb(120, 110, 170);
    pxl_button(&btn_up,   4, LIST_TOP, 10, 30, "^", nav_bg, nav_fg);
    pxl_button(&btn_down,  4, LIST_TOP + 35, 10, 30, "v", nav_bg, nav_fg);

    // Refresh button (bottom)
    pxl_button(&btn_refresh, 55, 88, 50, 8, "REFRESH", rgb(25, 22, 48), rgb(100, 160, 100));

    install_active = -1;
    last_install_status = INSTALL_IDLE;
    install_anim = 0.0f;
    scroll = 0;

    init_stars();

    state = STATE_WAITING_WIFI;
    set_status("WAITING WIFI...");
}

__attribute__((export_name("app_wifi_ready")))
void app_wifi_ready(void) {
    if (state == STATE_WAITING_WIFI) {
        fetch_index();
    }
}

static char index_buf[2048];

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    t += (float)dt * 0.001f;
    float dtf = (float)dt;

    // Animate stars
    for (int i = 0; i < NUM_STARS; i++) {
        star_x[i] -= star_speed[i] * dtf;
        if (star_x[i] < -1.0f) {
            star_x[i] = (float)screenW + (float)(rand() % 8);
            star_y[i] = (float)(rand() % screenH);
            star_speed[i] = 0.002f + (float)(rand() % 10) * 0.002f;
            star_bri[i] = 0.2f + (float)(rand() % 60) * 0.01f;
        }
    }

    if (state == STATE_WAITING_WIFI) return;

    // ---- Index fetch ----
    if (state == STATE_LOADING_INDEX) {
        int st = http_status(http_handle);
        if (st == HTTP_DONE) {
            int resp = http_response_code(http_handle);
            if (resp == 200) {
                int body_len = http_read(http_handle, index_buf, sizeof(index_buf) - 1);
                if (body_len > 0) {
                    index_buf[body_len] = '\0';
                    parse_index(index_buf, body_len);
                }
                http_close(http_handle);
                http_handle = -1;
                if (catalog_count > 0) {
                    state = STATE_READY;
                    scroll = 0;
                    refresh_installed();
                    setup_item_buttons();
                    set_status("");
                } else {
                    state = STATE_INDEX_ERROR;
                    set_status("NO FACES FOUND");
                }
            } else {
                http_close(http_handle);
                http_handle = -1;
                state = STATE_INDEX_ERROR;
                set_status("SERVER ERROR");
            }
        } else if (st == HTTP_ERROR) {
            http_close(http_handle);
            http_handle = -1;
            state = STATE_INDEX_ERROR;
            set_status("CONNECTION FAILED");
        }
        return;
    }

    // ---- Install polling ----
    if (install_active >= 0) {
        install_anim += (float)dt * 0.003f;
        if (install_anim > 1.0f) install_anim -= 1.0f;

        int ist = face_install_status();
        if (ist != last_install_status) {
            last_install_status = ist;
            if (ist == INSTALL_DOWNLOADING) set_status("DOWNLOADING...");
            else if (ist == INSTALL_LOADING) set_status("LOADING...");
            else if (ist == INSTALL_OK) set_status("INSTALLED!");
            else if (ist == INSTALL_ERROR) set_status("FAILED!");
        }
        if (ist == INSTALL_OK && (frame % 90 == 0)) {
            int new_face = face_count() - 1;
            face_install_reset();
            install_active = -1;
            last_install_status = INSTALL_IDLE;
            face_switch(new_face);
            return;
        }
        if (ist == INSTALL_ERROR && (frame % 120 == 0)) {
            face_install_reset();
            install_active = -1;
            last_install_status = INSTALL_IDLE;
            state = STATE_READY;
            refresh_installed();
            setup_item_buttons();
            set_status("");
        }
        return;
    }

    // ---- Error state ----
    if (state == STATE_INDEX_ERROR) {
        if (pxl_button_clicked(&btn_refresh)) fetch_index();
        return;
    }
    if (state != STATE_READY) return;

    // ---- Swipe up/down to scroll ----
    int gesture = get_gesture();
    if (gesture == GESTURE_SWIPE_UP && scroll + VISIBLE < catalog_count) {
        scroll++;
        setup_item_buttons();
    }
    if (gesture == GESTURE_SWIPE_DOWN && scroll > 0) {
        scroll--;
        setup_item_buttons();
    }

    // ---- Item button clicks ----
    int visible = catalog_count - scroll;
    if (visible > VISIBLE) visible = VISIBLE;

    for (int i = 0; i < visible; i++) {
        if (pxl_button_clicked(&item_btns[i])) {
            int idx = scroll + i;
            if (catalog[idx].installed) break;  // already installed, ignore
            build_url(catalog[idx].filename, catalog[idx].filename_len);
            int ret = face_install(full_url, pxl_strlen(full_url),
                                   catalog[idx].name, catalog[idx].name_len);
            if (ret == 0) {
                install_active = idx;
                last_install_status = INSTALL_DOWNLOADING;
                state = STATE_INSTALLING;
                install_anim = 0.0f;
                set_status("STARTING...");
            }
            return;
        }
    }

    // ---- Scroll buttons ----
    if (pxl_button_clicked(&btn_up) && scroll > 0) {
        scroll--;
        setup_item_buttons();
    }
    if (pxl_button_clicked(&btn_down) && scroll + VISIBLE < catalog_count) {
        scroll++;
        setup_item_buttons();
    }

    // ---- Refresh ----
    if (pxl_button_clicked(&btn_refresh)) {
        fetch_index();
    }
}

// ---- Drawing ----

static void draw_stars(void) {
    for (int i = 0; i < NUM_STARS; i++) {
        int sx = (int)star_x[i];
        int sy = (int)star_y[i];
        if (sx < 0 || sx >= screenW || sy < 0 || sy >= screenH) continue;

        float twinkle = sinf(t * 2.0f + star_bri[i] * 20.0f) * 0.3f + 0.7f;
        float bri = star_bri[i] * twinkle;
        unsigned int c = dim_color(rgb(180, 200, 255), bri);
        set_pixel(sx, sy, c);

        if (star_bri[i] > 0.6f && twinkle > 0.8f) {
            unsigned int dc = dim_color(c, 0.3f);
            if (sx > 0) set_pixel(sx - 1, sy, dc);
            if (sx < screenW - 1) set_pixel(sx + 1, sy, dc);
        }
    }
}

static void draw_mountains(void) {
    unsigned int m1 = rgb(15, 18, 35);
    unsigned int m2 = rgb(10, 12, 28);

    int peaks1[] = {92,88,82,78,80,76,72,75,79,83,80,77,74,78,82,86,90,92};
    for (int i = 0; i < 17; i++) {
        int x = i * 10;
        for (int px = 0; px < 10; px++) {
            int h = peaks1[i] + (peaks1[i+1] - peaks1[i]) * px / 10;
            fill_rect(x + px, h, 1, screenH - h, m2);
        }
    }

    int peaks2[] = {94,90,87,84,88,86,82,85,89,91,86,84,87,90,93,94,95,94};
    for (int i = 0; i < 17; i++) {
        int x = i * 10;
        for (int px = 0; px < 10; px++) {
            int h = peaks2[i] + (peaks2[i+1] - peaks2[i]) * px / 10;
            fill_rect(x + px, h, 1, screenH - h, m1);
        }
    }
}

static void draw_item_styled(int slot) {
    int idx = scroll + slot;
    if (idx >= catalog_count) return;

    pxl_button_t *btn = &item_btns[slot];
    unsigned int ac = item_colors[idx % 6];
    int inst = catalog[idx].installed;
    int pressed = !inst && is_pressed() && _pxl_btn_inside(btn);

    // Background
    unsigned int bg;
    if (inst) bg = dim_color(rgb(30, 35, 30), 0.5f);
    else bg = pressed ? dim_color(ac, 0.35f) : dim_color(ac, 0.12f);
    fill_rect(btn->x, btn->y, btn->w, btn->h, bg);

    // Left accent bar
    unsigned int bar = inst ? rgb(40, 120, 60) : (pressed ? ac : dim_color(ac, 0.6f));
    fill_rect(btn->x, btn->y, 2, btn->h, bar);

    // Border bottom
    draw_line_h(btn->x, btn->y + btn->h, btn->w, rgb(20, 18, 38));

    // Name text
    unsigned int fg;
    if (inst) fg = rgb(80, 140, 90);
    else fg = pressed ? rgb(255, 255, 255) : rgb(200, 205, 230);
    draw_str(btn->x + 6, btn->y + 3, catalog[idx].name, fg);

    // Installed checkmark
    if (inst) {
        draw_str(btn->x + btn->w - 10, btn->y + 3, "*", rgb(60, 180, 80));
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    // Sky gradient
    for (int y = 0; y < screenH; y++) {
        draw_line_h(0, y, screenW, rgb(5 + y/12, 4 + y/16, 18 + y/6));
    }

    draw_mountains();
    draw_stars();

    // === Header ===
    fill_rect(0, 0, screenW, 17, dim_color(rgb(10, 8, 25), 0.92f));
    draw_line_h(0, 17, screenW, rgb(45, 38, 75));

    float hue = t * 15.0f;
    draw_str_big(3, 0, "PIXELITL", hsv(hue, 0.3f, 1.0f));

    int total = face_count();
    char cnt_buf[4];
    int cnt_len = itoa_simple(total, cnt_buf, sizeof(cnt_buf));
    fill_rect(screenW - 24, 4, 20, 9, rgb(30, 25, 55));
    draw_text(screenW - 20, 5, cnt_buf, cnt_len, rgb(160, 140, 220));

    // === Content ===

    if (state == STATE_WAITING_WIFI) {
        fill_rect(20, 35, 120, 25, dim_color(rgb(10, 8, 25), 0.88f));
        draw_rect(20, 35, 120, 25, rgb(40, 35, 70));
        float pulse = sinf(t * 3.0f) * 0.3f + 0.7f;
        unsigned int wc = dim_color(rgb(100, 180, 255), pulse);
        int dots = ((int)(t * 2.0f)) % 4;
        draw_str(32, 43, "CONNECTING", wc);
        for (int d = 0; d < dots; d++)
            draw_str(92 + d * 6, 43, ".", wc);
    }
    else if (state == STATE_LOADING_INDEX) {
        fill_rect(20, 35, 120, 25, dim_color(rgb(10, 8, 25), 0.88f));
        draw_rect(20, 35, 120, 25, rgb(40, 35, 70));
        draw_str(40, 38, "LOADING", rgb(100, 180, 255));
        int dots = ((int)(t * 3.0f)) % 4;
        for (int d = 0; d < dots; d++)
            draw_str(82 + d * 6, 38, ".", rgb(100, 180, 255));
    }
    else if (state == STATE_INDEX_ERROR) {
        fill_rect(20, 30, 120, 35, dim_color(rgb(10, 8, 25), 0.88f));
        draw_rect(20, 30, 120, 35, rgb(60, 30, 30));
        draw_str(52, 36, "OOPS!", rgb(255, 80, 80));
        draw_text(24, 48, status_msg, status_len, rgb(120, 100, 140));
        pxl_button_draw(&btn_refresh);
    }
    else if (state == STATE_INSTALLING) {
        fill_rect(15, 25, 130, 46, dim_color(rgb(10, 8, 25), 0.92f));
        draw_rect(15, 25, 130, 46, rgb(50, 45, 85));

        if (install_active >= 0 && install_active < catalog_count) {
            catalog_entry_t *e = &catalog[install_active];
            unsigned int ac = item_colors[install_active % 6];

            fill_rect(16, 26, 128, 3, ac);

            int npx = (screenW - e->name_len * 6) / 2;
            draw_str(npx, 34, e->name, rgb(255, 255, 255));

            int bx = 28, bw = 104, by = 48;
            fill_rect(bx, by, bw, 5, rgb(12, 10, 25));
            draw_rect(bx, by, bw, 5, rgb(45, 40, 75));

            if (last_install_status == INSTALL_OK) {
                fill_rect(bx+1, by+1, bw-2, 3, rgb(70, 255, 120));
            } else if (last_install_status == INSTALL_ERROR) {
                fill_rect(bx+1, by+1, bw-2, 3, rgb(255, 70, 70));
            } else {
                int hw = bw/3;
                float pos = (sinf(install_anim * 6.28f) + 1.0f) * 0.5f;
                fill_rect(bx+1+(int)(pos*(float)(bw-2-hw)), by+1, hw, 3, dim_color(ac, 0.8f));
            }

            unsigned int sc = rgb(140, 130, 170);
            if (last_install_status == INSTALL_OK) sc = rgb(70, 255, 120);
            else if (last_install_status == INSTALL_ERROR) sc = rgb(255, 70, 70);
            draw_text((screenW - status_len*6)/2, 58, status_msg, status_len, sc);
        }
    }
    else {
        // === READY: List with buttons ===

        // List panel background
        fill_rect(LIST_LEFT - 2, LIST_TOP - 2, LIST_W + 4, VISIBLE * ITEM_H + 4,
                  dim_color(rgb(8, 6, 20), 0.88f));
        draw_rect(LIST_LEFT - 2, LIST_TOP - 2, LIST_W + 4, VISIBLE * ITEM_H + 4,
                  rgb(35, 30, 60));

        // Draw list items
        int visible = catalog_count - scroll;
        if (visible > VISIBLE) visible = VISIBLE;

        for (int i = 0; i < visible; i++) {
            draw_item_styled(i);
        }

        // Draw empty slots
        for (int i = visible; i < VISIBLE; i++) {
            int iy = LIST_TOP + i * ITEM_H;
            fill_rect(LIST_LEFT, iy, LIST_W, ITEM_H - 1, rgb(10, 8, 22));
        }

        // Scroll buttons
        int can_up = (scroll > 0);
        int can_down = (scroll + VISIBLE < catalog_count);

        // Up button
        unsigned int up_fg = can_up ? rgb(150, 140, 200) : rgb(40, 35, 60);
        fill_rect(btn_up.x, btn_up.y, btn_up.w, btn_up.h, rgb(18, 16, 35));
        draw_rect(btn_up.x, btn_up.y, btn_up.w, btn_up.h, rgb(35, 30, 60));
        draw_str(btn_up.x + 2, btn_up.y + 12, "^", up_fg);

        // Down button
        unsigned int dn_fg = can_down ? rgb(150, 140, 200) : rgb(40, 35, 60);
        fill_rect(btn_down.x, btn_down.y, btn_down.w, btn_down.h, rgb(18, 16, 35));
        draw_rect(btn_down.x, btn_down.y, btn_down.w, btn_down.h, rgb(35, 30, 60));
        draw_str(btn_down.x + 2, btn_down.y + 12, "v", dn_fg);

        // Scrollbar
        if (catalog_count > VISIBLE) {
            int tx = LIST_LEFT + LIST_W + 3;
            int th = VISIBLE * ITEM_H;
            fill_rect(tx, LIST_TOP, 2, th, rgb(15, 12, 30));
            int thumb_h = (VISIBLE * th) / catalog_count;
            if (thumb_h < 4) thumb_h = 4;
            int max_s = catalog_count - VISIBLE;
            int thumb_y = LIST_TOP + (max_s > 0 ? (scroll * (th - thumb_h)) / max_s : 0);
            fill_rect(tx, thumb_y, 2, thumb_h, rgb(80, 70, 140));
        }

        // Refresh + info
        pxl_button_draw(&btn_refresh);

        char info[8];
        int ilen = 0;
        ilen += itoa_simple(catalog_count, info + ilen, sizeof(info) - ilen);
        draw_text(screenW/2 - ilen*3 - 12, 90, info, ilen, rgb(50, 45, 80));
        draw_str(screenW/2 - ilen*3 - 12 + ilen*6, 90, " DL", rgb(50, 45, 80));
    }
}
