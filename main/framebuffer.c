// Triple-buffered VFB with on-the-fly LCD bounce buffer scaling.
//
// Architecture:
//   - 3 VFB buffers in SRAM (allocated at HD size = 30KB each, 90KB total)
//   - WASM task writes to s_back, then calls fb_publish()
//   - on_vsync ISR swaps ready→front via fb_vsync_swap()
//   - on_bounce_empty ISR calls fb_fill_bounce() to scale VFB→bounce (SRAM→SRAM)
//   - Zero PSRAM traffic for rendering
//   - Supports standard (80x48, 10x) and HD (160x96, 5x) per face

#include "framebuffer.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"
#include <string.h>

// Allocate at HD size (largest possible) — 16-byte aligned for SIMD
static uint16_t s_vfb[3][VFB_HD_PIXEL_COUNT] __attribute__((aligned(16)));

static volatile int s_front_idx = 0;   // ISR reads this for bounce buffer fill
static volatile int s_ready_idx = -1;  // published by wasm, consumed by vsync ISR
static int s_back_idx = 1;             // owned exclusively by wasm task

static portMUX_TYPE s_fb_lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_vsync_sem = NULL;

// Active resolution (set by fb_set_resolution, read by ISR via fb_fill_bounce)
static volatile int s_active_width  = VFB_WIDTH;
static volatile int s_active_height = VFB_HEIGHT;
static volatile int s_active_scale  = VFB_SCALE;

void fb_init(void) {
    memset(s_vfb, 0, sizeof(s_vfb));
    s_vsync_sem = xSemaphoreCreateBinary();
}

uint16_t *fb_get_back(void) {
    return s_vfb[s_back_idx];
}

void fb_publish(void) {
    portENTER_CRITICAL(&s_fb_lock);
    int old_ready = s_ready_idx;
    s_ready_idx = s_back_idx;
    if (old_ready >= 0) {
        s_back_idx = old_ready;
    } else {
        s_back_idx = 3 - s_front_idx - s_ready_idx;
    }
    portEXIT_CRITICAL(&s_fb_lock);
}

bool IRAM_ATTR fb_vsync_swap(void) {
    portENTER_CRITICAL_ISR(&s_fb_lock);
    if (s_ready_idx >= 0) {
        s_front_idx = s_ready_idx;
        s_ready_idx = -1;
    }
    portEXIT_CRITICAL_ISR(&s_fb_lock);

    BaseType_t yield = pdFALSE;
    xSemaphoreGiveFromISR(s_vsync_sem, &yield);
    return (yield == pdTRUE);
}

void fb_wait_vsync(void) {
    xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(100));
}

void fb_set_resolution(bool hd) {
    if (hd) {
        s_active_width  = VFB_HD_WIDTH;
        s_active_height = VFB_HD_HEIGHT;
        s_active_scale  = VFB_HD_SCALE;
    } else {
        s_active_width  = VFB_WIDTH;
        s_active_height = VFB_HEIGHT;
        s_active_scale  = VFB_SCALE;
    }
}

int fb_width(void)  { return s_active_width; }
int fb_height(void) { return s_active_height; }
int fb_scale(void)  { return s_active_scale; }

// Dim a single RGB565 pixel (factor 0-255, 255=full brightness)
static inline uint16_t IRAM_ATTR dim_pixel(uint16_t c, uint8_t factor) {
    uint16_t r = (uint16_t)((((c >> 11) & 0x1F) * factor) >> 8);
    uint16_t g = (uint16_t)((((c >> 5)  & 0x3F) * factor) >> 8);
    uint16_t b = (uint16_t)(((c         & 0x1F) * factor) >> 8);
    return (r << 11) | (g << 5) | b;
}

#define GLOW_DIM_FACTOR 100  // Border dimming (0=black, 255=none)

