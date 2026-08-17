#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lv_adapter.h"

#include "bsp/display.h"

/* Waveshare ESP32-S3-Touch-LCD-1.54 — pins taken from the vendor's own
 * ESP-IDF 5.5.1 examples (05_lvgl_example, 02_button_example,
 * 03_qmi8658_example) in waveshareteam/ESP32-S3-Touch-LCD-1.54. */

#define BSP_LCD_SPI_NUM            (SPI3_HOST)
#define BSP_LCD_PIXEL_CLK_HZ       (40 * 1000 * 1000)

#define BSP_LCD_GPIO_SCLK          (GPIO_NUM_38)
#define BSP_LCD_GPIO_MOSI          (GPIO_NUM_39)
#define BSP_LCD_GPIO_RST           (GPIO_NUM_40)
#define BSP_LCD_GPIO_DC            (GPIO_NUM_45)
#define BSP_LCD_GPIO_CS            (GPIO_NUM_21)
#define BSP_LCD_GPIO_BL            (GPIO_NUM_46)
#define BSP_LCD_BL_ON_LEVEL        (1)

#define BSP_I2C_NUM                (0)
#define BSP_I2C_CLK_HZ             (400 * 1000)
#define BSP_I2C_SCL                (GPIO_NUM_41)
#define BSP_I2C_SDA                (GPIO_NUM_42)

#define BSP_TOUCH_GPIO_INT         (GPIO_NUM_48)
#define BSP_TOUCH_GPIO_RST         (GPIO_NUM_47)

#define BSP_BUTTON_BOOT            (GPIO_NUM_0)
#define BSP_BUTTON_PWR             (GPIO_NUM_5)
#define BSP_BUTTON_PLUS            (GPIO_NUM_4)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lv_adapter_config_t          lv_adapter_cfg;
    esp_lv_adapter_rotation_t        rotation;
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode;
    struct {
        unsigned int swap_xy;
        unsigned int mirror_x;
        unsigned int mirror_y;
    } touch_flags;
} bsp_display_cfg_t;

esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

#ifdef __cplusplus
}
#endif
