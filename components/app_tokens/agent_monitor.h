#ifndef AGENT_MONITOR_H
#define AGENT_MONITOR_H

#include <stdint.h>

#include "lvgl.h"

#include "agent_status.h"
#include "needs_you_policy.h"

void tk_agent_monitor_create(lv_obj_t *app_root);
void tk_agent_monitor_apply(const tk_agent_snapshot *snapshot, int64_t now_us);
void tk_agent_monitor_tick(int64_t now_us);

/* Deterministisk simulatorväg; glastrycket går genom samma köfunktion. */
void tk_agent_monitor_dismiss_current(void);

/* A "Needs You" verdict left the glass: the human tapped APPROVE / DENY /
 * LEAVE IT on the interactive takeover. The app layer wires this to the signed
 * network POST; until it does, a tap only dismisses the screen locally, which
 * is exactly what the simulator wants. request_id names the interaction the
 * verdict answers. Never called for a verdict the policy would not allow. */
typedef void (*tk_agent_monitor_needs_you_cb)(tk_needs_you_verdict verdict,
                                              const char *request_id);
void tk_agent_monitor_set_needs_you_cb(tk_agent_monitor_needs_you_cb cb);

#endif
