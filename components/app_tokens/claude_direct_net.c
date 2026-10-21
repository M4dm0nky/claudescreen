/*
 * Direct mode: the panel fetches Claude quota from api.anthropic.com itself,
 * so it works with the Mac off. Compiled in only when secrets.h defines
 * TK_CLAUDE_REFRESH_TOKEN; it then replaces the Mac /api/tokens poll.
 *
 * Token lifecycle. Anthropic rotates the refresh token on every renewal, so
 * the pair lives in NVS (namespace "claude") and the secrets.h value is only
 * the seed for the very first boot. The NEW refresh token is written to NVS
 * before the access token is used, because the old one is already dead the
 * moment the server answered. Renewal happens ahead of expiry, and once more
 * on a 401, never in a loop.
 *
 * Hard rule: this must be the panel's OWN login. If it shares a grant with
 * Claude Code on a Mac, whoever refreshes first invalidates the other's
 * refresh token (docs/direct-mode.md).
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs.h"

#include "app_tokens.h"
#ifdef ESP_PLATFORM
#include "secrets.h"
#endif
#include "claude_direct_parse.h"
#include "tokens_parse.h"
#include "torget.h"

#ifdef TK_CLAUDE_REFRESH_TOKEN

static const char *TAG = "claude-direct";

#define USAGE_URL "https://api.anthropic.com/api/oauth/usage"
#define REFRESH_URL "https://platform.claude.com/v1/oauth/token"
#define OAUTH_CLIENT_ID "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
#define FETCH_EVERY_MS 120000   /* matches the Mac probe cadence; the usage
                                   endpoint rate-limits (429) if hammered */
#define BACKOFF_429_MS 600000
#define RENEW_AHEAD_S 300
#define BODY_MAX 4096
/* Any epoch before this means SNTP has not run; expiry maths would lie. */
#define EPOCH_SANE 1700000000LL

typedef struct {
  char *buf;
  size_t cap, len;
  bool overflow;
} body_t;

static tk_oauth_tokens creds;
static char body[BODY_MAX];

