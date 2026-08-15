/* See needs_you_policy.h. Pure decisions, no LVGL, no clock, no network. */
#include "needs_you_policy.h"

#include <string.h>

void tk_needs_you_reset(tk_needs_you_state *state) {
  if (!state) return;
  memset(state, 0, sizeof *state);
}

static bool same_interaction(const tk_needs_you_state *state,
                             const tk_pending_interaction *pending) {
  return state->has_answered &&
         strncmp(state->answered_id, pending->request_id,
                 TK_PENDING_ID_CAP) == 0;
}

tk_needs_you_view tk_needs_you_view_of(
    const tk_needs_you_state *state,
    const tk_pending_interaction *pending) {
  tk_needs_you_view view = {0};
  if (!state || !pending || !pending->present) return view;

  /* Answered already: the service deletes it on resolve, but the panel polls
   * at 1 Hz, so without this the takeover would sit there for up to a second
   * after the tap and invite a second one. */
  if (same_interaction(state, pending)) return view;

  /* Expired, or so close to it that raising the screen would be theatre.
   * Nothing is lost: an unanswered interaction always falls back to the
   * terminal, which is where it will be waiting. */
  if (pending->expires_in_ms < TK_NEEDS_YOU_MIN_SHOW_MS) return view;

  view.visible = true;
  view.kind = pending->kind;
  view.offer_deny = true;   /* refusing is always safe and reveals nothing */
  view.offer_approve = pending->can_approve;
  /* Round up: showing "0s left" while the thing is still answerable reads as
   * broken, and a second of optimism costs nothing here. */
  view.seconds_left = (pending->expires_in_ms + 999u) / 1000u;
  return view;
}

bool tk_needs_you_allows(const tk_pending_interaction *pending,
                         const tk_needs_you_view *view,
                         tk_needs_you_verdict verdict) {
  if (!pending || !view || !pending->present || !view->visible) return false;
  switch (verdict) {
    case TK_NEEDS_YOU_VERDICT_APPROVE:
      /* Both gates, deliberately: what the service permitted, and what this
       * frame actually offered. */
      return pending->can_approve && view->offer_approve;
    case TK_NEEDS_YOU_VERDICT_DENY:
      return view->offer_deny;
    case TK_NEEDS_YOU_VERDICT_LEAVE_IT:
      return true;
    default:
      return false;
  }
}

void tk_needs_you_mark_answered(tk_needs_you_state *state,
                                const tk_pending_interaction *pending) {
  if (!state || !pending || !pending->present) return;
  memcpy(state->answered_id, pending->request_id, TK_PENDING_ID_CAP);
  state->answered_id[TK_PENDING_ID_CAP - 1] = '\0';
  state->has_answered = true;
}
