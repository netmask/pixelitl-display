#include "pixelitl.h"

// Open-Meteo API — Montreal (45.50, -73.57), no API key needed
static const char API_URL[] =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=45.50&longitude=-73.57"
    "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m";

// State
static int  req_handle;
static int  fetch_state;  // 0=idle, 1=waiting, 2=done, 3=error
static int  refresh_timer;

// Parsed weather data
static int  temp_int;       // integer part
static int  temp_frac;      // one decimal digit
static int  temp_negative;
static int  humidity;
static int  weather_code;
static int  wind_speed;
static char condition[20];
static int  condition_len;

// Response buffer in WASM linear memory
static char resp_buf[4096];

// --- Helpers (no libc) ---

static int pxl_memcmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return 1;
    }
    return 0;
}

// Find needle in haystack, return pointer or 0
static const char *find_str(const char *hay, int hay_len, const char *needle, int needle_len) {
    for (int i = 0; i <= hay_len - needle_len; i++) {
        if (pxl_memcmp(hay + i, needle, needle_len) == 0)
            return hay + i;
    }
    return 0;
}

// Parse a number after a colon, e.g. "temperature_2m":5.2 or "weather_code":3
// Returns chars consumed
static int parse_number(const char *s, int len, int *integer, int *frac, int *is_neg) {
    int i = 0;
    *integer = 0;
    *frac = 0;
    *is_neg = 0;

    if (i < len && s[i] == '-') { *is_neg = 1; i++; }
    while (i < len && s[i] >= '0' && s[i] <= '9') {
        *integer = *integer * 10 + (s[i] - '0');
        i++;
    }
    if (i < len && s[i] == '.') {
        i++;
        if (i < len && s[i] >= '0' && s[i] <= '9') {
            *frac = s[i] - '0';
            i++;
        }
    }
    return i;
}

// Extract JSON number value for a given key
static int extract_json_number(const char *json, int json_len,
                                const char *key, int key_len,
                                int *integer, int *frac, int *is_neg) {
    const char *pos = find_str(json, json_len, key, key_len);
    if (!pos) return 0;
    // Skip past key and the ":
    const char *after = pos + key_len;
    int remaining = json_len - (int)(after - json);
    // skip whitespace/colon
    int i = 0;
    while (i < remaining && (after[i] == '"' || after[i] == ':' || after[i] == ' ')) i++;
    parse_number(after + i, remaining - i, integer, frac, is_neg);
    return 1;
}

// Map WMO weather code to description
static void weather_code_to_str(int code) {
    const char *s;
    int len;
    #define WC(c, txt) case c: s = txt; len = sizeof(txt)-1; break
    switch (code) {
        WC(0,  "Clear");
        WC(1,  "Mostly Clear");
        WC(2,  "Partly Cloudy");
        WC(3,  "Overcast");
        WC(45, "Fog");
        WC(48, "Rime Fog");
        WC(51, "Light Drizzle");
        WC(53, "Drizzle");
        WC(55, "Heavy Drizzle");
        WC(61, "Light Rain");
        WC(63, "Rain");
        WC(65, "Heavy Rain");
        WC(66, "Freezing Rain");
        WC(67, "Hvy Frzn Rain");
        WC(71, "Light Snow");
        WC(73, "Snow");
        WC(75, "Heavy Snow");
        WC(77, "Snow Grains");
        WC(80, "Rain Showers");
        WC(81, "Mod Showers");
        WC(82, "Hvy Showers");
        WC(85, "Snow Showers");
        WC(86, "Hvy Snow Shwr");
        WC(95, "Thunderstorm");
        WC(96, "T-Storm + Hail");
        WC(99, "Hvy T-Storm");
        default: s = "Unknown"; len = 7; break;
    }
    #undef WC
    for (int i = 0; i < len && i < 19; i++) condition[i] = s[i];
    condition[len] = '\0';
    condition_len = len;
}

