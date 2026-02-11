// Native C face — bypasses WASM runtime for performance comparison
#pragma once

#include <stdint.h>

void native_face_init(void);
void native_face_update(uint32_t frame, uint32_t dt_ms);
void native_face_draw(uint16_t *fb);

// Stats
int64_t native_face_get_avg_us(void);
uint32_t native_face_get_total_frames(void);
