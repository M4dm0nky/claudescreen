#include "github_status_parse.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../../third_party/cjson/cJSON.h"

static bool whitespace(unsigned char byte) {
  return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r';
}

/* cJSON stores decoded NUL inside a C string without exposing its decoded
 * length. Reject it before parsing so no identity can be silently truncated. */
static bool lexical_guard(const char *json, size_t len) {
  bool in_string = false;
  bool escaped = false;
  for (size_t i = 0; i < len; i++) {
    unsigned char byte = (unsigned char)json[i];
    if (!in_string) {
      if (byte == '"') in_string = true;
      else if (byte < 0x20 && !whitespace(byte)) return false;
      continue;
    }
    if (escaped) {
      escaped = false;
      if (byte == 'u' && i + 4 < len &&
          json[i + 1] == '0' && json[i + 2] == '0' &&
          json[i + 3] == '0' && json[i + 4] == '0') {
        return false;
      }
    } else if (byte == '\\') {
      escaped = true;
    } else if (byte == '"') {
      in_string = false;
    } else if (byte < 0x20) {
      return false;
    }
  }
  return !in_string && !escaped;
}

static bool trailing_whitespace(const char *json, size_t len,
                                const char *parse_end) {
  if (!parse_end || parse_end < json || parse_end > json + len) return false;
  for (const char *cursor = parse_end; cursor < json + len; cursor++) {
    if (!whitespace((unsigned char)*cursor)) return false;
  }
  return true;
}

static bool allowed_name(const char *name, const char *const *allowed) {
  if (!name) return false;
  for (size_t i = 0; allowed[i]; i++) {
    if (strcmp(name, allowed[i]) == 0) return true;
  }
  return false;
}

static bool exact_unique_fields(const cJSON *object,
                                const char *const *allowed) {
  if (!cJSON_IsObject(object)) return false;
  for (const cJSON *item = object->child; item; item = item->next) {
    if (!allowed_name(item->string, allowed)) return false;
    for (const cJSON *other = item->next; other; other = other->next) {
      if (other->string && strcmp(item->string, other->string) == 0) {
        return false;
      }
    }
  }
  return true;
}

static bool exact_int32(const cJSON *item, int32_t *out) {
  if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) ||
      item->valuedouble < 0 || item->valuedouble > INT32_MAX ||
      floor(item->valuedouble) != item->valuedouble) {
    return false;
  }
  *out = (int32_t)item->valuedouble;
  return true;
}

static bool copy_string(const cJSON *item, char *out, size_t cap) {
  if (!cJSON_IsString(item) || !item->valuestring) return false;
  size_t len = strlen(item->valuestring);
  if (len == 0 || len >= cap) return false;
  memcpy(out, item->valuestring, len + 1);
  return true;
}

static bool repo_char(unsigned char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
         (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
}

static bool valid_repo(const char *value) {
  const char *slash = strchr(value, '/');
  if (!slash || slash == value || slash[1] == '\0' ||
      strchr(slash + 1, '/')) return false;
  for (const char *cursor = value; *cursor; cursor++) {
    if (*cursor != '/' && !repo_char((unsigned char)*cursor)) return false;
  }
  return true;
}

static bool valid_project(const char *value) {
  for (const char *cursor = value; *cursor; cursor++) {
    if (!repo_char((unsigned char)*cursor)) return false;
  }
  return true;
}

static bool valid_actor(const char *value) {
  size_t len = strlen(value);
  if (len == 0 || len > 39 || value[0] == '-' || value[len - 1] == '-') {
    return false;
  }
  for (const char *cursor = value; *cursor; cursor++) {
    unsigned char ch = (unsigned char)*cursor;
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '-')) return false;
  }
  return true;
}

static bool valid_event_id(const char *value) {
  for (const char *cursor = value; *cursor; cursor++) {
    unsigned char ch = (unsigned char)*cursor;
    if (ch < 0x21 || ch > 0x7e) return false;
  }
  return true;
}

bool tk_github_status_parse(const char *json, size_t len,
                            tk_github_status *out) {
  if (!json || !out || len == 0 || !lexical_guard(json, len)) return false;

  const char *parse_end = NULL;
  cJSON *root = cJSON_ParseWithLengthOpts(json, len, &parse_end, false);
  if (!root || !trailing_whitespace(json, len, parse_end)) {
    cJSON_Delete(root);
    return false;
  }

  static const char *const allowed[] = {
      "v", "enabled", "repo", "project", "stars", "forks", "stale",
      "eventId", "actor", "eventStars", NULL,
  };
  tk_github_status parsed = {0};
  cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "v");
  cJSON *enabled = cJSON_GetObjectItemCaseSensitive(root, "enabled");
  int32_t version_value = 0;
  bool ok = exact_unique_fields(root, allowed) &&
            exact_int32(version, &version_value) && version_value == 1 &&
            cJSON_IsBool(enabled);

  if (ok && !cJSON_IsTrue(enabled)) {
    ok = root->child && root->child->next &&
         root->child->next->next == NULL;
  } else if (ok) {
    parsed.enabled = true;
    cJSON *repo = cJSON_GetObjectItemCaseSensitive(root, "repo");
    cJSON *project = cJSON_GetObjectItemCaseSensitive(root, "project");
    cJSON *stale = cJSON_GetObjectItemCaseSensitive(root, "stale");
    ok = copy_string(repo, parsed.repo, sizeof parsed.repo) &&
         valid_repo(parsed.repo) &&
         copy_string(project, parsed.project, sizeof parsed.project) &&
         valid_project(parsed.project) && cJSON_IsBool(stale);
    parsed.stale = cJSON_IsTrue(stale);

    cJSON *stars = cJSON_GetObjectItemCaseSensitive(root, "stars");
    cJSON *forks = cJSON_GetObjectItemCaseSensitive(root, "forks");
    if (ok && (stars || forks)) {
      ok = stars && forks &&
           exact_int32(stars, &parsed.stars) &&
           exact_int32(forks, &parsed.forks);
      parsed.has_data = ok;
    }

    cJSON *event_id = cJSON_GetObjectItemCaseSensitive(root, "eventId");
    cJSON *event_stars =
        cJSON_GetObjectItemCaseSensitive(root, "eventStars");
    cJSON *actor = cJSON_GetObjectItemCaseSensitive(root, "actor");
    if (ok && (event_id || event_stars || actor)) {
      ok = parsed.has_data && event_id && event_stars && actor &&
           copy_string(event_id, parsed.event_id, sizeof parsed.event_id) &&
           valid_event_id(parsed.event_id) &&
           exact_int32(event_stars, &parsed.event_stars) &&
           parsed.event_stars == parsed.stars &&
           (cJSON_IsNull(actor) ||
            (copy_string(actor, parsed.actor, sizeof parsed.actor) &&
             valid_actor(parsed.actor)));
      parsed.has_actor = ok && cJSON_IsString(actor);
      parsed.has_event = ok;
    }
  }

  cJSON_Delete(root);
  if (!ok) return false;
  *out = parsed;
  return true;
}

