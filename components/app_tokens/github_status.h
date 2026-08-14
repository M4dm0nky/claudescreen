#ifndef GITHUB_STATUS_H
#define GITHUB_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#define TK_GITHUB_REPO_CAP 202
#define TK_GITHUB_PROJECT_CAP 101
#define TK_GITHUB_EVENT_ID_CAP 65
#define TK_GITHUB_ACTOR_CAP 40

typedef struct {
  bool enabled;
  bool stale;
  bool has_data;
  bool has_event;
  bool has_actor;
  int32_t stars;
  int32_t forks;
  int32_t event_stars;
  char repo[TK_GITHUB_REPO_CAP];
  char project[TK_GITHUB_PROJECT_CAP];
  char event_id[TK_GITHUB_EVENT_ID_CAP];
  char actor[TK_GITHUB_ACTOR_CAP];
} tk_github_status;

#endif