static esp_err_t on_event(esp_http_client_event_t *e) {
  if (e->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  body_t *b = e->user_data;
  if (!b || b->overflow) return ESP_OK;
  if (b->len + (size_t)e->data_len >= b->cap) {
    b->overflow = true;
    return ESP_OK;
  }
  memcpy(b->buf + b->len, e->data, (size_t)e->data_len);
  b->len += (size_t)e->data_len;
  return ESP_OK;
}

/* One HTTPS request; returns the HTTP status, 0 on transport/overflow. The
 * body lands NUL-terminated in the shared buffer. */
static int https(esp_http_client_method_t method, const char *url,
                 const char *bearer, const char *post_json, size_t *len) {
  body_t b = { .buf = body, .cap = sizeof body };
  esp_http_client_config_t cfg = {
    .url = url,
    .method = method,
    .event_handler = on_event,
    .user_data = &b,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .timeout_ms = 15000,
  };
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (!c) return 0;
  char auth[TK_OAUTH_TOKEN_CAP + 16];
  if (bearer) {
    snprintf(auth, sizeof auth, "Bearer %s", bearer);
    esp_http_client_set_header(c, "Authorization", auth);
    esp_http_client_set_header(c, "anthropic-beta", "oauth-2025-04-20");
    esp_http_client_set_header(c, "User-Agent", "claude-cli/2.1.227 (external, cli)");
  }
  if (post_json) {
    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_post_field(c, post_json, (int)strlen(post_json));
  }
  esp_err_t err = esp_http_client_perform(c);
  int status = esp_http_client_get_status_code(c);
  esp_http_client_cleanup(c);
  if (err != ESP_OK || b.overflow) {
    ESP_LOGW(TAG, "request failed: %s", esp_err_to_name(err));
    return 0;
  }
  body[b.len] = '\0';
  if (len) *len = b.len;
  return status;
}

/* FNV-1a of the compiled-in seed: re-seeding secrets.h (the recovery path
 * after a dead refresh token) must beat whatever rotated pair NVS holds. */
static uint32_t seed_hash(void) {
  uint32_t h = 2166136261u;
  for (const char *p = TK_CLAUDE_REFRESH_TOKEN; *p; p++)
    h = (h ^ (uint8_t)*p) * 16777619u;
  return h;
}

static void nvs_load(void) {
  nvs_handle_t h;
  if (nvs_open("claude", NVS_READONLY, &h) != ESP_OK) return;
  uint32_t seed = 0;
  if (nvs_get_u32(h, "seed", &seed) != ESP_OK || seed != seed_hash()) {
    nvs_close(h);
    return;
  }
  size_t n = sizeof creds.refresh_token;
  char rt[TK_OAUTH_TOKEN_CAP];
  if (nvs_get_str(h, "rt", rt, &n) == ESP_OK && n > 1) {
    memcpy(creds.refresh_token, rt, n);
    n = sizeof creds.access_token;
    if (nvs_get_str(h, "at", creds.access_token, &n) != ESP_OK)
      creds.access_token[0] = '\0';
    int64_t exp = 0;
    if (nvs_get_i64(h, "exp", &exp) != ESP_OK) exp = 0;
    creds.expires_at = exp;
  }
  nvs_close(h);
}

static bool nvs_save(void) {
  nvs_handle_t h;
  if (nvs_open("claude", NVS_READWRITE, &h) != ESP_OK) return false;
  bool ok = nvs_set_str(h, "rt", creds.refresh_token) == ESP_OK &&
            nvs_set_str(h, "at", creds.access_token) == ESP_OK &&
            nvs_set_i64(h, "exp", creds.expires_at) == ESP_OK &&
            nvs_set_u32(h, "seed", seed_hash()) == ESP_OK &&
            nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

static int64_t now_epoch(void) { return (int64_t)time(NULL); }

/* Returns true when creds hold a fresh access token. A seed that never gets
 * accepted keeps failing visibly in the log; the panel shows dashes/stale. */
static bool renew(void) {
  char req[TK_OAUTH_TOKEN_CAP + 160];
  snprintf(req, sizeof req,
           "{\"grant_type\":\"refresh_token\",\"refresh_token\":\"%s\","
           "\"client_id\":\"%s\"}", creds.refresh_token, OAUTH_CLIENT_ID);
  size_t len = 0;
  int status = https(HTTP_METHOD_POST, REFRESH_URL, NULL, req, &len);
  memset(req, 0, sizeof req);
  if (status != 200) {
    ESP_LOGW(TAG, "token renewal refused (HTTP %d)%s", status,
             status == 400 || status == 401
                 ? " - refresh token dead: re-seed TK_CLAUDE_REFRESH_TOKEN" : "");
    return false;
  }
  tk_oauth_tokens fresh;
  if (!tk_oauth_refresh_parse(body, len, now_epoch(), &fresh)) {
    ESP_LOGW(TAG, "token renewal: unreadable answer, kept old tokens");
    return false;
  }
  creds = fresh;
  if (!nvs_save()) ESP_LOGE(TAG, "could not persist renewed tokens to NVS");
  ESP_LOGI(TAG, "access token renewed, valid %lld s",
           (long long)(creds.expires_at - now_epoch()));
  return true;
}

static void direct_task(void *arg) {
  (void)arg;
  torget_net_wait();
  vTaskDelay(pdMS_TO_TICKS(10000));

  strncpy(creds.refresh_token, TK_CLAUDE_REFRESH_TOKEN,
          sizeof creds.refresh_token - 1);
  nvs_load(); /* a rotated pair beats the compiled-in seed */

  for (;;) {
    int wait_ms = FETCH_EVERY_MS;
    int64_t now = now_epoch();
    if (now < EPOCH_SANE) {
      ESP_LOGW(TAG, "clock not set yet, waiting for SNTP");
      vTaskDelay(pdMS_TO_TICKS(15000));
      continue;
    }

    bool have = creds.access_token[0] && creds.expires_at - now > RENEW_AHEAD_S;
    if (!have) have = renew();

    if (have) {
      size_t len = 0;
      int status = https(HTTP_METHOD_GET, USAGE_URL, creds.access_token, NULL,
                         &len);
      if (status == 401 || status == 403) {
        /* Revoked or rotated elsewhere: one renewal, one retry. */
        creds.expires_at = 0;
        if (renew())
          status = https(HTTP_METHOD_GET, USAGE_URL, creds.access_token, NULL,
                         &len);
      }
      tk_tokens t;
      if (status == 200 && tk_claude_usage_parse(body, len, now_epoch(), &t)) {
        torget_ui_lock();
        tokens_apply(&t);
        torget_ui_unlock();
        ESP_LOGI(TAG, "usage ok (session %s, week %.0f%%)",
                 t.claude_session.has_pct ? "active" : "idle",
                 t.claude_week.has_pct ? t.claude_week.pct : -1.0);
      } else if (status == 429) {
        ESP_LOGW(TAG, "rate limited, backing off 10 min");
        wait_ms = BACKOFF_429_MS;
      } else {
        ESP_LOGW(TAG, "usage rejected (HTTP %d), values stay", status);
      }
    }
    /* A failed round changes nothing: the tick turns stale after two minutes. */
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
  }
}

void claude_direct_start(void) {
  xTaskCreate(direct_task, "claude-direct", 8192, NULL, 5, NULL);
}

#endif /* TK_CLAUDE_REFRESH_TOKEN */
