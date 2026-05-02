// pixelitl radar — receives ESP-NOW packets from the LD2450 peer and renders
// a top-down phosphor radar sweep with up to 3 tracked targets.

#include "pixelitl.h"

// Must match the peer's packet layout in peer/main/main.c
#define RADAR_MAGIC   0xA4
#define RADAR_VERSION 1

typedef struct __attribute__((packed)) {
    unsigned char  magic;
    unsigned char  version;
    unsigned char  flags;          // bit0..2: target presence
    unsigned char  reserved;
    short          x_mm[3];
    short          y_mm[3];
    short          speed_cms[3];
    unsigned short res_mm[3];
    unsigned int   frame_count;
} radar_pkt_t;

// Display geometry — Ultra resolution.
// LD2450 FOV is ~±60° horizontal, so we draw a 120° wedge with the apex at the
// bottom and the cone opening upward (forward direction).
#define SW            200
#define SH            120
#define CX            100        // wedge apex X
#define CY            108        // wedge apex Y (near bottom)
#define RAD            72        // wedge radius in pixels
#define MAX_RANGE_MM 6000        // map mm → radius

#define HALF_PI       1.5707963f
#define FOV_HALF_RAD  1.0471976f  // 60° each side of forward
// FOV bounds in our screen-angle convention (0=right, π/2=up, sweeping CCW)
#define FOV_LEFT_RAD  (HALF_PI + FOV_HALF_RAD)   // ~2.618 (up-left)
#define FOV_RIGHT_RAD (HALF_PI - FOV_HALF_RAD)   // ~0.524 (up-right)

// State
static radar_pkt_t pkt;
static int         pkt_valid;
static long long   last_rx_ms;
static unsigned int last_frame;

// Per-face diagnostic counters
static unsigned int dbg_has_new = 0;   // # of espnow_has_new() == 1 returns
static unsigned int dbg_parsed  = 0;   // # of successful parses
static int          dbg_last_n  = 0;
static unsigned int dbg_last_magic = 0;
static unsigned int dbg_last_version = 0;

// File-scope + initialized — initialized array lands in .data (lower offset),
// avoiding what looks like a wasm3 memory-access bug at offsets > 64KB where
// the same WASM offset reads bytes shifted by ~6 from where the host wrote.
static unsigned char rx_buf[64] = { 1 };

static float t;
static float sweep;        // sweep angle in radians
static int   missed_frames;

// Per-target screen coords + age (for fade-out)
typedef struct {
    float x, y;       // screen pixel coords
    int   age_ms;     // ms since last seen present
    int   ever_seen;
} tgt_t;

static tgt_t tgts[3];

// Tiny hex helper — writes 2 ASCII hex chars to dst, returns 2.
static int hex2(unsigned int b, char *dst) {
    static const char H[] = "0123456789abcdef";
    dst[0] = H[(b >> 4) & 0xF];
    dst[1] = H[b & 0xF];
    return 2;
}

// Read a 16-bit little-endian signed value from packet at byte offset.
static int rd_s16(int off) {
    int lo = espnow_pkt_byte(off);
    int hi = espnow_pkt_byte(off + 1);
    if (lo < 0 || hi < 0) return 0;
    int v = (lo & 0xFF) | ((hi & 0xFF) << 8);
    if (v & 0x8000) v |= ~0xFFFF;   // sign-extend
    return v;
}

static unsigned int rd_u16(int off) {
    int lo = espnow_pkt_byte(off);
    int hi = espnow_pkt_byte(off + 1);
    if (lo < 0 || hi < 0) return 0;
    return (unsigned int)((lo & 0xFF) | ((hi & 0xFF) << 8));
}

static unsigned int rd_u32(int off) {
    int b0 = espnow_pkt_byte(off);
    int b1 = espnow_pkt_byte(off + 1);
    int b2 = espnow_pkt_byte(off + 2);
    int b3 = espnow_pkt_byte(off + 3);
    if (b0 < 0 || b1 < 0 || b2 < 0 || b3 < 0) return 0;
    return ((unsigned)b0)        | ((unsigned)b1 << 8) |
           ((unsigned)b2 << 16)  | ((unsigned)b3 << 24);
}

