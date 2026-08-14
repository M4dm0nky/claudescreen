#include <stdio.h>

#include "../components/torget_ota/notice_policy.h"

static int failures;

static void check(const char *what, int condition) {
  if (!condition) {
    printf("FAIL %s\n", what);
    failures++;
  }
}

#define HOUR_US (60LL * 60LL * 1000000LL)

static void test_first_discovery_takes_over_immediately(void) {
  tg_notice_policy policy = {0};
  check("no announcement shows nothing",
        tg_notice_update(&policy, false, false, 1000) == TG_NOTICE_NONE);
  check("first discovery shows at once",
        tg_notice_update(&policy, true, false, 2000) == TG_NOTICE_SHOW);
  check("already showing stays quiet",
        tg_notice_update(&policy, true, false, 3000) == TG_NOTICE_NONE);
}

static void test_dismiss_snoozes_then_nags_again(void) {
  tg_notice_policy policy = {0};
  tg_notice_update(&policy, true, false, 0);
  tg_notice_dismiss(&policy, 1000);
  check("freshly dismissed stays hidden",
        tg_notice_update(&policy, true, false, 2000) == TG_NOTICE_NONE);
  check("still hidden just under the nag interval",
        tg_notice_update(&policy, true, false,
                         1000 + TG_NOTICE_NAG_US - 1) == TG_NOTICE_NONE);
  check("nags again after the interval",
        tg_notice_update(&policy, true, false,
                         1000 + TG_NOTICE_NAG_US) == TG_NOTICE_SHOW);
}

static void test_busy_device_is_never_taken_over(void) {
  tg_notice_policy policy = {0};
  check("busy suppresses even the first discovery",
        tg_notice_update(&policy, true, true, 1000) == TG_NOTICE_NONE);
  tg_notice_update(&policy, true, false, 2000);
  check("turning busy hides a visible takeover",
        tg_notice_update(&policy, true, true, 3000) == TG_NOTICE_HIDE);
  /* Ledig igen med annonsen kvar: första-gången-regeln är förbrukad men
   * dismissed_at är orörd (0), så tjatklockan har redan löpt ut — visa. */
  check("idle again resumes the takeover",
        tg_notice_update(&policy, true, false,
                         TG_NOTICE_NAG_US + 4000) == TG_NOTICE_SHOW);
}

static void test_installed_update_silences_the_notice(void) {
  tg_notice_policy policy = {0};
  tg_notice_update(&policy, true, false, 1000);
  check("announcement gone hides the takeover",
        tg_notice_update(&policy, false, false, 2000) == TG_NOTICE_HIDE);
  check("and stays silent after",
        tg_notice_update(&policy, false, false, 3000) == TG_NOTICE_NONE);
}

static void test_dismiss_without_takeover_is_a_no_op(void) {
  tg_notice_policy policy = {0};
  tg_notice_dismiss(&policy, 1000);
  check("stray dismiss does not start the snooze clock",
        tg_notice_update(&policy, true, false, 2000) == TG_NOTICE_SHOW);
}

int main(void) {
  test_first_discovery_takes_over_immediately();
  test_dismiss_snoozes_then_nags_again();
  test_busy_device_is_never_taken_over();
  test_installed_update_silences_the_notice();
  test_dismiss_without_takeover_is_a_no_op();
  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("OK: UPDATE READY-notisen tjatar lagom och stör aldrig en pågående uppdatering\n");
  return 0;
}
