#pragma once

#include "esp_lcd_touch.h"

#include "bsp/board_lcd_1_54.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_touch_new(const bsp_display_cfg_t *cfg, esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
