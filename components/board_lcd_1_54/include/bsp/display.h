#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_vendor.h"

/* LCD display color format */
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
/* LCD display color bytes endianess */
#define BSP_LCD_BIGENDIAN           (1)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
/* LCD display color space */
#define BSP_LCD_COLOR_SPACE         (LCD_RGB_ELEMENT_ORDER_RGB)
/* LCD display definition */
#define BSP_LCD_H_RES              (240)
#define BSP_LCD_V_RES              (240)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int max_transfer_sz;    /*!< Maximum transfer size, in bytes. */
} bsp_display_config_t;

typedef enum {
    BSP_DISPLAY_ROTATE_0   = 0,
    BSP_DISPLAY_ROTATE_90  = 1,
    BSP_DISPLAY_ROTATE_180 = 2,
    BSP_DISPLAY_ROTATE_270 = 3,
} bsp_display_rotation_t;

esp_err_t bsp_display_new(const bsp_display_config_t *config,
                          esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io);

esp_err_t bsp_display_brightness_init(void);
esp_err_t bsp_display_brightness_set(int brightness_percent);
int bsp_display_brightness_get(void);

#ifdef __cplusplus
}
#endif
