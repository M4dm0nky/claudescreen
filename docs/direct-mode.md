# Direct mode: Claude quota without the Mac

With `TK_CLAUDE_REFRESH_TOKEN` defined in `secrets.h`, the panel fetches the
Claude quota itself from `https://api.anthropic.com/api/oauth/usage` and
renews its own OAuth token. The Mac tokenserver is not needed for these pages.

**Works:** 5-hour session, Claude weekly, model weekly (Fable/Opus/Sonnet).
**Dashes (needs the Mac's local logs):** day/month tokens, burn forecast, value
multiple, agent activity, NEEDS YOU, Max Tracker, Codex, GitHub, OTA announce.

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

## Unverified (see [direct-mode-handoff.md](direct-mode-handoff.md))

The refresh endpoint (`platform.claude.com/v1/oauth/token`) and the public
Claude Code client id are taken from Claude Code's public behaviour and have
**not** been exercised from this cloud session (no credentials, no device).
The parsers are host-tested; the first on-device run decides whether the
endpoint answers. Watch the `claude-direct` log lines.
