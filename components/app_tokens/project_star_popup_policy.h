#ifndef PROJECT_STAR_POPUP_POLICY_H
#define PROJECT_STAR_POPUP_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "github_status.h"

#define TK_PROJECT_STAR_POPUP_HOLD_US (120LL * 1000000LL)

typedef struct {
  char last_event_id[TK_GITHUB_EVENT_ID_CAP];
  int64_t hide_at_us;
  bool visible;
} tk_project_star_popup_policy;

void tk_project_star_popup_policy_init(tk_project_star_popup_policy *policy);
bool tk_project_star_popup_policy_show(tk_project_star_popup_policy *policy,
                                       const char *event_id,
                                       int64_t now_us);
bool tk_project_star_popup_policy_tick(tk_project_star_popup_policy *policy,
                                       int64_t now_us);
bool tk_project_star_popup_policy_dismiss(
    tk_project_star_popup_policy *policy);

#endif
