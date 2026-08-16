#include <stdio.h>

#include "../components/torget_ota/button_policy.h"

static int failures;

static void check(const char *what, int condition) {
  if (!condition) {
    printf("FAIL %s\n", what);
    failures++;
  }
}

/* Väpna policyn som verkligheten gör: ett släppt sample. Före det litar
 * policyn inte på pinnen alls — se test_boot_low_pin_never_fires. */
static void arm(tg_button_policy *policy, int64_t now_us) {
  tg_button_update(policy, false, now_us);
}

static void test_boot_low_pin_never_fires(void) {
  /* Den fysiska läxan 2026-08-14: GPIO18 lästes låg från boot och öppnade
   * underhållsfönstret utan att någon rörde knappen. En pinne som aldrig
   * setts släppt får inte producera någonting — hur länge den än är låg. */
  tg_button_policy policy = {0};
  check("boot-low first sample emits nothing",
        tg_button_update(&policy, true, 1000) == TG_BUTTON_NONE);
  check("boot-low held past three seconds emits nothing",
        tg_button_update(&policy, true, 1000 + 4000000LL) == TG_BUTTON_NONE);
  check("boot-low held for a minute emits nothing",
        tg_button_update(&policy, true, 60LL * 1000000LL) == TG_BUTTON_NONE);
  /* Första släppet väpnar utan att avfyra något retroaktivt. */
  check("the arming release itself emits nothing",
        tg_button_update(&policy, false, 61LL * 1000000LL) == TG_BUTTON_NONE);
  /* Därefter fungerar knappen som vanligt, mätt från sin egen flank. */
  tg_button_update(&policy, true, 62LL * 1000000LL);
  check("armed press still reaches maintenance from its own edge",
        tg_button_update(&policy, true, 62LL * 1000000LL + 3000000LL) ==
            TG_BUTTON_OPEN_MAINTENANCE);
}

static void test_press_alone_does_nothing(void) {
  tg_button_policy policy = {0};
  check("idle poll emits nothing",
        tg_button_update(&policy, false, 100) == TG_BUTTON_NONE);
  check("press edge emits nothing",
        tg_button_update(&policy, true, 1000) == TG_BUTTON_NONE);
  check("held press under the threshold emits nothing",
        tg_button_update(&policy, true, 1000 + 2999999LL) == TG_BUTTON_NONE);
}

static void test_release_windows_split_at_the_panic_threshold(void) {
  /* Kort tryck byter app; mellanhåll-och-släpp panikar. Gränsen är 1,5 s. */
  {
    tg_button_policy policy = {0};
    arm(&policy, 500);
    tg_button_update(&policy, true, 1000);
    check("release just under 1.5 s emits next-app",
          tg_button_update(&policy, false, 1000 + TG_PANIC_HOLD_US - 1) ==
              TG_BUTTON_NEXT_APP);
  }
  {
    tg_button_policy policy = {0};
    arm(&policy, 500);
    tg_button_update(&policy, true, 1000);
    check("release at exactly 1.5 s panics",
          tg_button_update(&policy, false, 1000 + TG_PANIC_HOLD_US) ==
              TG_BUTTON_PANIC);
  }
  {
    tg_button_policy policy = {0};
    arm(&policy, 500);
    tg_button_update(&policy, true, 1000);
    check("release just before three seconds panics, not next-app",
          tg_button_update(&policy, false, 1000 + 2999999LL) ==
              TG_BUTTON_PANIC);
    check("repeated released polls emit nothing more",
          tg_button_update(&policy, false, 1000 + 3100000LL) == TG_BUTTON_NONE);
  }
}