// Integer to string with sign, returns length
static int int_to_buf(int val, int is_neg, char *buf) {
    int i = 0;
    if (is_neg && val > 0) buf[i++] = '-';
    // max 3 digits for temperature
    if (val >= 100) buf[i++] = '0' + (val / 100) % 10;
    if (val >= 10)  buf[i++] = '0' + (val / 10) % 10;
    buf[i++] = '0' + val % 10;
    return i;
}

static void parse_response(const char *data, int len) {
    int dummy_frac = 0, dummy_neg = 0;

    if (extract_json_number(data, len, "temperature_2m", 14,
                            &temp_int, &temp_frac, &temp_negative)) {
        // ok
    }

    if (extract_json_number(data, len, "relative_humidity_2m", 20,
                            &humidity, &dummy_frac, &dummy_neg)) {
        // ok
    }

    if (extract_json_number(data, len, "weather_code", 12,
                            &weather_code, &dummy_frac, &dummy_neg)) {
        weather_code_to_str(weather_code);
    }

    if (extract_json_number(data, len, "wind_speed_10m", 14,
                            &wind_speed, &dummy_frac, &dummy_neg)) {
        // ok
    }
}

static void start_fetch(void) {
    req_handle = http_get_str(API_URL);
    if (req_handle >= 0) {
        fetch_state = 1;
    } else {
        fetch_state = 3;
    }
}

// --- Face API ---

__attribute__((export_name("app_init")))
void app_init(void) {
    req_handle = -1;
    fetch_state = 0;
    refresh_timer = 0;
    temp_int = 0;
    temp_frac = 0;
    temp_negative = 0;
    humidity = 0;
    weather_code = -1;
    wind_speed = 0;
    condition[0] = '\0';
    condition_len = 0;
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    // Start first fetch once WiFi is up, then every ~60s
    if (fetch_state == 0) {
        if (wifi_status()) {
            start_fetch();
            refresh_timer = 0;
        }
    } else if (fetch_state == 1) {
        // Poll HTTP status
        int st = http_status(req_handle);
        if (st == HTTP_DONE) {
            int len = http_response_len(req_handle);
            if (len > 0) {
                if (len > (int)sizeof(resp_buf) - 1) len = (int)sizeof(resp_buf) - 1;
                int got = http_read(req_handle, resp_buf, len);
                if (got > 0) {
                    resp_buf[got] = '\0';
                    parse_response(resp_buf, got);
                    fetch_state = 2;
                } else {
                    fetch_state = 3;
                }
            } else {
                fetch_state = 3;
            }
            http_close(req_handle);
            req_handle = -1;
        } else if (st == HTTP_ERROR) {
            http_close(req_handle);
            req_handle = -1;
            fetch_state = 3;
        }
    } else {
        // Refresh every ~60 seconds
        refresh_timer += dt;
        if (refresh_timer > 60000) {
            refresh_timer = 0;
            fetch_state = 0;
        }
    }
}

// --- Drawing helpers ---

// Draw a simple sun icon (circle + rays)
static void draw_sun(int cx, int cy, unsigned int col) {
    fill_circle(cx, cy, 4, col);
    // rays
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 0.785f; // pi/4
        int rx = cx + (int)(sinf(a) * 7.0f);
        int ry = cy - (int)(cosf(a) * 7.0f);
        set_pixel(rx, ry, col);
        rx = cx + (int)(sinf(a) * 8.0f);
        ry = cy - (int)(cosf(a) * 8.0f);
        set_pixel(rx, ry, col);
    }
}

// Draw a simple cloud icon
static void draw_cloud(int cx, int cy, unsigned int col) {
    fill_circle(cx - 3, cy, 3, col);
    fill_circle(cx + 2, cy, 3, col);
    fill_circle(cx, cy - 2, 3, col);
    fill_rect(cx - 5, cy, 10, 3, col);
}

// Draw rain drops
static void draw_rain(int cx, int cy, unsigned int col) {
    draw_cloud(cx, cy - 3, rgb(150, 150, 170));
    int off = (int)(millis() / 200) % 3;
    for (int i = 0; i < 3; i++) {
        int dy = ((i * 2 + off) % 5);
        set_pixel(cx - 3 + i * 3, cy + 2 + dy, col);
    }
}

