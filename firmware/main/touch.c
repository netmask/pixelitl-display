#include "touch.h"
#include "framebuffer.h"
#include "board.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>

static const char *TAG = "touch";
static esp_lcd_touch_handle_t s_tp = NULL;
static touch_state_t s_state = {
    .touch_x = -1, .touch_y = -1,
    .virtual_x = -1, .virtual_y = -1,
    .gesture = GESTURE_NONE,
    .gesture_x = -1, .gesture_y = -1,
    .pressed = false,
};

// Gesture detection state
static struct {
    bool    was_pressed;
    int64_t press_start_us;
    int32_t start_x, start_y;
} s_gest = {0};

#define SWIPE_THRESHOLD_PX  30
#define LONG_PRESS_US       500000
#define TAP_MAX_US          300000

esp_err_t touch_init(void) {
    // Create I2C master bus
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TOUCH_I2C_PORT,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus_handle), TAG, "i2c bus");

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_cfg.scl_speed_hz = TOUCH_I2C_FREQ_HZ;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(bus_handle, &tp_io_cfg, &tp_io),
        TAG, "tp io");

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_RST,
        .int_gpio_num = TOUCH_INT,
        .levels = { .reset = 0, .interrupt = 0 },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_tp), TAG, "gt911");

    ESP_LOGI(TAG, "GT911 touch init OK");
    return ESP_OK;
}

void touch_poll(void) {
    esp_lcd_touch_read_data(s_tp);

    uint16_t x, y;
    uint8_t points = 0;
    bool touched = esp_lcd_touch_get_coordinates(s_tp, &x, &y, NULL, &points, 1);
    bool currently_pressed = touched && (points > 0);
    int64_t now = esp_timer_get_time();

    if (currently_pressed) {
        s_state.touch_x   = x;
        s_state.touch_y   = y;
        s_state.virtual_x = x / fb_scale();
        s_state.virtual_y = y / fb_scale();
        s_state.pressed   = true;

        if (!s_gest.was_pressed) {
            s_gest.press_start_us = now;
            s_gest.start_x = x;
            s_gest.start_y = y;
        } else if ((now - s_gest.press_start_us) > LONG_PRESS_US &&
                   s_state.gesture == GESTURE_NONE) {
            s_state.gesture = GESTURE_LONG_PRESS;
        }
    } else {
        if (s_gest.was_pressed) {
            int32_t dx = s_state.touch_x - s_gest.start_x;
            int32_t dy = s_state.touch_y - s_gest.start_y;
            int64_t duration = now - s_gest.press_start_us;

            // Save coordinates at gesture time (before clearing)
            s_state.gesture_x = s_state.virtual_x;
            s_state.gesture_y = s_state.virtual_y;

            if (abs(dx) > SWIPE_THRESHOLD_PX || abs(dy) > SWIPE_THRESHOLD_PX) {
                if (abs(dx) > abs(dy)) {
                    s_state.gesture = dx > 0 ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
                } else {
                    s_state.gesture = dy > 0 ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
                }
            } else if (duration < TAP_MAX_US) {
                s_state.gesture = GESTURE_TAP;
            }
        }
        s_state.touch_x   = -1;
        s_state.touch_y   = -1;
        // Keep virtual coords alive while gesture is pending
        if (s_state.gesture == GESTURE_NONE) {
            s_state.virtual_x = -1;
            s_state.virtual_y = -1;
        }
        s_state.pressed   = false;
    }
    s_gest.was_pressed = currently_pressed;
}

int32_t touch_consume_gesture(void) {
    int32_t g = s_state.gesture;
    s_state.gesture = GESTURE_NONE;
    if (g != GESTURE_NONE && !s_state.pressed) {
        // Gesture consumed and finger is up — clear stale coords
        s_state.virtual_x = -1;
        s_state.virtual_y = -1;
    }
    return g;
}

touch_state_t *touch_get_state(void) { return &s_state; }
