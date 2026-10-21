# Handoff: direct mode, to the local agent

Written 2026-10-21 by a cloud session that had no access to the Mac, the
device, `secrets.h`, `.ota-device` or ESP-IDF. Everything below marked
UNVERIFIED has never run against the real API or the real panel. You have the
access it lacked: finish and verify it.

## Goal (from the owner)

The panel must not depend on the Mac. It should fetch Claude quota straight
from the internet and keep working when the OAuth token renews itself.
Scope the owner chose: **quota pages only** (5h session, Claude weekly,
model weekly). Mac-only features (NEEDS YOU, agent activity, Max Tracker,
burn forecast, value, Codex) may dash in this mode. Device is next to the
owner, on WiFi. Panel: `torget-lcd-154-01`, last known firmware
`v0.6.0-14-g7d08d6a` (spec/device-units.yaml).

## What exists (branch `claude/laughing-babbage-nmeckk`)

| File | Role |
|---|---|
| `components/app_tokens/claude_direct_parse.{h,c}` | Pure parsers: usage JSON -> `tk_tokens`, refresh JSON -> token pair, ISO/epoch time. Host-tested. |
| `components/app_tokens/claude_direct_net.c` | Device task: HTTPS to `api.anthropic.com/api/oauth/usage`, refresh via `platform.claude.com/v1/oauth/token`, pair persisted in NVS ns `claude` (keys rt/at/exp/seed). |
| `components/app_tokens/net.c` | `TK_CLAUDE_REFRESH_TOKEN` defined => direct task replaces the Mac `/api/tokens` poll. |
| `test/test_claude_direct_parse.c` + `test/run.sh` | Host tests (pass). |
| `secrets.h.example`, `docs/direct-mode.md` | Config and user docs. |

Behaviour: poll every 120 s, 429 => 10 min back-off, renew 300 s before
expiry and once after 401/403, new refresh token saved to NVS BEFORE use,
re-seeding `secrets.h` (different value) overrides NVS, waits for SNTP.

## What is UNVERIFIED (check these first)

1. The firmware has never been compiled (`idf.py build`). Expect possible
   include/REQUIRES errors in `claude_direct_net.c` (nvs, esp-tls, time).
2. Refresh endpoint URL and `client_id` `9d1c250a-e61b-44d9-88ed-5944d1962f5e`
   come from memory of Claude Code's public behaviour. Confirm they answer;
   compare with what the installed Claude Code actually calls. If the
   response field names differ (`access_token`, `refresh_token`,
   `expires_in`), fix `tk_oauth_refresh_parse` and its test.
3. The usage JSON shape (`limits[]` with kind/percent/resets_at/scope) is
   copied from `tools/tokenserver/tokenserver.py::_parse_usage_limits`. Fetch
   a real response once and diff it against the parser.
4. TLS/heap: mbedTLS lives in PSRAM (sdkconfig.defaults). Watch for NO_MEM on
   the SPI flush when the HTTPS task runs; stagger the task if so (see the
   phase-offset comments in `net.c`).
5. 8 KB task stack and the 4 KB shared body buffer are guesses.

## Hard constraint: the panel needs its OWN login

Anthropic rotates refresh tokens. If the panel shares a grant with Claude
Code on the Mac, whoever renews first logs the other out. Do not seed the
panel from the Mac's keychain item. Create a separate login
(`CLAUDE_CONFIG_DIR=~/.claude-panel claude`, `/login`), take its
`claudeAiOauth.refreshToken`, put it in `secrets.h` as
`TK_CLAUDE_REFRESH_TOKEN`, and never use that config dir for real work.
`claude setup-token` tokens are inference-only and cannot read usage.
Never print, log or commit the token; never write it to the repo.

## Steps for you

1. `git fetch origin claude/laughing-babbage-nmeckk && git checkout` it.
2. `./test/run.sh` (venv per README "Hardware knowledge").
3. Help the owner create the separate login; add `TK_CLAUDE_REFRESH_TOKEN`
   to the git-ignored `secrets.h`. Comment out the Mac URLs
   (`TK_TOKENS_URL`, `TK_AGENT_STATUS_URL`, `TK_MAX_TRACKER_URL`,
   `TK_GITHUB_URL`) so no dead tasks log failures. Consider whether
   `TK_VIBEPULSE_BASE_URL`-dependent code compiles with them removed.
4. `idf.py build`; fix compile errors.
5. Flash ONLY on the owner's explicit request. Preferred path is OTA:
   `tools/ota-flash.sh` (IP from `.ota-device`). The owner must hold KEY
   (GPIO4) for 3 s; no script can open the window. Respect the sender gates
   (no `TG_OTA_ALLOW_*` bypass without the owner saying so; CI must be green
   on the pushed commit, so push first and check Actions).
6. Verify on the glass and in logs (`idf.py monitor`, USB): expect
   `claude-direct: access token renewed` then `usage ok`. Test the renewal
   path: force `exp` stale (e.g. temporarily shorten RENEW_AHEAD_S or erase
   the NVS `claude` namespace) and confirm the rotated pair survives a reboot.
7. Check the static physical panel per CLAUDE.md before motion; the pages
   will show dashes for Mac-only data. Decide with the owner whether dashed
   pages should be hidden in direct mode (not done).
8. Fix what the real API contradicts, update `docs/direct-mode.md` (drop the
   "Unverified" section once proven), add a `docs/lessons.md` entry for
   anything with a root-cause story, and note the verified date in
   `spec/device-units.yaml` only after a physical check.

## Not done / open

- Hiding Mac-only pages in direct mode.
- Codex quota without the Mac (needs a second login; out of scope so far).
- Simulator support for direct mode (sim reads fixtures, unchanged).
- README headline/section for the feature (CLAUDE.md requires it when the
  feature ships and is proven; add frames then).