// Draw snowflakes
static void draw_snow(int cx, int cy, unsigned int col) {
    draw_cloud(cx, cy - 3, rgb(150, 150, 170));
    int off = (int)(millis() / 300) % 3;
    for (int i = 0; i < 3; i++) {
        int dy = ((i * 2 + off) % 5);
        set_pixel(cx - 3 + i * 3, cy + 2 + dy, col);
        set_pixel(cx - 2 + i * 3, cy + 3 + dy, col);
    }
}

static void draw_weather_icon(int cx, int cy) {
    unsigned int yellow = rgb(255, 220, 50);
    unsigned int white = rgb(220, 220, 230);
    unsigned int blue = rgb(80, 150, 255);

    if (weather_code <= 1) draw_sun(cx, cy, yellow);
    else if (weather_code <= 3) draw_cloud(cx, cy, white);
    else if (weather_code <= 55) draw_cloud(cx, cy, rgb(120, 120, 140)); // fog/drizzle
    else if (weather_code <= 67) draw_rain(cx, cy, blue);
    else if (weather_code <= 77) draw_snow(cx, cy, white);
    else if (weather_code <= 82) draw_rain(cx, cy, blue);
    else if (weather_code <= 86) draw_snow(cx, cy, white);
    else draw_rain(cx, cy, rgb(255, 255, 100)); // thunderstorm
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    unsigned int bg      = rgb(5, 5, 15);
    unsigned int white   = rgb(255, 255, 255);
    unsigned int cyan    = rgb(0, 200, 255);
    unsigned int dim     = rgb(60, 60, 80);
    unsigned int orange  = rgb(255, 160, 40);

    clear(bg);

    // Title
    draw_str(8, 1, "MONTREAL", cyan);
    draw_line_h(0, 8, 80, dim);

    if (fetch_state == 0 || (fetch_state == 1 && weather_code < 0)) {
        // Loading state with animated dots
        int dots = ((int)(millis() / 500)) % 4;
        char buf[16];
        buf[0] = 'C'; buf[1] = 'o'; buf[2] = 'n';
        buf[3] = 'n'; buf[4] = 'e'; buf[5] = 'c';
        buf[6] = 't'; buf[7] = 'i'; buf[8] = 'n';
        buf[9] = 'g';
        for (int i = 0; i < dots; i++) buf[10 + i] = '.';
        draw_text(8, 20, buf, 10 + dots, dim);
        return;
    }

    if (fetch_state == 3 && weather_code < 0) {
        draw_str(14, 20, "No data", rgb(255, 60, 60));
        return;
    }

    // Weather icon (left side)
    draw_weather_icon(16, 22);

    // Temperature (big, right side)
    char tbuf[10];
    int ti = 0;
    if (temp_negative && (temp_int > 0 || temp_frac > 0)) tbuf[ti++] = '-';
    ti += int_to_buf(temp_int, 0, tbuf + ti);
    tbuf[ti++] = '.';
    tbuf[ti++] = '0' + temp_frac;
    draw_text_big(34, 12, tbuf, ti, white);

    // Degree symbol + C
    char deg[] = "`C";
    draw_text(34 + ti * 12, 12, deg, 2, dim);

    // Condition text
    draw_text(2, 34, condition, condition_len, orange);

    // Bottom stats: humidity + wind
    draw_line_h(0, 41, 80, dim);

    char hbuf[8];
    int hi = 0;
    hbuf[hi++] = 'H'; hbuf[hi++] = ':';
    hi += int_to_buf(humidity, 0, hbuf + hi);
    hbuf[hi++] = '%';
    draw_text(2, 43, hbuf, hi, cyan);

    char wbuf[10];
    int wi = 0;
    wbuf[wi++] = 'W'; wbuf[wi++] = ':';
    wi += int_to_buf(wind_speed, 0, wbuf + wi);
    wbuf[wi++] = 'k'; wbuf[wi++] = 'm';
    draw_text(44, 43, wbuf, wi, cyan);
}
