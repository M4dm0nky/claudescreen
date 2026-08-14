#ifndef TOKENS_H
#define TOKENS_H

#include <stdint.h>

#define TK_QUOTA_LABEL_CAP 17

/*
 * Speglar Mac-tjänstens /api/tokens (kontrakt v2) minus transportfälten —
 * VibePulse-datats kontrakt enligt glance-mönstret: platt JSON, tal inte
 * strängar, en takt så appen kan ticka lokalt. Tjänsten (tools/tokenserver/)
 * kombinerar tre källor:
 *
 *  1. Claude Codes sessionsloggar — tokenvolymen (dag/månad/takt/sessioner).
 *  2. Rate-limit-headrarna från ett minimalt Claude-API-anrop (Clawdmeter-
 *     mönstret) — sessionens 5h-fönster + veckofönstret i procent.
 *  3. Codex CLI:s rollout-loggar (passiv läsning) — Codex fönster.
 *
 * Varje limit är (procent använt, minuter till nollning). Fälten kan vara
 * null i payloaden (nyckelring/probe/loggar otillgängliga, eller planen
 * saknar fönstret) — då är has_* 0 och vyn visar streck, aldrig hittade
 * procent. Tokens är alla som passerat modellen: in + ut + cache.
 */

typedef struct {
  double pct;    /* utnyttjande, 0-100 */
  double delta_pct; /* förändring i samma resetcykel */
  int reset_min; /* minuter till fönstret nollas */
  int has_pct, has_reset, has_delta;
  int stale;     /* 1 när värdet är unexpired last-known-good */
} tk_limit;

typedef enum {
  TK_FORECAST_UNAVAILABLE,
  TK_FORECAST_COLLECTING,
  TK_FORECAST_AT_RESET,
  TK_FORECAST_EXHAUSTS,
} tk_forecast_state;

typedef struct {
  tk_forecast_state state;
  int pct_at_reset;
  double pace_factor;
  int64_t at_epoch;
  int offset_min;
  int has_pct_at_reset;
  int has_pace_factor;
  int has_at_epoch;
  int has_offset_min;
} tk_forecast;

/*
 * The value multiple: what this month's usage would cost at list API prices,
 * over what the subscription costs. Served under the additive "value" key.
 *
 * state is what the view branches on, because a wrong figure here is worse
 * than no figure. Anything the service could not stand behind arrives as
 * PARTIAL or NO_PLAN_COST rather than as a number, and an absent or
 * unrecognised block is UNAVAILABLE -- dashes, never an invented multiple.
 */
typedef enum {
  TK_VALUE_UNAVAILABLE,  /* key absent, malformed, or state unrecognised */
  TK_VALUE_PARTIAL,      /* too many unpriced tokens; dash everything */
  TK_VALUE_NO_PLAN_COST, /* dollars are real, denominator unknown */
  TK_VALUE_OK,
} tk_value_state;

typedef struct {
  tk_value_state state;
  double value_usd;  /* list-price value of the month's tokens */
  double plan_usd;   /* what the subscription costs per month */
  double multiple;   /* value_usd / plan_usd */
  int has_value_usd, has_plan_usd, has_multiple;
  /* 0 when the plan cost is this build's unverified default rather than a
   * figure the operator stated, and 0 when any contributing provider's rates
   * are estimates. The view must not present either as a precise figure. */
  int cost_configured;
  int prices_verified;
} tk_value;

typedef struct {
  /* volymen (alltid närvarande) */
  double day_tokens;          /* idag, lokal Mac-tid */
  double day_tokens_per_hour; /* brinntakten över senaste timmen; 0 = paus */
  int day_sessions;           /* sessioner med aktivitet idag */
  double month_tokens;        /* kalendermånaden */

  /* taken (null-bara). claude_model_week är veckofönstret för tyngsta
   * modellen (Fable/Opus) — tredje raden i Claudes egen usage-panel. */
  tk_limit claude_session, claude_week, claude_model_week;
  tk_limit codex_session, codex_week;
  char claude_model_week_label[TK_QUOTA_LABEL_CAP];
  int has_claude_model_week_label;
  tk_forecast claude_forecast, codex_forecast;
  tk_value value;
} tk_tokens;

#endif
