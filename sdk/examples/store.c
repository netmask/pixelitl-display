#include "pixelitl.h"

// ---- Utility ----

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
    for (int i = n - 1; i >= 0 && out < buf_sz; i--)
        buf[out++] = tmp[i];
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

#define MAX_CATALOG 10
#define MAX_NAME    20
#define MAX_FILE    32

typedef struct {
    char name[MAX_NAME];
    int  name_len;
    char filename[MAX_FILE];
    int  filename_len;
} catalog_entry_t;

static catalog_entry_t catalog[MAX_CATALOG];
static int catalog_count;

// ---- UI state ----

typedef enum {
    STATE_WAITING_WIFI,
    STATE_LOADING_INDEX,
    STATE_READY,
    STATE_INSTALLING,
    STATE_INDEX_ERROR,
} store_state_t;

static store_state_t state;
static int http_handle;        // handle for index.txt fetch
static int page;               // current page (0-based)

#define ENTRIES_PER_PAGE 4

static pxl_button_t entry_btns[ENTRIES_PER_PAGE];
static pxl_button_t btn_prev;
static pxl_button_t btn_next;
static pxl_button_t btn_refresh;

static int install_active;     // -1 = none, else catalog index
static int last_install_status;
static char status_msg[32];
static int status_len;

// Base URL
static char base_url[128];
static char full_url[256];

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

// Parse "name|filename\n..." into catalog[]
static void parse_index(const char *data, int len) {
    catalog_count = 0;
    int i = 0;
    while (i < len && catalog_count < MAX_CATALOG) {
        // Find '|' separator
        int name_start = i;
        while (i < len && data[i] != '|' && data[i] != '\n' && data[i] != '\r') i++;
        if (i >= len || data[i] != '|') {
            // skip malformed line
            while (i < len && data[i] != '\n') i++;
            if (i < len) i++; // skip \n
            continue;
        }
        int name_len = i - name_start;
        i++; // skip '|'

        int file_start = i;
        while (i < len && data[i] != '\n' && data[i] != '\r') i++;
        int file_len = i - file_start;
        // skip \r\n
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

// ---- UI setup helpers ----

static void setup_page_buttons(void) {
    unsigned int btn_bg = rgb(40, 50, 70);
    unsigned int btn_fg = rgb(200, 220, 255);

    int start = page * ENTRIES_PER_PAGE;
    int visible = catalog_count - start;
    if (visible > ENTRIES_PER_PAGE) visible = ENTRIES_PER_PAGE;

    for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
        if (i < visible) {
            catalog_entry_t *e = &catalog[start + i];
            // Convert name to upper for display (reuse name buffer — already stored)
            pxl_button(&entry_btns[i], 4, 20 + i * 16, 120, 13,
                        e->name, btn_bg, btn_fg);
        }
    }
}

static void fetch_index(void) {
    build_url("index.txt", 9);
    http_handle = http_get_str(full_url);
    state = STATE_LOADING_INDEX;
    set_status("LOADING...");
    catalog_count = 0;
    page = 0;
}

// ---- Exports ----

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(1);  // HD mode (160x96)

    // Load server URL from KV store
    int len = kv_get_str("server", base_url, sizeof(base_url));
    if (len <= 0) {
        pxl_strcpy(base_url, sizeof(base_url), "http://192.168.1.124:8080/");
    }

    unsigned int nav_bg = rgb(50, 50, 60);
    unsigned int nav_fg = rgb(180, 190, 210);
    pxl_button(&btn_prev,    4, 84, 30, 10, "<", nav_bg, nav_fg);
    pxl_button(&btn_next,   38, 84, 30, 10, ">", nav_bg, nav_fg);
    pxl_button(&btn_refresh, 110, 84, 46, 10, "REFRESH", rgb(50, 60, 50), rgb(180, 220, 180));

    install_active = -1;
    last_install_status = INSTALL_IDLE;

    // Wait for WiFi — app_wifi_ready callback will trigger the fetch
    state = STATE_WAITING_WIFI;
    set_status("WAITING WIFI...");
}

// Called by runtime when WiFi connects — start fetching index
__attribute__((export_name("app_wifi_ready")))
void app_wifi_ready(void) {
    if (state == STATE_WAITING_WIFI) {
        fetch_index();
    }
}

