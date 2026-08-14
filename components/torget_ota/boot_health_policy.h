#ifndef TORGET_OTA_BOOT_HEALTH_POLICY_H
#define TORGET_OTA_BOOT_HEALTH_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Ren beslutsmaskin för första boot efter en OTA. Adaptern (boot_health.c,
 * senare uppgift) samlar bevisbitar och pollar hit; själva accept/rollback-
 * beslutet är rent så att varje tidsgräns kan låsas i värdtester.
 *
 * Kravmasken är enbart lokala bevis. Wi-Fi, SNTP, tokenserver och
 * Solelkollen har inga bitar MED FLIT: en frisk avbild måste accepteras
 * i ett hus utan nät och med Macen avstängd — skärmen visar då streck,
 * vilket är korrekt beteende, inte ett skäl att rulla tillbaka.
 */

enum {
  TG_HEALTH_NVS = 1 << 0,
  TG_HEALTH_PSRAM = 1 << 1,
  TG_HEALTH_DISPLAY = 1 << 2,
  TG_HEALTH_UI = 1 << 3,
  TG_HEALTH_SCHEDULER = 1 << 4,
};

#define TG_HEALTH_REQUIRED (TG_HEALTH_NVS | TG_HEALTH_PSRAM | \
                            TG_HEALTH_DISPLAY | TG_HEALTH_UI | \
                            TG_HEALTH_SCHEDULER)

/* Minst åtta sekunder innan besked: schemaläggaren ska hinna bevisa att
 * den tickar under verklig last, inte bara att första callbacken kom.
 * Senast vid femton sekunder faller domen — en avbild som inte samlat
 * alla bevis på den tiden blir aldrig frisk, och en väntande avbild får
 * inte bli sittande i limbo där nästa strömavbrott avgör åt oss. */
#define TG_HEALTH_MIN_UPTIME_US 8000000LL
#define TG_HEALTH_DEADLINE_US 15000000LL

typedef struct {
  uint32_t passed;
  int64_t started_at_us;
  bool pending_verify;
  bool forced_failure;
} tg_boot_health;

typedef enum {
  TG_HEALTH_WAIT,
  TG_HEALTH_ACCEPT,
  TG_HEALTH_ROLLBACK,
} tg_health_decision;

tg_health_decision tg_boot_health_decide(const tg_boot_health *health,
                                         int64_t now_us);

#endif
