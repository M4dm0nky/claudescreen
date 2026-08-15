# Release: the value multiple and the GitHub star pulse (2026-08-15)

Two optional features reach the glass, side by side. One answers the question
every subscriber has standing every month — *am I getting my money's worth?* —
by pricing the tokens your agents already logged against what you pay. The
other lets the panel carry a public repo's star count and celebrate a new
star, by name. Neither is on by default; each is something you turn on.

## Enable / disable — this is opt-in, both halves

- **The GitHub star pulse is entirely opt-in**, via three independent,
  default-off flags in `secrets.h` (a fresh clone stays Claude/Codex-only
  until you set them):
  - `TK_GITHUB_SCREEN_ENABLED` — the star + fork screen (one swipeable tile).
  - `TK_GITHUB_NOTIFICATIONS_ENABLED` — the full-screen popup when a new star
    lands.
  - `TK_GITHUB_SOUND_ENABLED` — sound on that popup (a third, separate opt-in).

  Plus point the tokenserver at a repo: `--github-repo owner/repository`
  (and `TK_GITHUB_URL` in `secrets.h`). Leave any flag at `0` and that part
  simply isn't built. See `docs/github-pulse.md`.
- **The value multiple is opt-in by declaration.** It shows nothing confident
  until you tell the tokenserver what you actually pay. Declare it and the
  tile lights up; leave it out and the page shows the dollars but dashes the
  multiple rather than guess. See `docs/value-multiple.md`.

## The value multiple

- **What it is.** Your month-to-date agent usage, priced at published list API
  rates and divided by your subscription cost. *"$516 of Codex value against a
  $100 plan — 5.16×."* Same data as the Usage page, aimed at a different
  question. Turn it on by stating your real costs:
  `--plan claude=200 --plan codex=100` (dollar overrides), or the named tiers
  `--claude-plan {pro,max5x,max20x}` / `--codex-plan {plus,pro}`.
- **It refuses to guess.** A model the price table doesn't know becomes
  *unpriced* tokens; past a small tolerance the whole multiple degrades to a
  dash rather than show a confident wrong number. Rates are generated from a
  maintained public catalogue, never hand-typed.
- **The Codex overcount, fixed.** The Codex half read **$8,296** for a month
  that was ~10% of a weekly quota. The rate was never wrong (~$0.67/Mtok, right
  next to Claude's) — the token *count* was inflated ~16×. `codex resume`,
  fork and subagent-spawn each replay the parent conversation's entire
  `token_count` history, re-timestamped, under the same `session_id`; summing
  every rollout counted one conversation once per resume (a real `~/.codex`
  held a single conversation across 113 files). Now grouped by `session_id`
  and counted once, from the most-complete rollout. **$8,296 → $516.**

## The GitHub star pulse

- **What it is.** The Mac tokenserver polls a public repo's star and fork
  count (TLS, rate-limiting and backoff all live on the computer) and
  republishes one flat LAN payload on `/api/github`; the panel shows a tile
  and, when a new star lands, a full-screen celebration. The display never
  talks to GitHub.
- **The name, read-only.** The celebration names *who* just starred. Reading
  that used to fall back to "someone": GitHub gates the REST stargazers list
  behind a **write** permission. It now reads the newest star from the public
  **events feed** (`WatchEvent`), which needs only `metadata=read` — so a
  **fine-grained, public-repositories, read-only** token resolves the name
  with the least privilege possible. No token still works; it just shows
  "someone" as before. The token lives in git-ignored `secrets.h`
  (`#define TG_GITHUB_TOKEN "…"`), never committed.

## Both on one panel

The value tile and the GitHub tile now coexist in the swipeable usage strip —
eight tiles: the Claude/Codex quota and tracker views, then GitHub, then the
value multiple. Neither replaces the other; you swipe between them.

## Verified

Host suite green; the firmware built once and was delivered over the air with
consent on the glass; the panel reports both tiles live and the value figure
at the corrected $516. The read-only token was proven against the real repo
(HTTP 200, name resolves) before going live.
