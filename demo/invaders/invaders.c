#include "pixelitl.h"

// ===== SPACE INVADERS =====
// Ultra resolution (200x120), touch-controlled

#define SW 200
#define SH 120

// ========== SPRITES (8px wide, bit-pattern bytes, MSB=left) ==========

// Invaders: 8x6px  [type][frame][row]
static const unsigned char inv_sprites[3][2][6] = {
    // Type 0: Squid (top row, 30pts)
    {{ 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x42 },
     { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x81 }},
    // Type 1: Crab (mid rows, 20pts)
    {{ 0x42, 0x24, 0x7E, 0xDB, 0xFF, 0x5A },
     { 0x81, 0x42, 0x7E, 0xDB, 0xFF, 0x24 }},
    // Type 2: Octopus (bottom rows, 10pts)
    {{ 0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x5A },
     { 0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0xA5 }},
};

static const int inv_points[3] = { 30, 20, 10 };

// Player ship: 8x5px
static const unsigned char spr_player[5] = {
    0x18, 0x3C, 0x7E, 0xFF, 0xFF,
};

// ========== INVADER GRID ==========

#define COLS      8
#define ROWS      5
#define INV_W     8
#define INV_H     6
#define SPACE_X   12
#define SPACE_Y   10
#define TOTAL_INV (COLS * ROWS)

static int  inv_alive[ROWS][COLS];
static int  inv_type[ROWS];
static int  inv_x, inv_y;       // grid top-left
static int  inv_dir;            // +1 right, -1 left
static int  inv_anim;           // 0 or 1
static int  inv_count;
static float inv_timer;

// ========== PLAYER ==========

#define PLAYER_W  8
#define PLAYER_H  5
#define PLAYER_Y  (SH - 14)

static float player_x;

// ========== BULLETS ==========

typedef struct { float x, y; int active; } bullet_t;

static bullet_t pbullet;                // single player bullet

#define MAX_EBULLETS 4
static bullet_t ebullets[MAX_EBULLETS]; // enemy bullets

#define PBULLET_SPEED  0.12f            // px/ms upward
#define EBULLET_SPEED  0.04f            // px/ms downward
#define FIRE_INTERVAL  300.0f           // ms between auto-fire shots

static float fire_timer;
static float enemy_fire_timer;

// ========== EXPLOSIONS ==========

#define MAX_EXPL 6
typedef struct { int x, y; float timer; int active; } expl_t;
static expl_t explosions[MAX_EXPL];

// ========== STARS ==========

#define NSTARS 30
static int   star_sx[NSTARS], star_sy[NSTARS];
static float star_bri[NSTARS];

// ========== GAME STATE ==========

static int   score, lives, level;
static int   high_score;
static int   game_over;
static float t;
static float death_pause;   // >0 = brief invulnerability after dying

// ========== COLORS ==========

static unsigned int col_bg;
static unsigned int col_inv[3];
static unsigned int col_player, col_pbullet, col_ebullet;

// ========== HELPERS ==========

static void draw_1bit(int x, int y, const unsigned char *rows, int h,
                      unsigned int color) {
    for (int r = 0; r < h; r++) {
        unsigned char b = rows[r];
        for (int c = 0; c < 8; c++) {
            if (b & (0x80 >> c))
                set_pixel(x + c, y + r, color);
        }
    }
}

static void spawn_expl(int x, int y) {
    for (int i = 0; i < MAX_EXPL; i++) {
        if (!explosions[i].active) {
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].timer = 250.0f;
            explosions[i].active = 1;
            return;
        }
    }
}

static int inv_left(void) {
    for (int c = 0; c < COLS; c++)
        for (int r = 0; r < ROWS; r++)
            if (inv_alive[r][c]) return inv_x + c * SPACE_X;
    return inv_x;
}

static int inv_right(void) {
    for (int c = COLS - 1; c >= 0; c--)
        for (int r = 0; r < ROWS; r++)
            if (inv_alive[r][c]) return inv_x + c * SPACE_X + INV_W;
    return inv_x + INV_W;
}

static int inv_bottom(void) {
    for (int r = ROWS - 1; r >= 0; r--)
        for (int c = 0; c < COLS; c++)
            if (inv_alive[r][c]) return inv_y + r * SPACE_Y + INV_H;
    return inv_y + INV_H;
}

