#include "pixelitl.h"

static float t;
static int anim_tick;

// Original Nyan Cat sprite (21 wide × 18 tall) - 2 frames
// Palette: 0=clear, 1=outline(dark), 2=gray body, 3=light gray,
//          4=toast border, 5=pink frosting, 6=red sprinkle, 7=green sprinkle,
//          8=blue sprinkle, 9=white(eyes), A=cheek pink, B=mouth open
// Cat faces RIGHT, head on right, tail on left, poptart is the body

// Frame 1: legs down-up-down-up
static const unsigned char FRAME1[18][21] = {
//   tail area     |  poptart body          | head area
//   0  1  2  3  4   5  6  7  8  9 10 11 12  13 14 15 16 17 18 19 20
    {0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0}, // 0  poptart top
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 5, 5, 5, 5, 4, 0, 1, 1, 0, 1, 1, 0}, // 1  frosting + ears
    {0, 0, 0, 0, 0, 4, 5, 6, 5, 5, 5, 7, 5, 4, 1, 2, 2, 1, 2, 2, 1}, // 2  sprinkles + ear fill
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 8, 5, 5, 5, 1, 2, 2, 2, 2, 2, 2, 1}, // 3  + head top
    {0, 0, 0, 0, 0, 4, 5, 5, 6, 5, 5, 5, 5, 1, 2, 9, 2, 2, 9, 2, 1}, // 4  eyes
    {0, 0, 0, 0, 0, 4, 5, 7, 5, 5, 5, 6, 5, 1, 2, 2, 2, 2, 2, 2, 1}, // 5
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 5, 8, 5, 5, 1,10, 2, 2, 2, 2,10, 1}, // 6  cheeks (10=cheek)
    {0, 0, 0, 0, 0, 4, 5, 5, 6, 5, 5, 5, 7, 1, 2, 2,11,11, 2, 2, 1}, // 7  mouth (11=mouth)
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 5, 5, 5, 5, 4, 1, 2, 2, 2, 2, 1, 0}, // 8  jaw
    {0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0, 0, 1, 1, 1, 1, 0, 0}, // 9  poptart bottom
    {0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0}, // 10 body below tart
    {0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0}, // 11
    {0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0}, // 12
    {0, 1, 2, 2, 1, 0, 1, 2, 2, 1, 0, 1, 2, 2, 1, 2, 1, 0, 0, 0, 0}, // 13 legs start
    {1, 2, 2, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0}, // 14 legs (pair 1 down)
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 15 tail
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 16 tail tip
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 17
};

// Frame 2: legs alternate
static const unsigned char FRAME2[18][21] = {
    {0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0}, // 0
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 5, 5, 5, 5, 4, 0, 1, 1, 0, 1, 1, 0}, // 1
    {0, 0, 0, 0, 0, 4, 5, 7, 5, 5, 5, 6, 5, 4, 1, 2, 2, 1, 2, 2, 1}, // 2  sprinkles shift
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 6, 5, 5, 5, 1, 2, 2, 2, 2, 2, 2, 1}, // 3
    {0, 0, 0, 0, 0, 4, 5, 5, 8, 5, 5, 5, 5, 1, 2, 9, 2, 2, 9, 2, 1}, // 4
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 5, 7, 5, 6, 1, 2, 2, 2, 2, 2, 2, 1}, // 5
    {0, 0, 0, 0, 0, 4, 5, 6, 5, 5, 5, 8, 5, 1,10, 2, 2, 2, 2,10, 1}, // 6
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 7, 5, 5, 5, 1, 2, 2,11,11, 2, 2, 1}, // 7
    {0, 0, 0, 0, 0, 4, 5, 5, 5, 5, 5, 5, 5, 4, 1, 2, 2, 2, 2, 1, 0}, // 8
    {0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0, 0, 1, 1, 1, 1, 0, 0}, // 9
    {0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0}, // 10
    {0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0}, // 11
    {0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0}, // 12
    {0, 0, 0, 1, 1, 0, 1, 2, 2, 1, 0, 1, 2, 2, 1, 1, 0, 0, 0, 0, 0}, // 13
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0}, // 14 legs (pair 2 down)
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 15 tail tucked
    {0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 16 tail shifted
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 17 tail down
};

#define SPR_W 21
#define SPR_H 18

// Rainbow colors (6 stripes)
static unsigned int rainbow[6];

// Stars
#define NUM_STARS 16
static float star_x[NUM_STARS];
static int star_y[NUM_STARS];
static float star_speed[NUM_STARS];
static int star_phase[NUM_STARS];

// Palette colors (indices 1-11)
static unsigned int palette[12];

static int screenW, screenH;

