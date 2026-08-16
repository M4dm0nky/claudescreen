#ifndef TORGET_OTA_BUTTON_POLICY_H
#define TORGET_OTA_BUTTON_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * KEY3 bär tre avsikter på en enda knapp, åtskilda så ingen kan förväxlas med
 * en annan:
 *   - kort tryck (< 1,5 s), släpp: byter app.
 *   - mellanhåll (1,5-3 s), släpp: Needs You-panik — neka allt parkerat.
 *   - tre sekunders håll: öppnar OTA-underhållsfönstret.
 * Panik avgörs vid SLÄPPET; OTA avfyras MEDAN fingret ligger kvar vid tre
 * sekunder. Ett håll blir därför antingen panik (släppt före 3 s) eller OTA
 * (kvar vid 3 s), aldrig båda. Priset: paniken har ingen ring under hållet —
 * det är en håll-och-släpp-gest, inte en håll-igenom-gest. Policyn är ren och
 * pollas med (nere, nu) så gränsfallen låses i värdtester före main.c:s
 * flankkoll.
 */

#define TG_PANIC_HOLD_US 1500000LL
#define TG_MAINTENANCE_HOLD_US 3000000LL

typedef enum {
  TG_BUTTON_NONE,
  TG_BUTTON_NEXT_APP,
  TG_BUTTON_PANIC,
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
