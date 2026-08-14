#include <stdio.h>
#include <stdint.h>

#include "../components/torget_ota/boot_health_policy.h"

static int failures;

static void check(const char *what, int condition) {
  if (!condition) {
    printf("FAIL %s\n", what);
    failures++;
  }
}

/* Starttiden är avsiktligt inte noll: all tidsmatematik måste vara
 * relativ flanken då mätningen började, aldrig mot epok noll. */
#define STARTED_AT_US 123456789LL

static tg_boot_health health_with(uint32_t passed, bool pending,
                                  bool forced) {
  tg_boot_health health = {
      .passed = passed,
      .started_at_us = STARTED_AT_US,
      .pending_verify = pending,
      .forced_failure = forced,
  };
  return health;
}

static tg_health_decision decide_at(tg_boot_health health,
                                    int64_t elapsed_us) {
  return tg_boot_health_decide(&health, STARTED_AT_US + elapsed_us);
}

static void test_non_pending_images_are_ignored(void) {
  /* En redan accepterad avbild har inget rollbackbeslut kvar att fatta;
   * gaten får aldrig röra en stabil boot, hur sen pollen än är. */
  check("stable boot with every bit stays WAIT",
        decide_at(health_with(TG_HEALTH_REQUIRED, false, false),
                  20000000LL) == TG_HEALTH_WAIT);
  check("stable boot with no bits stays WAIT",
        decide_at(health_with(0, false, false), 20000000LL) ==
            TG_HEALTH_WAIT);
  check("stable boot ignores forced failure",
        decide_at(health_with(TG_HEALTH_REQUIRED, false, true),
                  20000000LL) == TG_HEALTH_WAIT);
  check("null health stays WAIT",
        tg_boot_health_decide(NULL, 20000000LL) == TG_HEALTH_WAIT);
}

static void test_pending_image_waits_before_eight_seconds(void) {
  check("healthy pending image still waits at seven seconds",
        decide_at(health_with(TG_HEALTH_REQUIRED, true, false),
                  7000000LL) == TG_HEALTH_WAIT);
  check("healthy pending image waits one microsecond before eight",
        decide_at(health_with(TG_HEALTH_REQUIRED, true, false),
                  7999999LL) == TG_HEALTH_WAIT);
}

static void test_all_bits_accept_at_eight_seconds(void) {
  check("all bits accept exactly at eight seconds",
        decide_at(health_with(TG_HEALTH_REQUIRED, true, false),
                  8000000LL) == TG_HEALTH_ACCEPT);
  check("late poll with all bits still accepts after the deadline",
        decide_at(health_with(TG_HEALTH_REQUIRED, true, false),
                  16000000LL) == TG_HEALTH_ACCEPT);
}

static void test_any_missing_bit_rolls_back_at_fifteen_seconds(void) {
  const uint32_t bits[] = {TG_HEALTH_NVS, TG_HEALTH_PSRAM,
                           TG_HEALTH_DISPLAY, TG_HEALTH_UI,
                           TG_HEALTH_SCHEDULER};
  for (unsigned i = 0; i < sizeof bits / sizeof bits[0]; i++) {
    tg_boot_health health =
        health_with(TG_HEALTH_REQUIRED & ~bits[i], true, false);
    check("one missing bit waits at eight seconds",
          tg_boot_health_decide(&health, STARTED_AT_US + 8000000LL) ==
              TG_HEALTH_WAIT);
    check("one missing bit waits one microsecond before the deadline",
          tg_boot_health_decide(&health, STARTED_AT_US + 14999999LL) ==
              TG_HEALTH_WAIT);
    check("one missing bit rolls back at fifteen seconds",
          tg_boot_health_decide(&health, STARTED_AT_US + 15000000LL) ==
              TG_HEALTH_ROLLBACK);
  }
  check("no bits at all rolls back at fifteen seconds",
        decide_at(health_with(0, true, false), 15000000LL) ==
            TG_HEALTH_ROLLBACK);
}

static void test_forced_failure_rolls_back_at_eight_seconds(void) {
  check("forced failure still waits before eight seconds",
        decide_at(health_with(TG_HEALTH_REQUIRED, true, true),
                  7999999LL) == TG_HEALTH_WAIT);
  check("forced failure rolls back at eight seconds despite full health",
        decide_at(health_with(TG_HEALTH_REQUIRED, true, true),
                  8000000LL) == TG_HEALTH_ROLLBACK);
}

static void test_external_services_are_not_health_requirements(void) {
  /* Kravmasken är exakt de fem lokala bitarna. Wi-Fi, SNTP, tokenserver
   * och Solelkollen saknar bitar med flit: en frisk avbild måste
   * accepteras i ett hus utan nät och med Macen avstängd, annars gör en
   * släckt router varje uppdatering till en falsk rollback. */
  check("required mask holds exactly the five local bits",
        TG_HEALTH_REQUIRED == ((1U << 5) - 1U));
  check("local-only health accepts with no network evidence at all",
        decide_at(health_with(TG_HEALTH_NVS | TG_HEALTH_PSRAM |
                                  TG_HEALTH_DISPLAY | TG_HEALTH_UI |
                                  TG_HEALTH_SCHEDULER,
                              true, false),
                  8000000LL) == TG_HEALTH_ACCEPT);
}

int main(void) {
  test_non_pending_images_are_ignored();
  test_pending_image_waits_before_eight_seconds();
  test_all_bits_accept_at_eight_seconds();
  test_any_missing_bit_rolls_back_at_fifteen_seconds();
  test_forced_failure_rolls_back_at_eight_seconds();
  test_external_services_are_not_health_requirements();

  if (failures == 0) {
    printf("OK: all boot-health policy tests pass\n");
    return 0;
  }
  printf("%d tests failed\n", failures);
  return 1;
}
