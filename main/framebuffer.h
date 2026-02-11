// Triple-buffered virtual framebuffer with on-the-fly LCD scaling
// Supports standard (80x48, 10x) and HD (160x96, 5x) resolution per face.
#pragma once

#include <stdint.h>
#include <stdbool.h>

void      fb_init(void);
uint16_t *fb_get_back(void);

// WASM task calls this when a frame is complete
void fb_publish(void);

// Called from vsync ISR — swaps ready→front, signals wasm task
bool fb_vsync_swap(void);

// Block until next vsync (call from wasm task to sync frame production)
void fb_wait_vsync(void);

// Called from bounce buffer ISR — fills bounce buffer with scaled VFB data
// pos_px: pixel offset into the 800x480 frame
// len_bytes: bounce buffer size in bytes
void fb_fill_bounce(void *bounce_buf, int pos_px, int len_bytes);

// Dynamic resolution — call before app_init or before each frame
void fb_set_resolution(bool hd);
int  fb_width(void);
int  fb_height(void);
int  fb_scale(void);
