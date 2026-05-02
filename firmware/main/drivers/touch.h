// GT911 touch driver with gesture detection
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define GESTURE_NONE        0
#define GESTURE_TAP         1
#define GESTURE_SWIPE_LEFT  2
#define GESTURE_SWIPE_RIGHT 3
#define GESTURE_SWIPE_UP    4
#define GESTURE_SWIPE_DOWN  5
#define GESTURE_LONG_PRESS  6

typedef struct {
    volatile int32_t  touch_x;      // LCD coords (-1 if not touching)
    volatile int32_t  touch_y;
    volatile int32_t  virtual_x;    // Virtual FB coords (0-79, -1 if none)
    volatile int32_t  virtual_y;    // Virtual FB coords (0-47, -1 if none)
    volatile int32_t  gesture;      // Last gesture (consumed on read)
    volatile int32_t  gesture_x;    // Virtual coords at gesture time
    volatile int32_t  gesture_y;
    volatile bool     pressed;
} touch_state_t;

esp_err_t      touch_init(void);
void           touch_poll(void);
touch_state_t *touch_get_state(void);
int32_t        touch_consume_gesture(void);
