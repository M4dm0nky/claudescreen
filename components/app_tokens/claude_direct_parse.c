#include "claude_direct_parse.h"

#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../../third_party/cjson/cJSON.h"

static int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

bool tk_parse_epoch(const char *text, int64_t *out) {
  if (!text || !out || !*text) return false;

  char *end = NULL;
  double n = strtod(text, &end);
  if (end != text && *end == '\0') {
    if (!isfinite(n) || n < 0) return false;
    *out = (int64_t)n;
    return true;
  }

  int y, mo, d, h, mi, s;
  int used = 0;
  if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &s,
             &used) != 6 || used != 19)
    return false;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60)
    return false;

  const char *p = text + used;
  if (*p == '.') {
    p++;
    while (isdigit((unsigned char)*p)) p++;
  }
  int64_t offset = 0;
  if (*p == 'Z' || *p == '\0') {
    /* UTC */
  } else if (*p == '+' || *p == '-') {
    int oh, om;
    if (sscanf(p + 1, "%2d:%2d", &oh, &om) != 2) return false;
    offset = (int64_t)(oh * 3600 + om * 60) * (*p == '+' ? 1 : -1);
  } else {
    return false;
  }
  *out = days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400 + h * 3600 +
         mi * 60 + s - offset;
  return true;
}

static bool ci_equals(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
  return *a == *b;
}

static void set_limit(tk_limit *lim, double pct, int64_t reset_at,
                      int64_t now) {
  memset(lim, 0, sizeof *lim);
  lim->pct = floor(pct * 10.0 + 0.5) / 10.0;
  lim->has_pct = 1;
  int64_t sec = reset_at - now;
  lim->reset_min = (int)((sec + 30) / 60);
  lim->has_reset = 1;
}

bool tk_claude_usage_parse(const char *json, size_t len, int64_t now_epoch,
                           tk_tokens *out) {
  if (!json || !out) return false;
  cJSON *root = cJSON_ParseWithLength(json, len);
  if (!root) return false;
  const cJSON *limits = cJSON_GetObjectItemCaseSensitive(root, "limits");
  if (!cJSON_IsArray(limits)) {
    cJSON_Delete(root);
    return false;
  }

  tk_tokens t;
  memset(&t, 0, sizeof t);
  t.claude_session_state = TK_SESSION_IDLE;

  const cJSON *row;
  cJSON_ArrayForEach(row, limits) {
    if (!cJSON_IsObject(row)) continue;
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(row, "kind");
    const cJSON *pct = cJSON_GetObjectItemCaseSensitive(row, "percent");
    const cJSON *rst = cJSON_GetObjectItemCaseSensitive(row, "resets_at");
    if (!cJSON_IsString(kind) || !cJSON_IsNumber(pct)) continue;
    double p = pct->valuedouble;
    if (!isfinite(p) || p < 0 || p > 100) continue;
    int64_t reset_at;
    if (cJSON_IsString(rst)) {
      if (!tk_parse_epoch(rst->valuestring, &reset_at)) continue;
    } else if (cJSON_IsNumber(rst) && isfinite(rst->valuedouble) &&
               rst->valuedouble >= 0) {
      reset_at = (int64_t)rst->valuedouble;
    } else {
      continue;
    }
    if (reset_at <= now_epoch) continue; /* passed window: not a live value */

    if (strcmp(kind->valuestring, "session") == 0) {
      set_limit(&t.claude_session, p, reset_at, now_epoch);
      t.claude_session_state = TK_SESSION_ACTIVE;
    } else if (strcmp(kind->valuestring, "weekly_all") == 0) {
      set_limit(&t.claude_week, p, reset_at, now_epoch);
    } else if (strcmp(kind->valuestring, "weekly_scoped") == 0) {
      const cJSON *active = cJSON_GetObjectItemCaseSensitive(row, "is_active");
      if (!(cJSON_IsTrue(active) || p > 0)) continue; /* untouched pool */
      const cJSON *scope = cJSON_GetObjectItemCaseSensitive(row, "scope");
      const cJSON *model = cJSON_IsObject(scope)
          ? cJSON_GetObjectItemCaseSensitive(scope, "model") : NULL;
      const cJSON *name = cJSON_IsObject(model)
          ? cJSON_GetObjectItemCaseSensitive(model, "display_name") : NULL;
      if (!cJSON_IsString(name)) continue;
      const char *label = NULL;
      if (ci_equals(name->valuestring, "fable")) label = "FABLE \xC2\xB7 WEEK";
      else if (ci_equals(name->valuestring, "opus")) label = "OPUS \xC2\xB7 WEEK";
      else if (ci_equals(name->valuestring, "sonnet")) label = "SONNET \xC2\xB7 WEEK";
      if (!label) continue;
      set_limit(&t.claude_model_week, p, reset_at, now_epoch);
      strncpy(t.claude_model_week_label, label,
              sizeof t.claude_model_week_label - 1);
      t.has_claude_model_week_label = 1;
    }
  }
  cJSON_Delete(root);
  *out = t;
  return true;
}

static bool copy_string(const cJSON *item, char *dst, size_t cap) {
  if (!cJSON_IsString(item)) return false;
  size_t n = strlen(item->valuestring);
  if (n == 0 || n >= cap) return false;
  memcpy(dst, item->valuestring, n + 1);
  return true;
}

bool tk_oauth_refresh_parse(const char *json, size_t len, int64_t now_epoch,
                            tk_oauth_tokens *out) {
  if (!json || !out) return false;
  cJSON *root = cJSON_ParseWithLength(json, len);
  if (!root) return false;
  tk_oauth_tokens t;
  memset(&t, 0, sizeof t);
  const cJSON *ttl = cJSON_GetObjectItemCaseSensitive(root, "expires_in");
  bool ok = copy_string(cJSON_GetObjectItemCaseSensitive(root, "access_token"),
                        t.access_token, sizeof t.access_token) &&
            copy_string(cJSON_GetObjectItemCaseSensitive(root, "refresh_token"),
                        t.refresh_token, sizeof t.refresh_token) &&
            cJSON_IsNumber(ttl) && isfinite(ttl->valuedouble) &&
            ttl->valuedouble > 0 && ttl->valuedouble < 10 * 365 * 86400.0;
  if (ok) {
    t.expires_at = now_epoch + (int64_t)ttl->valuedouble;
    *out = t;
  }
  cJSON_Delete(root);
  return ok;
}
