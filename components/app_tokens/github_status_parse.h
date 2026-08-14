#ifndef GITHUB_STATUS_PARSE_H
#define GITHUB_STATUS_PARSE_H

#include <stdbool.h>
#include <stddef.h>

#include "github_status.h"

bool tk_github_status_parse(const char *json, size_t len,
                            tk_github_status *out);

#endif

