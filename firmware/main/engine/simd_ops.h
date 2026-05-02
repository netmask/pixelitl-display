// ESP32-S3 128-bit SIMD operations (PIE instructions)
// All pointers must be 16-byte aligned, sizes must be multiples of 32.
#pragma once

#include <stdint.h>

// Fill aligned buffer with a repeated 16-bit value (e.g., pixel color)
// byte_count must be a multiple of 32.
void simd_fill16(uint16_t *dst, uint16_t value, int byte_count);

// 128-bit pipelined memcpy for aligned buffers.
// byte_count must be a multiple of 32.
void simd_memcpy(void *dst, const void *src, int byte_count);

// Scale VFB row to LCD row (10x pixel duplication).
// Processes 4 pixels per iteration using SIMD.
// pixel_count must be a multiple of 4.
void simd_scale_row(uint16_t *lcd_row, const uint16_t *vfb_row, int pixel_count);
