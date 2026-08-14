#include "button_policy.h"

tg_button_action tg_button_update(tg_button_policy *policy,
                                  bool down, int64_t now_us) {
  if (!policy) return TG_BUTTON_NONE;

  if (!policy->armed) {
    /* Ovänd policy: allt före det första höga samplet ignoreras. En pinne
     * som läser låg från boot (svag pull-up, kapsling, kabelhantering) kan
     * därmed aldrig syntetisera vare sig appbyte eller tresekundershåll —
     * knappen finns för policyn först när den bevisat att den kan släppas. */
    if (!down) policy->armed = true;
    return TG_BUTTON_NONE;
  }

  if (down) {
    if (!policy->was_down) {
      /* Tiden tas ENDAST på uppe-till-nere-flanken. En nollad policy
       * (pressed_at_us = 0) får aldrig jämföras mot klockan, annars
       * skulle första trycket efter boot räknas som ett urgammalt håll
       * och öppna underhållsläget av misstag. */
      policy->was_down = true;
      policy->hold_fired = false;
      policy->pressed_at_us = now_us;
      return TG_BUTTON_NONE;
    }
    if (!policy->hold_fired &&
        now_us - policy->pressed_at_us >= TG_MAINTENANCE_HOLD_US) {
      /* Hållet avfyras exakt en gång vid tröskeln, medan fingret ligger
       * kvar — användaren ska se overlayn öppnas utan att släppa. */
      policy->hold_fired = true;
      return TG_BUTTON_OPEN_MAINTENANCE;
    }
    return TG_BUTTON_NONE;
  }

  if (policy->was_down) {
    /* Släppet efter ett avfyrat håll sväljs: handen bad om underhåll,
     * inte om ett appbyte bakom overlayn. Flaggan nollas här så nästa
     * korta tryck byter app som vanligt. */
    bool fired = policy->hold_fired;
    policy->was_down = false;
    policy->hold_fired = false;
    return fired ? TG_BUTTON_NONE : TG_BUTTON_NEXT_APP;
  }
  return TG_BUTTON_NONE;
}