static void poll_packet(int dt_ms) {
    if (espnow_has_new()) {
        dbg_has_new++;
        int n = espnow_pkt_len();
        dbg_last_n = n;

        if (n >= (int)sizeof(radar_pkt_t)) {
            int magic   = espnow_pkt_byte(0);
            int version = espnow_pkt_byte(1);
            int flags   = espnow_pkt_byte(2);
            dbg_last_magic   = (unsigned)magic;
            dbg_last_version = (unsigned)version;

            if (magic == RADAR_MAGIC && version == RADAR_VERSION) {
                pkt.magic    = (unsigned char)magic;
                pkt.version  = (unsigned char)version;
                pkt.flags    = (unsigned char)flags;
                pkt.reserved = 0;
                for (int i = 0; i < 3; i++) {
                    pkt.x_mm[i]      = (short)rd_s16(4 + i * 2);
                    pkt.y_mm[i]      = (short)rd_s16(10 + i * 2);
                    pkt.speed_cms[i] = (short)rd_s16(16 + i * 2);
                    pkt.res_mm[i]    = (unsigned short)rd_u16(22 + i * 2);
                }
                pkt.frame_count = rd_u32(28);
                pkt_valid = 1;
                last_frame = pkt.frame_count;
                last_rx_ms = (long long)millis();
                dbg_parsed++;
            }
        }
        espnow_consume();
    }
    // Bump ages even when no packet arrives
    for (int i = 0; i < 3; i++) tgts[i].age_ms += dt_ms;
}

static void update_targets(void) {
    if (!pkt_valid) return;
    for (int i = 0; i < 3; i++) {
        int present = (pkt.flags >> i) & 1;
        if (!present) continue;

        float xn = (float)pkt.x_mm[i] / (float)MAX_RANGE_MM;
        float yn = (float)pkt.y_mm[i] / (float)MAX_RANGE_MM;
        // Clamp to circle
        if (xn < -1.0f) xn = -1.0f; if (xn > 1.0f) xn = 1.0f;
        if (yn < -1.0f) yn = -1.0f; if (yn > 1.0f) yn = 1.0f;
        tgts[i].x = (float)CX + xn * (float)RAD;
        // Forward (positive y) maps upward on screen, so subtract
        tgts[i].y = (float)CY - yn * (float)RAD;
        tgts[i].age_ms = 0;
        tgts[i].ever_seen = 1;
    }
}

// ─── exports ────────────────────────────────────────────────────────────

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(2);  // Ultra 200×120
    t = 0.0f;
    sweep = 0.0f;
    pkt_valid = 0;
    last_rx_ms = 0;
    last_frame = 0xFFFFFFFFu;
    missed_frames = 0;
    for (int i = 0; i < 3; i++) {
        tgts[i].age_ms = 99999;
        tgts[i].ever_seen = 0;
    }
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    t += (float)dt * 0.001f;
    // Ping-pong sweep within the FOV — sin oscillator gives a smooth ease at
    // each edge, like an Alien motion tracker. Period ~2.5 s for a full bounce.
    sweep = HALF_PI + sinf(t * 2.5f) * FOV_HALF_RAD;

    poll_packet(dt);
    update_targets();
}