static void test_panic_cannot_collide_with_the_ota_hold(void) {
  /* Ett håll som når tre sekunder avfyrar OTA medan fingret ligger kvar; det
   * efterföljande släppet får INTE också panika, trots att hållet vida
   * översteg panikgränsen. Detta är hela poängen med panik-vid-släpp. */
  tg_button_policy policy = {0};
  arm(&policy, 500);
  tg_button_update(&policy, true, 1000);
  check("crossing three seconds opens maintenance",
        tg_button_update(&policy, true, 1000 + TG_MAINTENANCE_HOLD_US) ==
            TG_BUTTON_OPEN_MAINTENANCE);
  check("release long after the OTA hold does not also panic",
        tg_button_update(&policy, false, 1000 + 5000000LL) == TG_BUTTON_NONE);
}

static void test_hold_emits_one_open_maintenance(void) {
  tg_button_policy policy = {0};
  arm(&policy, 500);
  tg_button_update(&policy, true, 1000);
  check("held poll one microsecond early emits nothing",
        tg_button_update(&policy, true, 1000 + 2999999LL) == TG_BUTTON_NONE);
  check("crossing three seconds opens maintenance",
        tg_button_update(&policy, true, 1000 + 3000000LL) ==
            TG_BUTTON_OPEN_MAINTENANCE);
  check("continued hold does not reopen maintenance",
        tg_button_update(&policy, true, 1000 + 5000000LL) == TG_BUTTON_NONE);
}

static void test_release_after_hold_does_not_switch_app(void) {
  tg_button_policy policy = {0};
  arm(&policy, 500);
  tg_button_update(&policy, true, 1000);
  tg_button_update(&policy, true, 1000 + 3000000LL);
  /* Handen bad om underhållsläget; att fingret sedan släpper får inte
   * dessutom byta app bakom overlayn. */
  check("release after a fired hold emits nothing",
        tg_button_update(&policy, false, 1000 + 4000000LL) == TG_BUTTON_NONE);
  check("poll after the swallowed release emits nothing",
        tg_button_update(&policy, false, 1000 + 4100000LL) == TG_BUTTON_NONE);

  tg_button_update(&policy, true, 10000000LL);
  check("next short press switches app again",
        tg_button_update(&policy, false, 10000000LL + 100000LL) ==
            TG_BUTTON_NEXT_APP);
}

static void test_time_before_press_cannot_trigger_hold(void) {
  /* En nollad policy bär pressed_at_us = 0. Första nedtrycket sker långt
   * senare; höll-tiden måste räknas från flanken, inte från epok noll,
   * annars öppnar första korta trycket efter boot underhållsläget. */
  tg_button_policy policy = {0};
  arm(&policy, 1000);
  check("first press long after boot emits nothing",
        tg_button_update(&policy, true, 60LL * 1000000LL) == TG_BUTTON_NONE);
  check("immediate held poll after a late press emits nothing",
        tg_button_update(&policy, true, 60LL * 1000000LL + 1) ==
            TG_BUTTON_NONE);
  check("late press still switches app on quick release",
        tg_button_update(&policy, false, 60LL * 1000000LL + 200000LL) ==
            TG_BUTTON_NEXT_APP);

  tg_button_update(&policy, true, 70LL * 1000000LL);
  tg_button_update(&policy, false, 70LL * 1000000LL + 100000LL);
  tg_button_update(&policy, true, 80LL * 1000000LL);
  check("second press measures from its own edge",
        tg_button_update(&policy, true, 80LL * 1000000LL + 2000000LL) ==
            TG_BUTTON_NONE);
  check("second press can still reach maintenance",
        tg_button_update(&policy, true, 80LL * 1000000LL + 3000000LL) ==
            TG_BUTTON_OPEN_MAINTENANCE);
}

int main(void) {
  test_boot_low_pin_never_fires();
  test_press_alone_does_nothing();
  test_release_windows_split_at_the_panic_threshold();
  test_panic_cannot_collide_with_the_ota_hold();
  test_hold_emits_one_open_maintenance();
  test_release_after_hold_does_not_switch_app();
  test_time_before_press_cannot_trigger_hold();

  if (failures == 0) {
    printf("OK: all OTA button-policy tests pass\n");
    return 0;
  }
  printf("%d tests failed\n", failures);
  return 1;
}
