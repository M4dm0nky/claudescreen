#ifndef TORGET_OTA_BOOT_HEALTH_H
#define TORGET_OTA_BOOT_HEALTH_H

#include <stdint.h>

#include "boot_health_policy.h"

/*
 * Målsidans adapter runt den rena hälsopolicyn: läser rollbacktillståndet ur
 * esp_ota_ops, samlar de lokala bevisen och verkställer domen. Endast den
 * här filen får röra esp_ota_mark_* — policyn förblir värdtestbar utan
 * ESP-IDF.
 *
 * Kontrakt mot main.c: kalla torget_boot_health_start() direkt efter att
 * NVS initierats; markera sedan TG_HEALTH_DISPLAY efter display_start(),
 * TG_HEALTH_UI efter torget_ui_create() och TG_HEALTH_SCHEDULER från första
 * tick_cb. NVS- och PSRAM-bevisen samlar adaptern in själv.
 */

void torget_boot_health_start(void);

/* Trådsäker (atomär OR): får kallas från vilken task som helst, även
 * LVGL-tasken — inga lås tas och inget LVGL-objekt rörs. */
void torget_boot_health_mark(uint32_t bit);

#endif
