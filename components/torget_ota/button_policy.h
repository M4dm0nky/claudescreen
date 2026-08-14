#ifndef TORGET_OTA_BUTTON_POLICY_H
#define TORGET_OTA_BUTTON_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * KEY3 bär två avsikter på en enda knapp: kort tryck byter app, tre
 * sekunders håll öppnar OTA-underhållsfönstret. Policyn är ren och
 * pollas med (nere, nu) — ingen GPIO, inga timers — så gränsfallen
 * kan låsas i värdtester innan main.c byter från sin råa flankkoll.
 */

#define TG_MAINTENANCE_HOLD_US 3000000LL

typedef enum {
  TG_BUTTON_NONE,
  TG_BUTTON_NEXT_APP,
  TG_BUTTON_OPEN_MAINTENANCE,
} tg_button_action;

typedef struct {
  /* Väpning: policyn litar inte på pinnen förrän den bevisat sig genom
   * ett släppt (högt) sample. GPIO18 lästes låg vid boot på den fysiska
   * enheten 2026-08-14 och öppnade underhållsfönstret utan att någon
   * rörde knappen — en ovänd policy får aldrig se det hända igen. */
  bool armed;
  bool was_down;
  bool hold_fired;
  int64_t pressed_at_us;
} tg_button_policy;

tg_button_action tg_button_update(tg_button_policy *policy,
                                  bool down, int64_t now_us);

#endif
