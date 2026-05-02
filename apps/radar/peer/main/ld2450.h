#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/uart.h"

// One radar target. Up to 3 are reported per frame.
//   x_mm: lateral position. Negative = left, positive = right (relative to radar's facing).
//   y_mm: distance straight ahead.
//   speed_cms: signed velocity. Negative = moving toward the radar.
//   res_mm: per-target distance resolution reported by the radar.
typedef struct {
    int16_t  x_mm;
    int16_t  y_mm;
    int16_t  speed_cms;
    uint16_t res_mm;
    bool     present;
} ld2450_target_t;

typedef struct {
    ld2450_target_t targets[3];
    uint32_t        frame_count;
    uint32_t        bad_frames;   // header/footer mismatches
} ld2450_state_t;

// Bring up the UART, set pin mux, start the parsing task.
// Defaults: 256000 8N1, no flow control. Pass UART_PIN_NO_CHANGE for unused pins.
void ld2450_init(uart_port_t uart_num, int rx_gpio, int tx_gpio);

// Read-only snapshot. Safe to read at any time; the parsing task updates it
// in place per frame.
const ld2450_state_t *ld2450_get_state(void);
