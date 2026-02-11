// RGB LCD driver — no-framebuffer mode.
//
// Instead of allocating 768KB PSRAM framebuffers and copying to bounce buffers,
// we use flags.no_fb=1 and fill bounce buffers on-the-fly from the 7.5KB VFB.
// All rendering happens SRAM→SRAM, eliminating PSRAM bandwidth bottleneck.

#include "lcd.h"
#include "board.h"
#include "framebuffer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "lcd";
static esp_lcd_panel_handle_t s_panel = NULL;

// Called in ISR context when a bounce buffer needs pixel data
IRAM_ATTR static bool on_bounce_empty(esp_lcd_panel_handle_t panel,
    void *bounce_buf, int pos_px, int len_bytes, void *user_ctx)
{
    fb_fill_bounce(bounce_buf, pos_px, len_bytes);
    return false;
}

// Called in ISR context at vsync — swap triple buffer
IRAM_ATTR static bool on_vsync(esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    return fb_vsync_swap();
}

esp_err_t lcd_init(void)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz            = LCD_PIXEL_CLOCK_HZ,
            .h_res              = LCD_H_RES,
            .v_res              = LCD_V_RES,
            .hsync_pulse_width  = 4,
            .hsync_back_porch   = 8,
            .hsync_front_porch  = 8,
            .vsync_pulse_width  = 4,
            .vsync_back_porch   = 8,
            .vsync_front_porch  = 8,
            .flags = {
                .pclk_active_neg = 1,
            },
        },
        .data_width             = LCD_RGB_DATA_WIDTH,
        .in_color_format        = LCD_COLOR_FMT_RGB565,
        .out_color_format       = LCD_COLOR_FMT_RGB565,
        .bounce_buffer_size_px  = LCD_H_RES * 10,
        .dma_burst_size         = 64,
        .hsync_gpio_num         = LCD_IO_RGB_HSYNC,
        .vsync_gpio_num         = LCD_IO_RGB_VSYNC,
        .de_gpio_num            = LCD_IO_RGB_DE,
        .pclk_gpio_num          = LCD_IO_RGB_PCLK,
        .disp_gpio_num          = LCD_IO_DISP,
        .data_gpio_nums = {
            LCD_IO_RGB_DATA0,  LCD_IO_RGB_DATA1,  LCD_IO_RGB_DATA2,
            LCD_IO_RGB_DATA3,  LCD_IO_RGB_DATA4,  LCD_IO_RGB_DATA5,
            LCD_IO_RGB_DATA6,  LCD_IO_RGB_DATA7,  LCD_IO_RGB_DATA8,
            LCD_IO_RGB_DATA9,  LCD_IO_RGB_DATA10, LCD_IO_RGB_DATA11,
            LCD_IO_RGB_DATA12, LCD_IO_RGB_DATA13, LCD_IO_RGB_DATA14,
            LCD_IO_RGB_DATA15,
        },
        .flags = {
            .no_fb = 1,
        },
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &s_panel), TAG, "new panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = on_vsync,
        .on_bounce_empty = on_bounce_empty,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_register_event_callbacks(s_panel, &cbs, NULL),
        TAG, "register callbacks");

    ESP_LOGI(TAG, "LCD init OK (no-fb bounce mode, pclk=%d MHz)",
             LCD_PIXEL_CLOCK_HZ / 1000000);
    return ESP_OK;
}
