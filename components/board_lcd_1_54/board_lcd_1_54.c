#include "bsp/board_lcd_1_54.h"
#include "bsp/touch.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"

static const char *TAG = "bsp_lcd_1_54";

static i2c_master_bus_handle_t s_i2c_bus;
static bool s_brightness_ready;
static int s_brightness_pct;

/* Backlight is a plain GPIO on this board, driven by LEDC so the firmware's
 * brightness ramp keeps working (the AMOLED board used a panel command). */
#define BL_LEDC_TIMER      LEDC_TIMER_1
#define BL_LEDC_CHANNEL    LEDC_CHANNEL_1
#define BL_LEDC_DUTY_RES   LEDC_TIMER_10_BIT
#define BL_LEDC_FREQ_HZ    (5000)

esp_err_t bsp_i2c_init(void) {
  if (s_i2c_bus) return ESP_OK;

  i2c_master_bus_config_t cfg = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = BSP_I2C_NUM,
    .scl_io_num = BSP_I2C_SCL,
    .sda_io_num = BSP_I2C_SDA,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };
  return i2c_new_master_bus(&cfg, &s_i2c_bus);
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void) {
  return s_i2c_bus;
}

esp_err_t bsp_display_brightness_init(void) {
  if (s_brightness_ready) return ESP_OK;

  const ledc_timer_config_t timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = BL_LEDC_DUTY_RES,
    .timer_num = BL_LEDC_TIMER,
    .freq_hz = BL_LEDC_FREQ_HZ,
    .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "ledc timer");

  const ledc_channel_config_t channel = {
    .gpio_num = BSP_LCD_GPIO_BL,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = BL_LEDC_CHANNEL,
    .timer_sel = BL_LEDC_TIMER,
    .duty = 0,
    .hpoint = 0,
  };
  ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "ledc channel");

  s_brightness_ready = true;
  return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent) {
  if (!s_brightness_ready) return ESP_ERR_INVALID_STATE;
  if (brightness_percent < 0) brightness_percent = 0;
  if (brightness_percent > 100) brightness_percent = 100;

  const uint32_t max_duty = (1u << BL_LEDC_DUTY_RES) - 1u;
  uint32_t duty = (max_duty * (uint32_t)brightness_percent) / 100u;
#if BSP_LCD_BL_ON_LEVEL == 0
  duty = max_duty - duty;
#endif

  ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty), TAG, "set duty");
  ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL), TAG, "update duty");

  s_brightness_pct = brightness_percent;
  return ESP_OK;
}

int bsp_display_brightness_get(void) {
  return s_brightness_pct;
}

esp_err_t bsp_display_new(const bsp_display_config_t *config,
                          esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io) {
  ESP_RETURN_ON_FALSE(config && ret_panel && ret_io, ESP_ERR_INVALID_ARG, TAG, "bad args");

  const spi_bus_config_t buscfg = {
    .sclk_io_num = BSP_LCD_GPIO_SCLK,
    .mosi_io_num = BSP_LCD_GPIO_MOSI,
    .miso_io_num = GPIO_NUM_NC,
    .quadwp_io_num = GPIO_NUM_NC,
    .quadhd_io_num = GPIO_NUM_NC,
    .max_transfer_sz = config->max_transfer_sz,
  };
  ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi bus");

  const esp_lcd_panel_io_spi_config_t io_config = {
    .dc_gpio_num = BSP_LCD_GPIO_DC,
    .cs_gpio_num = BSP_LCD_GPIO_CS,
    .pclk_hz = BSP_LCD_PIXEL_CLK_HZ,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .spi_mode = 0,
    .trans_queue_depth = 10,
  };
  ESP_RETURN_ON_ERROR(
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io),
    TAG, "panel io");

  const esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = BSP_LCD_GPIO_RST,
    .rgb_ele_order = BSP_LCD_COLOR_SPACE,
    .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
  };
  ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(*ret_io, &panel_config, ret_panel), TAG, "st7789");

  ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*ret_panel), TAG, "reset");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*ret_panel), TAG, "init");
  /* This panel ships inverted; the vendor example sets the same. */
  ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(*ret_panel, true), TAG, "invert");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*ret_panel, true), TAG, "disp on");

  return ESP_OK;
}

esp_err_t bsp_touch_new(const bsp_display_cfg_t *cfg, esp_lcd_touch_handle_t *ret_touch) {
  ESP_RETURN_ON_FALSE(ret_touch, ESP_ERR_INVALID_ARG, TAG, "bad args");
  ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "i2c");

  esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
  io_config.scl_speed_hz = BSP_I2C_CLK_HZ;

  esp_lcd_panel_io_handle_t touch_io = NULL;
  ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_config, &touch_io), TAG, "touch io");

  esp_lcd_touch_config_t tp_cfg = {
    .x_max = BSP_LCD_H_RES,
    .y_max = BSP_LCD_V_RES,
    .rst_gpio_num = BSP_TOUCH_GPIO_RST,
    .int_gpio_num = BSP_TOUCH_GPIO_INT,
    .levels = {
      .reset = 0,
      .interrupt = 0,
    },
    .flags = {
      .swap_xy = cfg ? cfg->touch_flags.swap_xy : 0,
      .mirror_x = cfg ? cfg->touch_flags.mirror_x : 0,
      .mirror_y = cfg ? cfg->touch_flags.mirror_y : 0,
    },
  };
  return esp_lcd_touch_new_i2c_cst816s(touch_io, &tp_cfg, ret_touch);
}
