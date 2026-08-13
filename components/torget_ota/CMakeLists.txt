# Torgets OTA-fundament. De rena policyfilerna (ota_policy, button_policy,
# boot_health_policy) värdtestas i test/run.sh utan ESP-IDF; adaptrarna får
# röra esp_ota_ops och NVS men aldrig bära beslutslogik som inte redan är
# låst i en policy.
idf_component_register(
  SRCS "ota_policy.c" "button_policy.c" "boot_health_policy.c"
       "boot_health.c"
  INCLUDE_DIRS "."
  PRIV_REQUIRES app_update nvs_flash esp_timer)