__attribute__((export_name("app_init")))
void app_init(void) {
    t = 0.0f;
    anim_tick = 0;
    screenW = width();
    screenH = height();
    srand((int)millis());

    // Palette
    palette[0]  = 0;                      // transparent
    palette[1]  = rgb(100, 100, 100);     // outline / dark
    palette[2]  = rgb(153, 153, 153);     // gray body
    palette[3]  = rgb(190, 190, 190);     // light gray (unused now)
    palette[4]  = rgb(200, 150, 80);      // toast border
    palette[5]  = rgb(255, 170, 200);     // pink frosting
    palette[6]  = rgb(255, 90, 90);       // red sprinkle
    palette[7]  = rgb(90, 220, 90);       // green sprinkle
    palette[8]  = rgb(90, 130, 255);      // blue sprinkle
    palette[9]  = rgb(255, 255, 255);     // white (eyes)
    palette[10] = rgb(255, 130, 150);     // cheek pink
    palette[11] = rgb(80, 50, 50);        // mouth dark

    // Rainbow stripes (classic nyan)
    rainbow[0] = rgb(255, 0, 0);
    rainbow[1] = rgb(255, 153, 0);
    rainbow[2] = rgb(255, 255, 0);
    rainbow[3] = rgb(51, 255, 0);
    rainbow[4] = rgb(0, 153, 255);
    rainbow[5] = rgb(102, 51, 255);

    // Init stars
    for (int i = 0; i < NUM_STARS; i++) {
        star_x[i] = (float)(rand() % screenW);
        star_y[i] = rand() % screenH;
        star_speed[i] = 0.005f + (float)(rand() % 10) * 0.003f;
        star_phase[i] = rand() % 100;
    }
}

__attribute__((export_name("app_update")))
void app_update(int f, int dt) {
    t += (float)dt * 0.001f;
    anim_tick++;

    // Move stars left (slow)
    float dtf = (float)dt;
    for (int i = 0; i < NUM_STARS; i++) {
        star_x[i] -= star_speed[i] * dtf;
        if (star_x[i] < -3.0f) {
            star_x[i] = (float)screenW + (float)(rand() % 10);
            star_y[i] = rand() % screenH;
            star_speed[i] = 0.005f + (float)(rand() % 10) * 0.003f;
        }
    }
}

__attribute__((export_name("app_draw")))
void app_draw(void) {
    // Dark blue space background
    clear(rgb(0, 0, 40));

    // Animation frame: switch every ~8 display frames (slow bob)
    int anim_frame = (anim_tick / 8) % 2;
    int bob = anim_frame; // 0 or 1 pixel bob

    // Cat position - centered, leaving room for rainbow trail on left
    int cat_x = screenW / 2 - SPR_W / 2 + 8;
    int cat_y = (screenH - SPR_H) / 2 + bob;

    // Pick current frame
    const unsigned char (*spr)[21] = anim_frame ? FRAME2 : FRAME1;

    // --- Rainbow trail ---
    // 6 stripes, each 2px tall, trailing from left edge to cat's poptart
    int trail_end = cat_x + 5; // left edge of poptart
    int stripe_h = 2;
    int rainbow_top = cat_y + 1; // align with poptart body

    for (int s = 0; s < 6; s++) {
        int ry = rainbow_top + s * stripe_h;
        // Wavy: each stripe shifts ±1 based on column + time
        for (int x = 0; x < trail_end; x++) {
            int wave = ((x + anim_tick / 6) / 3) % 2;
            int py = ry + wave;
            if (py >= 0 && py < screenH) {
                set_pixel(x, py, rainbow[s]);
                if (py + 1 < screenH) {
                    set_pixel(x, py + 1, rainbow[s]);
                }
            }
        }
    }

    // --- Stars ---
    for (int i = 0; i < NUM_STARS; i++) {
        int sx = (int)star_x[i];
        int sy = star_y[i];
        if (sx < 0 || sx >= screenW || sy < 0 || sy >= screenH) continue;

        // Gentle twinkle
        float tw = sinf(t * 2.5f + (float)star_phase[i]);
        if (tw > -0.2f) {
            float bri = 0.4f + tw * 0.4f;
            unsigned int sc = dim_color(rgb(255, 255, 255), bri);
            set_pixel(sx, sy, sc);
            // Cross shape for bright stars
            if (tw > 0.5f) {
                unsigned int dim_sc = dim_color(sc, 0.4f);
                if (sx > 0) set_pixel(sx - 1, sy, dim_sc);
                if (sx < screenW - 1) set_pixel(sx + 1, sy, dim_sc);
                if (sy > 0) set_pixel(sx, sy - 1, dim_sc);
                if (sy < screenH - 1) set_pixel(sx, sy + 1, dim_sc);
            }
        }
    }

    // --- Draw cat sprite ---
    for (int row = 0; row < SPR_H; row++) {
        for (int col = 0; col < SPR_W; col++) {
            int idx = spr[row][col];
            if (idx == 0) continue;

            unsigned int c = palette[idx];
            int px = cat_x + col;
            int py = cat_y + row;
            if (px >= 0 && px < screenW && py >= 0 && py < screenH) {
                set_pixel(px, py, c);
            }
        }
    }
}
