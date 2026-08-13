# Torgets OTA-fundament. De rena policyfilerna (ota_policy, button_policy,
# boot_health_policy) värdtestas i test/run.sh utan ESP-IDF; adaptrarna får
# röra esp_ota_ops och NVS men aldrig bära beslutslogik som inte redan är
# låst i en policy.
idf_component_register(
  SRCS "ota_policy.c" "button_policy.c" "boot_health_policy.c"
       "boot_health.c" "ota_ui.c" "ota_service.c"
  INCLUDE_DIRS "."
  # torget_app: designsystemets fonter (extern lv_font_t) + torget.h-låset.
  REQUIRES torget_app
  # secrets.h (TG_OTA_TOKEN) bor i repytroten, gitignorad — därav "../..".
  PRIV_INCLUDE_DIRS "../.."
  PRIV_REQUIRES app_update nvs_flash esp_timer
                esp_http_server mbedtls esp_app_format)
