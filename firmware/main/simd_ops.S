// ESP32-S3 SIMD operations using PIE (Processor Instruction Extensions)
// 128-bit vector operations: process 8 RGB565 pixels per instruction.
// All pointers must be 16-byte aligned, sizes must be multiples of 32 bytes.

    .text

// ---------------------------------------------------------------------------
// void simd_fill16(uint16_t *dst, uint16_t value, int byte_count)
//
// Fill aligned buffer with a repeated 16-bit value using 128-bit stores.
// Writes 8 pixels per instruction, 16 pixels per loop iteration.
//
// a2 = dst (16-byte aligned)
// a3 = value (16-bit pixel)
// a4 = byte_count (multiple of 32)
// ---------------------------------------------------------------------------
    .global simd_fill16
    .type   simd_fill16, @function
    .align  4
simd_fill16:
    entry   a1, 16

    // Pack pixel pair: a5 = value | (value << 16)
    slli    a5, a3, 16
    or      a5, a5, a3

    // Broadcast to all 4 words of q0 = 8 copies of the pixel
    ee.movi.32.q q0, a5, 0
    ee.movi.32.q q0, a5, 1
    ee.movi.32.q q0, a5, 2
    ee.movi.32.q q0, a5, 3

    // Loop count = byte_count / 32 (two 128-bit stores per iteration)
    srli    a4, a4, 5

    loopnez a4, .Lfill_end
        ee.vst.128.ip q0, a2, 16
        ee.vst.128.ip q0, a2, 16
    .Lfill_end:

    retw.n


// ---------------------------------------------------------------------------
// void simd_memcpy(void *dst, const void *src, int byte_count)
//
// 128-bit SIMD memcpy for aligned buffers. Pipelined: interleaves loads
// and stores for maximum throughput (~4 cycles per 32 bytes).
//
// a2 = dst (16-byte aligned)
// a3 = src (16-byte aligned)
// a4 = byte_count (multiple of 32)
// ---------------------------------------------------------------------------
    .global simd_memcpy
    .type   simd_memcpy, @function
    .align  4
simd_memcpy:
    entry   a1, 16

    srli    a4, a4, 5           // count = byte_count / 32

    loopnez a4, .Lcpy_end
        ee.vld.128.ip q0, a3, 16
        ee.vld.128.ip q1, a3, 16
        ee.vst.128.ip q0, a2, 16
        ee.vst.128.ip q1, a2, 16
    .Lcpy_end:

    retw.n


// ---------------------------------------------------------------------------
// void simd_scale_row(uint16_t *lcd_row, const uint16_t *vfb_row,
//                     int pixel_count)
//
// Expand VFB pixels to LCD pixels (10x duplication). Processes 4 VFB pixels
// per iteration → 40 LCD pixels = 80 bytes = 5 aligned 128-bit stores.
//
// Pattern for P0,P1,P2,P3 → 5 chunks:
//   [P0 P0 P0 P0 P0 P0 P0 P0]  8x P0
//   [P0 P0 P1 P1 P1 P1 P1 P1]  2x P0 + 6x P1
//   [P1 P1 P1 P1 P2 P2 P2 P2]  4x P1 + 4x P2
//   [P2 P2 P2 P2 P2 P2 P3 P3]  6x P2 + 2x P3
//   [P3 P3 P3 P3 P3 P3 P3 P3]  8x P3
//
// a2 = lcd_row (16-byte aligned)
// a3 = vfb_row (source pixels)
// a4 = pixel_count (must be multiple of 4, e.g. 80)
// ---------------------------------------------------------------------------
    .global simd_scale_row
    .type   simd_scale_row, @function
    .align  4
simd_scale_row:
    entry   a1, 16

    srli    a4, a4, 2           // loop count = pixel_count / 4

    loopnez a4, .Lscale_end

        // Load 4 source pixels
        l16ui   a5, a3, 0          // a5 = P0
        l16ui   a6, a3, 2          // a6 = P1
        l16ui   a7, a3, 4          // a7 = P2
        l16ui   a8, a3, 6          // a8 = P3
        addi    a3, a3, 8

        // Pack pixel pairs: aX = PX | (PX << 16)
        slli    a9,  a5, 16
        or      a9,  a9, a5        // a9  = P0|P0
        slli    a10, a6, 16
        or      a10, a10, a6       // a10 = P1|P1
        slli    a11, a7, 16
        or      a11, a11, a7       // a11 = P2|P2
        slli    a12, a8, 16
        or      a12, a12, a8       // a12 = P3|P3

        // Chunk 0: 8x P0
        ee.movi.32.q q0, a9, 0
        ee.movi.32.q q0, a9, 1
        ee.movi.32.q q0, a9, 2
        ee.movi.32.q q0, a9, 3
        ee.vst.128.ip q0, a2, 16

        // Chunk 1: [P0|P0, P1|P1, P1|P1, P1|P1]
        ee.movi.32.q q0, a10, 1
        ee.movi.32.q q0, a10, 2
        ee.movi.32.q q0, a10, 3
        ee.vst.128.ip q0, a2, 16

        // Chunk 2: [P1|P1, P1|P1, P2|P2, P2|P2]
        ee.movi.32.q q0, a10, 0
        ee.movi.32.q q0, a10, 1
        ee.movi.32.q q0, a11, 2
        ee.movi.32.q q0, a11, 3
        ee.vst.128.ip q0, a2, 16

        // Chunk 3: [P2|P2, P2|P2, P2|P2, P3|P3]
        ee.movi.32.q q0, a11, 0
        ee.movi.32.q q0, a11, 1
        ee.movi.32.q q0, a11, 2
        ee.movi.32.q q0, a12, 3
        ee.vst.128.ip q0, a2, 16

        // Chunk 4: 8x P3
        ee.movi.32.q q0, a12, 0
        ee.movi.32.q q0, a12, 1
        ee.movi.32.q q0, a12, 2
        ee.vst.128.ip q0, a2, 16

    .Lscale_end:

    retw.n