static char index_buf[1024];

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    // Still waiting for WiFi callback — nothing to do
    if (state == STATE_WAITING_WIFI) return;

    // ---- Index fetch state machine ----
    if (state == STATE_LOADING_INDEX) {
        int st = http_status(http_handle);
        if (st == HTTP_DONE) {
            int resp_code = http_response_code(http_handle);
            if (resp_code == 200) {
                int body_len = http_read(http_handle, index_buf, sizeof(index_buf) - 1);
                if (body_len > 0) {
                    index_buf[body_len] = '\0';
                    parse_index(index_buf, body_len);
                }
                http_close(http_handle);
                http_handle = -1;

                if (catalog_count > 0) {
                    state = STATE_READY;
                    page = 0;
                    setup_page_buttons();
                    set_status("READY");
                } else {
                    state = STATE_INDEX_ERROR;
                    set_status("EMPTY INDEX");
                }
            } else {
                http_close(http_handle);
                http_handle = -1;
                state = STATE_INDEX_ERROR;
                set_status("HTTP ERR");
            }
        } else if (st == HTTP_ERROR) {
            http_close(http_handle);
            http_handle = -1;
            state = STATE_INDEX_ERROR;
            set_status("FETCH FAIL");
        }
        return;
    }

    // ---- Install polling ----
    if (install_active >= 0) {
        int ist = face_install_status();
        if (ist != last_install_status) {
            last_install_status = ist;
            if (ist == INSTALL_DOWNLOADING) set_status("DOWNLOADING...");
            else if (ist == INSTALL_LOADING)  set_status("LOADING...");
            else if (ist == INSTALL_OK)       set_status("INSTALLED!");
            else if (ist == INSTALL_ERROR)    set_status("ERROR!");
        }
        if ((ist == INSTALL_OK || ist == INSTALL_ERROR) && (frame % 100 == 0)) {
            face_install_reset();
            install_active = -1;
            last_install_status = INSTALL_IDLE;
            set_status("READY");
        }
        return;
    }

    // ---- Button handling (READY state) ----
    if (state != STATE_READY) {
        // In error state, only refresh is active
        if (pxl_button_clicked(&btn_refresh)) {
            fetch_index();
        }
        return;
    }

    int start = page * ENTRIES_PER_PAGE;
    int visible = catalog_count - start;
    if (visible > ENTRIES_PER_PAGE) visible = ENTRIES_PER_PAGE;

    for (int i = 0; i < visible; i++) {
        if (pxl_button_clicked(&entry_btns[i])) {
            int ci = start + i;
            build_url(catalog[ci].filename, catalog[ci].filename_len);
            int ret = face_install(full_url, pxl_strlen(full_url),
                                    catalog[ci].name, catalog[ci].name_len);
            if (ret == 0) {
                install_active = ci;
                last_install_status = INSTALL_DOWNLOADING;
                state = STATE_INSTALLING;
                set_status("STARTING...");
            } else {
                set_status("BUSY/FULL");
            }
            return;
        }
    }

    int total_pages = (catalog_count + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
    if (pxl_button_clicked(&btn_prev) && page > 0) {
        page--;
        setup_page_buttons();
    }
    if (pxl_button_clicked(&btn_next) && page < total_pages - 1) {
        page++;
        setup_page_buttons();
    }
    if (pxl_button_clicked(&btn_refresh)) {
        fetch_index();
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    clear(rgb(10, 12, 20));

    // Title
    draw_str_big(4, 2, "FACE STORE", rgb(255, 220, 100));

    // Face count
    int total = face_count();
    char cnt_buf[8];
    int cnt_len = itoa_simple(total, cnt_buf, sizeof(cnt_buf));
    draw_str(110, 6, "FACES:", rgb(150, 150, 150));
    draw_text(146, 6, cnt_buf, cnt_len, rgb(200, 200, 200));

    if (state == STATE_WAITING_WIFI) {
        draw_str(20, 44, "WAITING FOR WIFI...", rgb(200, 200, 100));
    } else if (state == STATE_LOADING_INDEX) {
        draw_str(20, 44, "FETCHING INDEX...", rgb(100, 200, 255));
    } else if (state == STATE_INDEX_ERROR) {
        draw_str(20, 40, "COULD NOT LOAD", rgb(255, 100, 100));
        draw_str(20, 50, "INDEX", rgb(255, 100, 100));
        pxl_button_draw(&btn_refresh);
    } else {
        // Draw catalog entries for current page
        int start = page * ENTRIES_PER_PAGE;
        int visible = catalog_count - start;
        if (visible > ENTRIES_PER_PAGE) visible = ENTRIES_PER_PAGE;

        for (int i = 0; i < visible; i++) {
            pxl_button_draw(&entry_btns[i]);
        }

        // Page indicator
        int total_pages = (catalog_count + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
        if (total_pages > 1) {
            pxl_button_draw(&btn_prev);
            pxl_button_draw(&btn_next);

            // "1/2" page number
            char pg_buf[8];
            int pg_len = 0;
            pg_len += itoa_simple(page + 1, pg_buf + pg_len, sizeof(pg_buf) - pg_len);
            pg_buf[pg_len++] = '/';
            pg_len += itoa_simple(total_pages, pg_buf + pg_len, sizeof(pg_buf) - pg_len);
            draw_text(72, 86, pg_buf, pg_len, rgb(150, 150, 150));
        }

        pxl_button_draw(&btn_refresh);
    }

    // Status bar
    unsigned int sc = rgb(150, 150, 150);
    if (last_install_status == INSTALL_OK)           sc = rgb(100, 255, 100);
    else if (last_install_status == INSTALL_ERROR)    sc = rgb(255, 100, 100);
    else if (last_install_status == INSTALL_DOWNLOADING) sc = rgb(100, 200, 255);
    else if (last_install_status == INSTALL_LOADING)  sc = rgb(255, 200, 100);

    draw_str(4, 74, "STATUS:", rgb(150, 150, 150));
    draw_text(46, 74, status_msg, status_len, sc);
}
