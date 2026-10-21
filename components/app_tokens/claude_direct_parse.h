#ifndef CLAUDE_DIRECT_PARSE_H
#define CLAUDE_DIRECT_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tokens.h"

/*
 * Direct mode: the panel asks api.anthropic.com itself instead of the Mac
 * tokenserver. These two parsers are the pure half (host-tested, no ESP-IDF).
 *
 * tk_claude_usage_parse maps the OAuth usage contract ({"limits":[...]}) onto
 * the Claude fields of tk_tokens, mirroring tokenserver's _parse_usage_limits:
 * an expired row, an out-of-range percent or an unknown scoped pool is left
 * out rather than guessed. Everything the Mac derived from local logs (day
 * tokens, forecasts, value) stays zero/UNAVAILABLE, so those views dash.
 * *out is untouched unless the body is a valid usage document.
 */
bool tk_claude_usage_parse(const char *json, size_t len, int64_t now_epoch,
                           tk_tokens *out);

#define TK_OAUTH_TOKEN_CAP 512

typedef struct {
  char access_token[TK_OAUTH_TOKEN_CAP];
  char refresh_token[TK_OAUTH_TOKEN_CAP];
  int64_t expires_at; /* epoch seconds */
} tk_oauth_tokens;

/* Parses the refresh response. Both tokens are required: a rotating refresh
 * token that is dropped would strand the panel at the next renewal. */
bool tk_oauth_refresh_parse(const char *json, size_t len, int64_t now_epoch,
                            tk_oauth_tokens *out);

/* ISO-8601 UTC ("2026-08-14T10:00:00Z", fractions and +00:00 accepted) or a
 * plain epoch number to epoch seconds; false on anything else. */
bool tk_parse_epoch(const char *text, int64_t *out);

#endif