// Helper: draw an arc segment at radius r from a0 to a1 (both in screen-angle
// convention: 0=right, π/2=up, CCW). Steps based on arc length so we don't
// over- or under-sample.
static void draw_arc(int cx, int cy, int r, float a0, float a1, unsigned int c) {
    if (a1 < a0) { float t = a0; a0 = a1; a1 = t; }
    float arc_len = (a1 - a0) * (float)r;
    int steps = (int)arc_len + 4;
    for (int i = 0; i <= steps; i++) {
        float a = a0 + (a1 - a0) * (float)i / (float)steps;
        int x = cx + (int)(cosf(a) * (float)r);
        int y = cy - (int)(sinf(a) * (float)r);
        set_pixel(x, y, c);
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    // CRT-phosphor backdrop
    clear(rgb(0, 6, 3));

    unsigned int grid_dim    = rgb(0, 22, 11);
    unsigned int grid        = rgb(0, 50, 25);
    unsigned int phos_dim    = rgb(0, 80, 28);
    unsigned int phos_mid    = rgb(0, 170, 50);
    unsigned int phos_bright = rgb(0, 255, 90);

    // ---- Wedge frame ----
    // FOV edge lines from apex out to perimeter
    {
        int ex_l = CX + (int)(cosf(FOV_LEFT_RAD)  * (float)RAD);
        int ey_l = CY - (int)(sinf(FOV_LEFT_RAD)  * (float)RAD);
        int ex_r = CX + (int)(cosf(FOV_RIGHT_RAD) * (float)RAD);
        int ey_r = CY - (int)(sinf(FOV_RIGHT_RAD) * (float)RAD);
        draw_line(CX, CY, ex_l, ey_l, grid);
        draw_line(CX, CY, ex_r, ey_r, grid);
    }

    // Range arcs at 1m..6m (only within FOV)
    for (int m = 1; m * 1000 <= MAX_RANGE_MM; m++) {
        int r = (m * 1000 * RAD) / MAX_RANGE_MM;
        draw_arc(CX, CY, r, FOV_RIGHT_RAD, FOV_LEFT_RAD,
                 (m == (MAX_RANGE_MM / 1000)) ? grid : grid_dim);
    }

    // Center boresight tick (forward axis as a faint dotted line)
    for (int i = 4; i < RAD; i += 4) {
        set_pixel(CX, CY - i, grid_dim);
    }

    // ---- Sweep arm: ping-pong inside the FOV with a fading trail ----
    for (int i = 12; i >= 0; i--) {
        float a = HALF_PI + sinf((t - (float)i * 0.025f) * 2.5f) * FOV_HALF_RAD;
        // Stop drawing tail samples that fall outside the current FOV bounds
        if (a < FOV_RIGHT_RAD || a > FOV_LEFT_RAD) continue;
        float ca = cosf(a), sa = sinf(a);
        int ex = CX + (int)(ca * (float)RAD);
        int ey = CY - (int)(sa * (float)RAD);
        float k = 1.0f - (float)i / 12.0f;
        unsigned int c = dim_color(phos_mid, k * 0.85f);
        draw_line(CX, CY, ex, ey, c);
    }
    // Bright leading edge
    {
        float ca = cosf(sweep), sa = sinf(sweep);
        int ex = CX + (int)(ca * (float)RAD);
        int ey = CY - (int)(sa * (float)RAD);
        draw_line(CX, CY, ex, ey, phos_bright);
        fill_circle(ex, ey, 1, phos_bright);
    }

    // Apex / radar position itself
    float pulse = sinf(t * 4.0f) * 0.3f + 0.7f;
    fill_circle(CX, CY, 2, dim_color(phos_bright, pulse));
    set_pixel(CX, CY, rgb(255, 255, 255));

    // Targets — fade over 2s after they disappear
    for (int i = 0; i < 3; i++) {
        if (!tgts[i].ever_seen) continue;
        float fade;
        int present = pkt_valid && ((pkt.flags >> i) & 1);
        if (present) {
            fade = 0.7f + sinf(t * 7.0f + (float)i) * 0.3f;
        } else {
            float k = (float)tgts[i].age_ms / 2000.0f;
            if (k >= 1.0f) continue;
            fade = (1.0f - k) * 0.7f;
        }

        int tx = (int)tgts[i].x;
        int ty = (int)tgts[i].y;

        unsigned int c = dim_color(phos_bright, fade);
        // Outer faint ring
        draw_circle(tx, ty, 4, dim_color(c, 0.25f));
        // Mid ring
        draw_circle(tx, ty, 2, dim_color(c, 0.55f));
        // Bright dot
        fill_circle(tx, ty, 1, c);
        set_pixel(tx, ty, rgb(220, 255, 230));

        // Tiny label "T0/T1/T2" off to the corner of the dot
        char lbl[3] = {'T', (char)('0' + i), 0};
        draw_str(tx + 5, ty - 3, lbl, dim_color(c, 0.7f));
    }

    // ─── HUD ───────────────────────────────────────────────────────────
    // Title top-left
    draw_str(2, 2, "RADAR", phos_mid);

    // Link status top-right
    long long now = (long long)millis();
    long long age = now - last_rx_ms;
    const char *lbl;
    unsigned int lbl_c;
    if (!pkt_valid)        { lbl = "NO LINK"; lbl_c = rgb(180, 60, 60); }
    else if (age > 2000)   { lbl = "STALE";   lbl_c = rgb(200, 130, 50); }
    else                   { lbl = "ONLINE";  lbl_c = phos_mid; }
    int lbl_n = 0; for (const char *p = lbl; *p; p++) lbl_n++;
    draw_str(SW - lbl_n * PXL_FONT_W - 2, 2, lbl, lbl_c);

    // WiFi channel under the link badge — peer's ESPNOW_CHANNEL must match this.
    int ch = wifi_channel();
    char chb[8];
    int cn = 0;
    chb[cn++] = 'c'; chb[cn++] = 'h';
    cn += pxl_itoa(ch, chb + cn, 8 - cn);
    chb[cn] = 0;
    draw_str(SW - cn * PXL_FONT_W - 2, 2 + 8, chb, phos_dim);

    // Range markers along the boresight (forward axis)
    {
        int top_y = CY - RAD;
        char m6[3] = "6m";
        draw_str(CX - 6, top_y - 8, m6, phos_dim);
    }

    // Compact target list across the top strip — three small slots
    // (each: "Tn xNNN yNNN" or "Tn ---" if absent). Coords in cm.
    for (int i = 0; i < 3; i++) {
        int present = pkt_valid && ((pkt.flags >> i) & 1);
        int rx = 2 + i * 66;
        int ry = 12;
        char lb[3] = {'T', (char)('0' + i), 0};
        unsigned int lc = present ? phos_bright : phos_dim;
        draw_str(rx, ry, lb, lc);

        if (present) {
            char xb[8], yb[8];
            int xl = pxl_itoa(pkt.x_mm[i] / 10, xb, sizeof(xb));   // cm
            int yl = pxl_itoa(pkt.y_mm[i] / 10, yb, sizeof(yb));   // cm
            xb[xl] = 0; yb[yl] = 0;
            draw_str(rx + 14, ry,     "x", phos_dim);
            draw_text(rx + 20, ry, xb, xl, phos_mid);
            draw_str(rx + 14, ry + 8, "y", phos_dim);
            draw_text(rx + 20, ry + 8, yb, yl, phos_mid);
        } else {
            draw_str(rx + 14, ry + 4, "---", phos_dim);
        }
    }

    // Bottom-right diagnostics:
    //   row 1: peer's reported frame_count (last received packet)
    //   row 2: firmware's total ESP-NOW rx counter (increases as packets arrive,
    //          regardless of whether this face polled them)
    if (pkt_valid) {
        char fb[12];
        int fl = pxl_itoa((int)pkt.frame_count, fb, sizeof(fb));
        fb[fl] = 0;
        draw_text(SW - fl * PXL_FONT_W - 2, SH - 16, fb, fl, phos_dim);
    }
    {
        char rb[16];
        int rl = 0;
        rb[rl++] = 'r'; rb[rl++] = 'x'; rb[rl++] = ':';
        rl += pxl_itoa(espnow_total_rx(), rb + rl, 16 - rl);
        rb[rl] = 0;
        draw_text(SW - rl * PXL_FONT_W - 2, SH - 8, rb, rl, phos_dim);
    }
    // Detailed face-side diag along the LEFT edge:
    //   hn  has_new count       — should keep climbing while peer broadcasts
    //   pf  parsed count        — should equal hn within ±1
    //   n=  last espnow_recv()  — should be 32 (sizeof radar_pkt_t)
    //   m=  last magic byte     — should be 0xA4 (164 decimal)
    //   v=  last version byte   — should be 1
    {
        char b[24]; int bl;
        int dy = 22;
        bl = 0; b[bl++]='h'; b[bl++]='n'; b[bl++]=' '; bl += pxl_itoa((int)dbg_has_new, b+bl, 22-bl); b[bl]=0;
        draw_str(2, dy, b, phos_dim);
        bl = 0; b[bl++]='p'; b[bl++]='f'; b[bl++]=' '; bl += pxl_itoa((int)dbg_parsed,  b+bl, 22-bl); b[bl]=0;
        draw_str(2, dy + 8, b, phos_dim);
        bl = 0; b[bl++]='n'; b[bl++]='='; bl += pxl_itoa(dbg_last_n,  b+bl, 22-bl); b[bl]=0;
        draw_str(2, dy + 16, b, phos_dim);
        bl = 0; b[bl++]='m'; b[bl++]='='; bl += pxl_itoa((int)dbg_last_magic,   b+bl, 22-bl); b[bl]=0;
        draw_str(2, dy + 24, b, phos_dim);
        bl = 0; b[bl++]='v'; b[bl++]='='; bl += pxl_itoa((int)dbg_last_version, b+bl, 22-bl); b[bl]=0;
        draw_str(2, dy + 32, b, phos_dim);
    }
    (void)pulse;
}
