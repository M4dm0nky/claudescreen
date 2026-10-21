#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../components/app_tokens/claude_direct_parse.h"

#define NOW 1786700000LL /* 2026-08-14T08:53:20Z */

static void test_epoch(void) {
  int64_t v;
  assert(tk_parse_epoch("1970-01-01T00:00:00Z", &v) && v == 0);
  assert(tk_parse_epoch("2026-08-14T10:00:00.123456+00:00", &v));
  assert(v == 1786701600LL);
  assert(tk_parse_epoch("2026-08-14T12:00:00+02:00", &v) && v == 1786701600LL);
  assert(tk_parse_epoch("1786701600", &v) && v == 1786701600LL);
  assert(!tk_parse_epoch("garbage", &v));
  assert(!tk_parse_epoch("2026-13-14T10:00:00Z", &v));
  assert(!tk_parse_epoch("", &v));
}

static void test_usage(void) {
  const char *body =
    "{\"limits\":["
    "{\"kind\":\"session\",\"percent\":21.04,\"resets_at\":\"2026-08-14T10:00:00Z\"},"
    "{\"kind\":\"weekly_all\",\"percent\":73,\"resets_at\":1786900000},"
    "{\"kind\":\"weekly_scoped\",\"percent\":11,\"is_active\":false,"
    "\"resets_at\":1786900000,\"scope\":{\"model\":{\"display_name\":\"Fable\"}}},"
    "{\"kind\":\"weekly_scoped\",\"percent\":0,\"is_active\":false,"
    "\"resets_at\":1786900000,\"scope\":{\"model\":{\"display_name\":\"Opus\"}}}]}";
  tk_tokens t;
  assert(tk_claude_usage_parse(body, strlen(body), NOW, &t));
  assert(t.claude_session_state == TK_SESSION_ACTIVE);
  assert(t.claude_session.has_pct && t.claude_session.pct == 21.0);
  assert(t.claude_session.reset_min == 27); /* 1600 s */
  assert(t.claude_week.has_pct && t.claude_week.pct == 73.0);
  assert(t.claude_model_week.has_pct && t.claude_model_week.pct == 11.0);
  assert(t.has_claude_model_week_label);
  assert(strcmp(t.claude_model_week_label, "FABLE \xC2\xB7 WEEK") == 0);
  assert(!t.codex_week.has_pct);
}

/* A real api.anthropic.com/api/oauth/usage response (captured 2026-10-21 from
 * the panel's own login, free-text fields trimmed, numbers as served): the
 * document is far wider than the "limits" array, and only session and
 * weekly_all are present - no model-scoped pool. */
static void test_usage_real_capture(void) {
  const char *body =
    "{\"five_hour\":{\"utilization\":41.0,\"resets_at\":\"2026-10-21T14:29:59.956604+00:00\","
    "\"limit_dollars\":null,\"used_dollars\":null,\"remaining_dollars\":null,\"locked_reason\":null},"
    "\"seven_day\":{\"utilization\":20.0,\"resets_at\":\"2026-10-27T05:59:59.956627+00:00\"},"
    "\"seven_day_opus\":null,\"seven_day_sonnet\":null,"
    "\"iguana_necktie\":{\"utilization\":20.284494,\"resets_at\":\"2026-11-05T07:59:00+00:00\","
    "\"limit_dollars\":100,\"used_dollars\":20.284494,\"remaining_dollars\":79.715506,"
    "\"locked_reason\":null},"
    "\"extra_usage\":{\"is_enabled\":false,\"monthly_limit\":null,\"user_disabled\":true},"
    "\"limits\":["
    "{\"kind\":\"session\",\"group\":\"session\",\"percent\":41,\"severity\":\"normal\","
    "\"resets_at\":\"2026-10-21T14:29:59.956604+00:00\",\"scope\":null,\"is_active\":true},"
    "{\"kind\":\"weekly_all\",\"group\":\"weekly\",\"percent\":20,\"severity\":\"normal\","
    "\"resets_at\":\"2026-10-27T05:59:59.956627+00:00\",\"scope\":null,\"is_active\":false}],"
    "\"spend\":{\"used\":{\"amount_minor\":0,\"currency\":\"USD\",\"exponent\":2},\"limit\":null,"
    "\"percent\":0,\"enabled\":false},\"member_dashboard_available\":false,"
    "\"seven_day_breakdown\":{\"rows\":[{\"key\":\"claude_code\",\"display_name\":\"Claude Code\",\"percent\":71}]},"
    "\"weekly_scoped_shares\":null}";
  tk_tokens t;
  assert(tk_claude_usage_parse(body, strlen(body), 1792592774LL, &t));
  assert(t.claude_session_state == TK_SESSION_ACTIVE);
  assert(t.claude_session.has_pct && t.claude_session.pct == 41.0);
  assert(t.claude_session.reset_min == 4); /* 225 s */
  assert(t.claude_week.has_pct && t.claude_week.pct == 20.0);
  assert(!t.claude_model_week.has_pct && !t.has_claude_model_week_label);
  /* The dollar pool is not under a stable name (the API serves a codename),
   * so it is found by shape: 100 USD limit, 20.284494 spent, 79.715506 left,
   * expiring 2026-11-05T07:59:00Z = 1793865540 -> 1272766 s from now. */
  assert(t.has_claude_credits_usd);
  assert(t.claude_credits_left_usd > 79.71 && t.claude_credits_left_usd < 79.72);
  assert(t.claude_credits_spent_usd > 20.28 && t.claude_credits_spent_usd < 20.29);
  assert(t.claude_credits.has_pct && t.claude_credits.pct == 20.3);
  assert(t.claude_credits.has_reset && t.claude_credits.reset_min == 21213);
}