static float move_interval(void) {
    float base = 800.0f - (float)(level - 1) * 80.0f;
    if (base < 200.0f) base = 200.0f;
    float ratio = (float)inv_count / (float)TOTAL_INV;
    float interval = base * ratio;
    if (interval < 50.0f) interval = 50.0f;
    return interval;
}

static void init_stars(void) {
    for (int i = 0; i < NSTARS; i++) {
        star_sx[i] = rand() % SW;
        star_sy[i] = rand() % SH;
        star_bri[i] = 0.15f + (float)(rand() % 50) * 0.01f;
    }
}

static void start_level(void) {
    inv_count = TOTAL_INV;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            inv_alive[r][c] = 1;

    inv_type[0] = 0;   // squid
    inv_type[1] = 1;   // crab
    inv_type[2] = 1;
    inv_type[3] = 2;   // octopus
    inv_type[4] = 2;

    int grid_w = (COLS - 1) * SPACE_X + INV_W;
    inv_x = (SW - grid_w) / 2;
    inv_y = 16;
    inv_dir = 1;
    inv_anim = 0;
    inv_timer = 0.0f;

    pbullet.active = 0;
    for (int i = 0; i < MAX_EBULLETS; i++) ebullets[i].active = 0;
    for (int i = 0; i < MAX_EXPL; i++) explosions[i].active = 0;

    fire_timer = FIRE_INTERVAL;   // fire immediately on first touch
    enemy_fire_timer = 0.0f;
    death_pause = 0.0f;

    player_x = (float)(SW / 2 - PLAYER_W / 2);
}

static void save_high_score(void) {
    if (score > high_score) {
        high_score = score;
        char buf[12];
        pxl_itoa(high_score, buf, sizeof(buf));
        kv_set_str("hiscore", buf);
    }
}

static void new_game(void) {
    score = 0;
    lives = 3;
    level = 1;
    game_over = 0;
    start_level();
}

// ========== EXPORTS ==========

__attribute__((export_name("app_init")))
void app_init(void) {
    set_resolution(2);   // ultra 200x120
    srand((int)millis());
    t = 0.0f;

    col_bg      = rgb(2, 2, 10);
    col_inv[0]  = rgb(50, 255, 50);    // squid: green
    col_inv[1]  = rgb(50, 220, 255);   // crab: cyan
    col_inv[2]  = rgb(255, 255, 255);  // octopus: white
    col_player  = rgb(30, 255, 80);
    col_pbullet = rgb(255, 255, 255);
    col_ebullet = rgb(255, 100, 50);

    // Load high score
    char hs[12];
    high_score = 0;
    if (kv_get_str("hiscore", hs, sizeof(hs)) > 0) {
        for (int i = 0; hs[i] >= '0' && hs[i] <= '9'; i++)
            high_score = high_score * 10 + (hs[i] - '0');
    }

    init_stars();
    new_game();
}

