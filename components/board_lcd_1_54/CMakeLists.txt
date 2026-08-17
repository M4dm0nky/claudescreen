idf_component_register(
    SRCS "board_lcd_1_54.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd esp_lcd_touch esp_lcd_touch_cst816s esp_lvgl_adapter
)