static void test_credits_edge_cases(void) {
  tk_tokens t;
  /* Two pools of that shape: refuse to guess which one is meant. */
  const char *two =
    "{\"limits\":[],\"a\":{\"limit_dollars\":100,\"used_dollars\":10,"
    "\"remaining_dollars\":90,\"resets_at\":1786900000},"
    "\"b\":{\"limit_dollars\":50,\"used_dollars\":10,"
    "\"remaining_dollars\":40,\"resets_at\":1786900000}}";
  assert(tk_claude_usage_parse(two, strlen(two), NOW, &t));
  assert(!t.has_claude_credits_usd && !t.claude_credits.has_pct);
  /* Expired, inconsistent, null and negative pools never show. */
  const char *bad[] = {
    "{\"limits\":[],\"a\":{\"limit_dollars\":100,\"used_dollars\":10,"
    "\"remaining_dollars\":90,\"resets_at\":1786600000}}",
    "{\"limits\":[],\"a\":{\"limit_dollars\":100,\"used_dollars\":10,"
    "\"remaining_dollars\":50,\"resets_at\":1786900000}}",
    "{\"limits\":[],\"a\":{\"limit_dollars\":null,\"used_dollars\":null,"
    "\"remaining_dollars\":null,\"resets_at\":1786900000}}",
    "{\"limits\":[],\"a\":{\"limit_dollars\":100,\"used_dollars\":-5,"
    "\"remaining_dollars\":105,\"resets_at\":1786900000}}",
    "{\"limits\":[],\"a\":{\"limit_dollars\":100,\"used_dollars\":10,"
    "\"remaining_dollars\":90}}",
  };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
    assert(tk_claude_usage_parse(bad[i], strlen(bad[i]), NOW, &t));
    assert(!t.has_claude_credits_usd && !t.claude_credits.has_pct);
  }
}

static void test_idle_and_hostile(void) {
  tk_tokens t;
  memset(&t, 0x5a, sizeof t);
  /* passed session window -> idle, week still valid */
  const char *idle =
    "{\"limits\":[{\"kind\":\"session\",\"percent\":50,\"resets_at\":1786600000},"
    "{\"kind\":\"weekly_all\",\"percent\":5,\"resets_at\":1786900000}]}";
  assert(tk_claude_usage_parse(idle, strlen(idle), NOW, &t));
  assert(t.claude_session_state == TK_SESSION_IDLE && !t.claude_session.has_pct);
  assert(t.claude_week.has_pct);

  /* hostile / invalid: rejected without touching *out */
  memset(&t, 0x5a, sizeof t);
  tk_tokens snap = t;
  const char *bad[] = {
    "", "{", "[]", "{\"error\":\"x\"}", "{\"limits\":{}}",
  };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
    assert(!tk_claude_usage_parse(bad[i], strlen(bad[i]), NOW, &t));
    assert(memcmp(&t, &snap, sizeof t) == 0);
  }
  const char *oor =
    "{\"limits\":[{\"kind\":\"weekly_all\",\"percent\":140,\"resets_at\":1786900000},"
    "{\"kind\":\"weekly_all\",\"percent\":-1,\"resets_at\":1786900000}]}";
  assert(tk_claude_usage_parse(oor, strlen(oor), NOW, &t));
  assert(!t.claude_week.has_pct);
}

static void test_refresh(void) {
  tk_oauth_tokens o;
  const char *ok =
    "{\"access_token\":\"sk-ant-oat01-a\",\"refresh_token\":\"sk-ant-ort01-b\","
    "\"expires_in\":28800}";
  assert(tk_oauth_refresh_parse(ok, strlen(ok), NOW, &o));
  assert(strcmp(o.access_token, "sk-ant-oat01-a") == 0);
  assert(strcmp(o.refresh_token, "sk-ant-ort01-b") == 0);
  assert(o.expires_at == NOW + 28800);
  const char *no_rt = "{\"access_token\":\"a\",\"expires_in\":10}";
  assert(!tk_oauth_refresh_parse(no_rt, strlen(no_rt), NOW, &o));
  const char *no_ttl = "{\"access_token\":\"a\",\"refresh_token\":\"b\"}";
  assert(!tk_oauth_refresh_parse(no_ttl, strlen(no_ttl), NOW, &o));
}

int main(void) {
  test_epoch();
  test_usage();
  test_usage_real_capture();
  test_credits_edge_cases();
  test_idle_and_hostile();
  test_refresh();
  puts("OK: claude direct parse");
  return 0;
}