__attribute__((export_name("app_update")))
void app_update(int frame, int dt) {
    float dtf = (float)dt;
    t += dtf * 0.001f;

    // === Game over: tap to restart ===
    if (game_over) {
        if (get_gesture() == GESTURE_TAP) new_game();
        return;
    }

    // === Death pause (invulnerability blink) ===
    if (death_pause > 0.0f) {
        death_pause -= dtf;
        if (death_pause < 0.0f) death_pause = 0.0f;
    }

    // === Player input ===
    if (is_pressed()) {
        int tx = get_touch_x();
        player_x = (float)(tx - PLAYER_W / 2);
        if (player_x < 1.0f) player_x = 1.0f;
        if (player_x > (float)(SW - PLAYER_W - 1))
            player_x = (float)(SW - PLAYER_W - 1);

        fire_timer += dtf;
        if (fire_timer >= FIRE_INTERVAL && !pbullet.active) {
            pbullet.x = player_x + (float)(PLAYER_W / 2);
            pbullet.y = (float)PLAYER_Y - 1.0f;
            pbullet.active = 1;
            fire_timer = 0.0f;
        }
    } else {
        fire_timer = FIRE_INTERVAL;   // instant fire on next touch
    }

    // === Move player bullet ===
    if (pbullet.active) {
        pbullet.y -= PBULLET_SPEED * dtf;
        if (pbullet.y < 0.0f) pbullet.active = 0;
    }

    // === Move enemy bullets ===
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!ebullets[i].active) continue;
        ebullets[i].y += EBULLET_SPEED * dtf;
        if (ebullets[i].y > (float)SH) ebullets[i].active = 0;
    }

    // === Invader movement (stepped) ===
    inv_timer += dtf;
    if (inv_timer >= move_interval()) {
        inv_timer = 0.0f;
        inv_anim ^= 1;

        int le = inv_left();
        int re = inv_right();

        if (inv_dir > 0 && re + 2 > SW - 2) {
            inv_y += 4;
            inv_dir = -1;
        } else if (inv_dir < 0 && le - 2 < 2) {
            inv_y += 4;
            inv_dir = 1;
        } else {
            inv_x += inv_dir * 2;
        }
    }

    // === Invaders reached player level → game over ===
    if (inv_bottom() >= PLAYER_Y) {
        game_over = 1;
        save_high_score();
        return;
    }

    // === Player bullet vs invaders ===
    if (pbullet.active) {
        int bx = (int)pbullet.x;
        int by = (int)pbullet.y;
        for (int r = 0; r < ROWS && pbullet.active; r++) {
            for (int c = 0; c < COLS && pbullet.active; c++) {
                if (!inv_alive[r][c]) continue;
                int ix = inv_x + c * SPACE_X;
                int iy = inv_y + r * SPACE_Y;
                if (bx >= ix && bx < ix + INV_W &&
                    by >= iy && by < iy + INV_H) {
                    inv_alive[r][c] = 0;
                    inv_count--;
                    score += inv_points[inv_type[r]];
                    pbullet.active = 0;
                    spawn_expl(ix + INV_W / 2, iy + INV_H / 2);
                }
            }
        }
    }

    // === Enemy bullets vs player ===
    if (death_pause <= 0.0f) {
        int px = (int)player_x;
        for (int i = 0; i < MAX_EBULLETS; i++) {
            if (!ebullets[i].active) continue;
            int ex = (int)ebullets[i].x;
            int ey = (int)ebullets[i].y;
            if (ex >= px && ex < px + PLAYER_W &&
                ey >= PLAYER_Y && ey < PLAYER_Y + PLAYER_H) {
                ebullets[i].active = 0;
                spawn_expl(px + PLAYER_W / 2, PLAYER_Y + PLAYER_H / 2);
                lives--;
                if (lives <= 0) {
                    game_over = 1;
                    save_high_score();
                } else {
                    death_pause = 1000.0f;
                    player_x = (float)(SW / 2 - PLAYER_W / 2);
                    pbullet.active = 0;
                }
            }
        }
    }

    // === Enemy fire ===
    enemy_fire_timer += dtf;
    float enemy_interval = 1500.0f - (float)(level - 1) * 150.0f;
    if (enemy_interval < 400.0f) enemy_interval = 400.0f;

    if (enemy_fire_timer >= enemy_interval && inv_count > 0) {
        enemy_fire_timer = 0.0f;
        // Pick bottom-most alive invader per column, then choose one randomly
        int cands[COLS];
        int ncand = 0;
        for (int c = 0; c < COLS; c++) {
            for (int r = ROWS - 1; r >= 0; r--) {
                if (inv_alive[r][c]) {
                    cands[ncand++] = r * COLS + c;
                    break;
                }
            }
        }
        if (ncand > 0) {
            int pick = cands[rand() % ncand];
            int pr = pick / COLS, pc = pick % COLS;
            for (int i = 0; i < MAX_EBULLETS; i++) {
                if (!ebullets[i].active) {
                    ebullets[i].x = (float)(inv_x + pc * SPACE_X + INV_W / 2);
                    ebullets[i].y = (float)(inv_y + pr * SPACE_Y + INV_H);
                    ebullets[i].active = 1;
                    break;
                }
            }
        }
    }

    // === Tick explosions ===
    for (int i = 0; i < MAX_EXPL; i++) {
        if (!explosions[i].active) continue;
        explosions[i].timer -= dtf;
        if (explosions[i].timer <= 0.0f) explosions[i].active = 0;
    }

    // === Level complete ===
    if (inv_count <= 0) {
        level++;
        start_level();
    }
}

// ========== DRAWING ==========

static void draw_stars(void) {
    for (int i = 0; i < NSTARS; i++) {
        float tw = sinf(t * 2.5f + (float)i * 1.3f) * 0.3f + 0.7f;
        unsigned int c = dim_color(rgb(80, 100, 160), star_bri[i] * tw);
        set_pixel(star_sx[i], star_sy[i], c);
    }
}

