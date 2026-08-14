#include "notice_policy.h"

/* Tidsreglerna bor här och ingen annanstans — se headern för kontraktet. */

tg_notice_action tg_notice_update(tg_notice_policy *policy,
                                  bool available, bool busy,
                                  int64_t now_us) {
  if (!policy) return TG_NOTICE_NONE;

  if (!available || busy) {
    /* Slocknad annons eller upptagen enhet: en synlig takeover göms.
     * Avfärdandeklockan lämnas orörd — blir enheten ledig igen med
     * annonsen kvar gäller samma tjatrytm som innan. */
    if (policy->showing) {
      policy->showing = false;
      return TG_NOTICE_HIDE;
    }
    return TG_NOTICE_NONE;
  }

  if (policy->showing) return TG_NOTICE_NONE;

  /* Första upptäckten tar över direkt; därefter styr tjatklockan. */
  if (!policy->ever_shown ||
      now_us - policy->dismissed_at_us >= TG_NOTICE_NAG_US) {
    policy->showing = true;
    policy->ever_shown = true;
    return TG_NOTICE_SHOW;
  }
  return TG_NOTICE_NONE;
}

void tg_notice_dismiss(tg_notice_policy *policy, int64_t now_us) {
  if (!policy || !policy->showing) return;
  policy->showing = false;
  policy->dismissed_at_us = now_us;
}