// Standard mode (10x): 10 LCD rows per VFB row
// Each pixel block: 9 bright + 1 dimmed right border
// Bottom border row (sub_row==9): all dimmed
static void IRAM_ATTR fill_bounce_10x(uint16_t *dst, const uint16_t *vfb,
                                       int lcd_row_start, int num_lcd_rows) {
    for (int lcd_row = lcd_row_start; lcd_row < lcd_row_start + num_lcd_rows; lcd_row++) {
        int vy = lcd_row / VFB_SCALE;
        int sub_row = lcd_row % VFB_SCALE;

        if (vy >= VFB_HEIGHT) {
            memset(dst, 0, LCD_H_RES * sizeof(uint16_t));
            dst += LCD_H_RES;
            continue;
        }

        const uint16_t *vfb_row = &vfb[vy * VFB_WIDTH];

        if (sub_row == VFB_SCALE - 1) {
            // Bottom border row — all dimmed
            for (int vx = 0; vx < VFB_WIDTH; vx++) {
                uint16_t dimmed = dim_pixel(vfb_row[vx], GLOW_DIM_FACTOR);
                uint32_t dim2 = ((uint32_t)dimmed << 16) | dimmed;
                uint32_t *d32 = (uint32_t *)&dst[vx * VFB_SCALE];
                d32[0] = dim2; d32[1] = dim2; d32[2] = dim2; d32[3] = dim2; d32[4] = dim2;
            }
        } else {
            // Interior row: 9 bright pixels + 1 dimmed right border
            for (int vx = 0; vx < VFB_WIDTH; vx++) {
                uint16_t px = vfb_row[vx];
                uint32_t px2 = ((uint32_t)px << 16) | px;
                uint32_t *d32 = (uint32_t *)&dst[vx * VFB_SCALE];
                d32[0] = px2; d32[1] = px2; d32[2] = px2; d32[3] = px2;
                dst[vx * VFB_SCALE + 8] = px;
                dst[vx * VFB_SCALE + 9] = dim_pixel(px, GLOW_DIM_FACTOR);
            }
        }
        dst += LCD_H_RES;
    }
}

// HD mode (5x): 5 LCD rows per VFB row
// Each pixel block: 4 bright + 1 dimmed right border
// Bottom border row (sub_row==4): all dimmed
static void IRAM_ATTR fill_bounce_5x(uint16_t *dst, const uint16_t *vfb,
                                      int lcd_row_start, int num_lcd_rows) {
    for (int lcd_row = lcd_row_start; lcd_row < lcd_row_start + num_lcd_rows; lcd_row++) {
        int vy = lcd_row / VFB_HD_SCALE;
        int sub_row = lcd_row % VFB_HD_SCALE;

        if (vy >= VFB_HD_HEIGHT) {
            memset(dst, 0, LCD_H_RES * sizeof(uint16_t));
            dst += LCD_H_RES;
            continue;
        }

        const uint16_t *vfb_row = &vfb[vy * VFB_HD_WIDTH];

        if (sub_row == VFB_HD_SCALE - 1) {
            // Bottom border row — all dimmed
            for (int vx = 0; vx < VFB_HD_WIDTH; vx++) {
                uint16_t dimmed = dim_pixel(vfb_row[vx], GLOW_DIM_FACTOR);
                uint32_t dim2 = ((uint32_t)dimmed << 16) | dimmed;
                uint32_t *d32 = (uint32_t *)&dst[vx * VFB_HD_SCALE];
                d32[0] = dim2;
                dst[vx * VFB_HD_SCALE + 2] = dimmed;
                dst[vx * VFB_HD_SCALE + 3] = dimmed;
                dst[vx * VFB_HD_SCALE + 4] = dimmed;
            }
        } else {
            // Interior row: 4 bright + 1 dimmed right border
            for (int vx = 0; vx < VFB_HD_WIDTH; vx++) {
                uint16_t px = vfb_row[vx];
                uint32_t px2 = ((uint32_t)px << 16) | px;
                uint32_t *d32 = (uint32_t *)&dst[vx * VFB_HD_SCALE];
                d32[0] = px2;
                dst[vx * VFB_HD_SCALE + 2] = px;
                dst[vx * VFB_HD_SCALE + 3] = px;
                dst[vx * VFB_HD_SCALE + 4] = dim_pixel(px, GLOW_DIM_FACTOR);
            }
        }
        dst += LCD_H_RES;
    }
}

void IRAM_ATTR fb_fill_bounce(void *bounce_buf, int pos_px, int len_bytes) {
    uint16_t *dst = (uint16_t *)bounce_buf;
    const uint16_t *vfb = s_vfb[s_front_idx];

    int lcd_row_start = pos_px / LCD_H_RES;
    int num_lcd_rows = len_bytes / (LCD_H_RES * sizeof(uint16_t));

    if (s_active_scale == VFB_HD_SCALE) {
        fill_bounce_5x(dst, vfb, lcd_row_start, num_lcd_rows);
    } else {
        fill_bounce_10x(dst, vfb, lcd_row_start, num_lcd_rows);
    }
}
