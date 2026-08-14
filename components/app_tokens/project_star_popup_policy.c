#include "project_star_popup_policy.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

void tk_project_star_popup_policy_init(tk_project_star_popup_policy *policy) {
  if (policy) memset(policy, 0, sizeof *policy);
}

bool tk_project_star_popup_policy_show(tk_project_star_popup_policy *policy,
                                       const char *event_id,
                                       int64_t now_us) {
  if (!policy || !event_id || !event_id[0] ||
      strcmp(policy->last_event_id, event_id) == 0) return false;
  snprintf(policy->last_event_id, sizeof policy->last_event_id, "%s",
           event_id);
  policy->hide_at_us =
      now_us > INT64_MAX - TK_PROJECT_STAR_POPUP_HOLD_US
          ? INT64_MAX : now_us + TK_PROJECT_STAR_POPUP_HOLD_US;
  policy->visible = true;
  return true;
}

bool tk_project_star_popup_policy_tick(tk_project_star_popup_policy *policy,
                                       int64_t now_us) {
  if (!policy || !policy->visible || now_us < policy->hide_at_us) {
    return false;
  }
  policy->visible = false;
  return true;
}

bool tk_project_star_popup_policy_dismiss(
    tk_project_star_popup_policy *policy) {
  if (!policy || !policy->visible) return false;
  policy->visible = false;
  return true;
}
