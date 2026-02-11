// Waveshare 4.3" ESP32-S3 RGB LCD — pin definitions and constants
#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"

// Physical LCD resolution
#define LCD_H_RES             800
#define LCD_V_RES             480
#define LCD_PIXEL_CLOCK_HZ    (21 * 1000 * 1000)
#define LCD_BIT_PER_PIXEL     16
#define LCD_RGB_DATA_WIDTH    16

// RGB LCD pins
#define LCD_IO_RGB_VSYNC      GPIO_NUM_3
#define LCD_IO_RGB_HSYNC      GPIO_NUM_46
#define LCD_IO_RGB_DE         GPIO_NUM_5
#define LCD_IO_RGB_PCLK       GPIO_NUM_7
#define LCD_IO_RGB_DATA0      GPIO_NUM_14
#define LCD_IO_RGB_DATA1      GPIO_NUM_38
#define LCD_IO_RGB_DATA2      GPIO_NUM_18
#define LCD_IO_RGB_DATA3      GPIO_NUM_17
#define LCD_IO_RGB_DATA4      GPIO_NUM_10
#define LCD_IO_RGB_DATA5      GPIO_NUM_39
#define LCD_IO_RGB_DATA6      GPIO_NUM_0
#define LCD_IO_RGB_DATA7      GPIO_NUM_45
#define LCD_IO_RGB_DATA8      GPIO_NUM_48
#define LCD_IO_RGB_DATA9      GPIO_NUM_47
#define LCD_IO_RGB_DATA10     GPIO_NUM_21
#define LCD_IO_RGB_DATA11     GPIO_NUM_1
#define LCD_IO_RGB_DATA12     GPIO_NUM_2
#define LCD_IO_RGB_DATA13     GPIO_NUM_42
#define LCD_IO_RGB_DATA14     GPIO_NUM_41
#define LCD_IO_RGB_DATA15     GPIO_NUM_40

#define LCD_IO_RST            (-1)
#define LCD_IO_DISP           (-1)
#define LCD_BK_LIGHT_PIN      (-1)

// Touch (GT911) I2C pins
#define TOUCH_I2C_SCL         GPIO_NUM_9
#define TOUCH_I2C_SDA         GPIO_NUM_8
#define TOUCH_I2C_PORT        0
#define TOUCH_I2C_FREQ_HZ     400000
#define TOUCH_RST             (-1)
#define TOUCH_INT             (-1)

// Virtual framebuffer — standard resolution
#define VFB_WIDTH             80
#define VFB_HEIGHT            48
#define VFB_SCALE             10
#define VFB_PIXEL_COUNT       (VFB_WIDTH * VFB_HEIGHT)

// Virtual framebuffer — HD resolution (per-face opt-in)
#define VFB_HD_WIDTH          160
#define VFB_HD_HEIGHT         96
#define VFB_HD_SCALE          5
#define VFB_HD_PIXEL_COUNT    (VFB_HD_WIDTH * VFB_HD_HEIGHT)
