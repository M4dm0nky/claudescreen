#include <stdio.h>
#include <string.h>

#include "../components/app_tokens/project_star_popup_policy.h"

static int failures;

static void check(const char *what, int condition) {
  if (!condition) {
    printf("FAIL %s\n", what);
    failures++;
  }
}

int main(void) {
  tk_project_star_popup_policy policy;
  memset(&policy, 0xa5, sizeof policy);
  tk_project_star_popup_policy_init(&policy);
  check("starts hidden", !policy.visible && !policy.last_event_id[0]);

  check("new event shows", tk_project_star_popup_policy_show(
      &policy, "star-1", 1000000));
  check("holds for two minutes",
        policy.visible && policy.hide_at_us == 121000000);
  check("duplicate is ignored", !tk_project_star_popup_policy_show(
      &policy, "star-1", 2000000));
  check("duplicate does not extend hold", policy.hide_at_us == 121000000);
  check("still visible before boundary",
        !tk_project_star_popup_policy_tick(&policy, 120999999) &&
        policy.visible);
  check("hides at boundary",
        tk_project_star_popup_policy_tick(&policy, 121000000) &&
        !policy.visible);
  check("new id can show after hide", tk_project_star_popup_policy_show(
      &policy, "star-2", 122000000));
  check("tap dismisses", tk_project_star_popup_policy_dismiss(&policy) &&
        !policy.visible);
  check("second tap is inert", !tk_project_star_popup_policy_dismiss(&policy));
  check("old id remains deduped only when current",
        tk_project_star_popup_policy_show(&policy, "star-1", 123000000));

  if (failures) return 1;
  printf("GitHub popup policy tests passed\n");
  return 0;
}
