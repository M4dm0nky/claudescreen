# Direct mode: Claude quota without the Mac

With `TK_CLAUDE_REFRESH_TOKEN` defined in `secrets.h`, the panel fetches the
Claude quota itself from `https://api.anthropic.com/api/oauth/usage` and
renews its own OAuth token. The Mac tokenserver is not needed for these pages.

**Works:** 5-hour session, Claude weekly, model weekly (Fable/Opus/Sonnet) and
the dollar credits (see below).
**Not built in this mode** (they need the Mac's local logs, so they would be
dashes forever): burn rate, both Max Trackers, Value, Codex weekly, GitHub.
`TK_DIRECT_MODE` (derived from `TK_CLAUDE_REFRESH_TOKEN`, see
`app_tokens_config.h`) builds exactly four pages: session, weekly, model
weekly, credits. The left "used today / last hour" statistic is not drawn
either. NEEDS YOU, agent activity and the OTA announce still need the Mac.

The model-weekly page is kept because the API can fill it once your plan has a
model-scoped limit; today it shows dashes. If it stays empty, remove it.

## Credits page

The usage response carries a dollar pool (limit / used / remaining and an
expiry) under a codename that is not stable. The parser therefore finds it by
shape, not by name: exactly one top-level object with `limit_dollars`,
`used_dollars`, `remaining_dollars` that add up and a future `resets_at`.
Zero or several matches show dashes, never a guess. The hero is what is left
(rounded to whole dollars; 1000 or more is dashed because the hero font cannot
draw four digits), the bar is the share spent, and the page shows the amount
spent and the time to expiry. When the pool expires the page shows dashes;
remove it then.

## Token lifecycle

- Anthropic **rotates the refresh token on every renewal**. The pair is kept in
  NVS (namespace `claude`); the `secrets.h` value is only the first-boot seed.
- Renewal runs 5 minutes before expiry and once after a 401/403, never in a
  loop. The new refresh token is saved before the access token is used.
- Re-seeding `secrets.h` with a different value overrides NVS (recovery path
  when the refresh token died, e.g. after being unused for weeks or revoked).
- Polling is every 120 s; a 429 backs off 10 minutes.
- Needs SNTP time; until the clock is set the task waits.

## Give the panel its own login (do not share the Mac's)

A refresh token shared with Claude Code on the Mac is a race: whichever side
renews first invalidates the other's token, and one of them gets logged out.
Create a separate login for the panel:

1. `CLAUDE_CONFIG_DIR=~/.claude-panel claude` then `/login` (same account).
2. Read the `claudeAiOauth.refreshToken` from that login's credentials
   (macOS keychain item `Claude Code-credentials` for that config dir, or
   `~/.claude-panel/.credentials.json`).
3. Put it in `secrets.h` as `TK_CLAUDE_REFRESH_TOKEN`; never commit it.
4. Do not use that config dir for real work afterwards.

`claude setup-token` tokens are inference-only and do not read usage, so they
do not work here.

## Security

The token is compiled into the firmware image (as are the WiFi credentials). A
lost panel leaks a revocable Claude login for that account. Revoke it by
signing out of the panel's session in Claude settings.

## Verified 2026-10-21 on `torget-lcd-154-01`

Confirmed on the real panel with its own login (`~/.claude-panel`):

- The usage endpoint answers with the `limits[]` shape the parser expects; the
  real response is pinned as a host test (`test_usage_real_capture`).
- The renewal exchange works: the panel had only a refresh token seeded and
  still read usage (`claude-direct: usage ok (session active, week 20%)`).
- Heap held (internal low-water 167 KB, largest block 90 KB) with the HTTPS
  task running; no NO_MEM.

**Not yet seen:** that a rotated pair survives a reboot after the 8-hour access
token expires. Watch the log the first time the panel runs past that point.
