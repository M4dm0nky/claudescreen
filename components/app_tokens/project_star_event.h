#ifndef PROJECT_STAR_EVENT_H
#define PROJECT_STAR_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "github_status.h"

#define TK_PROJECT_EVENT_SOURCE_CAP 16

typedef struct {
  char id[TK_GITHUB_EVENT_ID_CAP];
  char source[TK_PROJECT_EVENT_SOURCE_CAP];
  char actor[TK_GITHUB_ACTOR_CAP];
  char repo[TK_GITHUB_REPO_CAP];
  char project[TK_GITHUB_PROJECT_CAP];
  int32_t stars;
  bool has_actor;
} tk_project_star_event;

#endif