static void draw_invaders(void) {
    for (int r = 0; r < ROWS; r++) {
        int type = inv_type[r];
        unsigned int color = col_inv[type];
        const unsigned char *spr = inv_sprites[type][inv_anim];
        for (int c = 0; c < COLS; c++) {
            if (!inv_alive[r][c]) continue;
            int ix = inv_x + c * SPACE_X;
            int iy = inv_y + r * SPACE_Y;
            draw_1bit(ix, iy, spr, INV_H, color);
        }
    }
}

static void draw_player_ship(void) {
    // Blink during death pause
    if (death_pause > 0.0f && ((int)(death_pause * 0.008f)) & 1) return;
    draw_1bit((int)player_x, PLAYER_Y, spr_player, PLAYER_H, col_player);
}

static void draw_bullets(void) {
    if (pbullet.active)
        fill_rect((int)pbullet.x, (int)pbullet.y, 1, 3, col_pbullet);

    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!ebullets[i].active) continue;
        // Zigzag enemy bullet (alternates column each row)
        int ex = (int)ebullets[i].x;
        int ey = (int)ebullets[i].y;
        set_pixel(ex, ey, col_ebullet);
        set_pixel(ex - 1, ey + 1, col_ebullet);
        set_pixel(ex, ey + 2, col_ebullet);
    }
}

static void draw_explosions(void) {
    for (int i = 0; i < MAX_EXPL; i++) {
        if (!explosions[i].active) continue;
        float progress = 1.0f - explosions[i].timer / 250.0f;
        int r = 1 + (int)(progress * 3.0f);
        float fade = 1.0f - progress;
        unsigned int c1 = dim_color(rgb(255, 220, 50), fade);
        unsigned int c2 = dim_color(rgb(255, 80, 20), fade * 0.6f);
        int ex = explosions[i].x, ey = explosions[i].y;
        // Cross
        for (int d = -r; d <= r; d++) {
            set_pixel(ex + d, ey, c1);
            set_pixel(ex, ey + d, c1);
        }
        // Diagonals
        if (r > 1) {
            set_pixel(ex - r + 1, ey - r + 1, c2);
            set_pixel(ex + r - 1, ey - r + 1, c2);
            set_pixel(ex - r + 1, ey + r - 1, c2);
            set_pixel(ex + r - 1, ey + r - 1, c2);
        }
    }
}

static void draw_hud(void) {
    // Score (top left)
    draw_str(2, 2, "SCORE", rgb(100, 100, 130));
    draw_int(34, 2, score, rgb(50, 255, 50));

    // Level (top center)
    draw_str(SW / 2 - 12, 2, "LV", rgb(100, 100, 130));
    draw_int(SW / 2 + 2, 2, level, rgb(255, 220, 50));

    // Lives (top right, as small ship icons)
    int lx = SW - 4;
    for (int i = 0; i < lives && i < 5; i++) {
        lx -= 10;
        draw_1bit(lx, 2, spr_player, PLAYER_H, dim_color(col_player, 0.5f));
    }

    // Separator line
    draw_line_h(0, 10, SW, rgb(20, 50, 20));

    // Ground line
    draw_line_h(0, SH - 3, SW, rgb(30, 200, 30));
}

static void draw_game_over(void) {
    // Dark overlay
    fill_rect(25, 25, 150, 70, rgb(4, 4, 12));
    draw_rect(25, 25, 150, 70, rgb(200, 40, 40));

    // Title
    draw_str_big_centered(25, 150, 30, "GAME OVER", rgb(255, 50, 50));

    // Final score
    draw_str_centered(25, 150, 50, "SCORE", rgb(140, 140, 160));
    char buf[12];
    pxl_itoa(score, buf, sizeof(buf));
    draw_str_big_centered(25, 150, 58, buf, rgb(50, 255, 50));

    // High score
    if (high_score > 0) {
        draw_str_centered(25, 150, 74, "HI", rgb(100, 100, 130));
        pxl_itoa(high_score, buf, sizeof(buf));
        draw_str(25 + 75 + 10, 74, buf, rgb(255, 220, 50));
    }

    // Blink restart prompt
    if (sinf(t * 4.0f) > 0.0f)
        draw_str_centered(25, 150, 86, "TAP TO PLAY", rgb(200, 200, 200));
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    clear(col_bg);
    draw_stars();
    draw_hud();
    draw_invaders();
    draw_player_ship();
    draw_bullets();
    draw_explosions();

    if (game_over)
        draw_game_over();
}
