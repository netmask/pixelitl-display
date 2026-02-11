// Raw RGB LCD panel driver — no-framebuffer mode with bounce buffer callbacks
#pragma once

#include "esp_err.h"
#include <stdint.h>

esp_err_t lcd_init(void);
