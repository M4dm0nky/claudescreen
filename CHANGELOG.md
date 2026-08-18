# Changelog

Notable changes to VibePulse. Release notes for this fork's tagged versions are
published on [its own releases page](https://github.com/M4dm0nky/claudescreen/releases);
upstream's live at
[niclasvestlund-YT/vibepulse](https://github.com/niclasvestlund-YT/vibepulse/releases).

## Unreleased

### Fixed

- The KEY button works on this fork's board. `main.c` was still polling
  `GPIO_NUM_18` — KEY3 on the upstream AMOLED-2.16 — while the
  ESP32-S3-Touch-LCD-1.54 wires its single button, silkscreened KEY, to GPIO4.
  With an internal pull-up an unconnected pin reads "released" forever, so
  every intent on that button was dead: app switching, the Needs You panic,
  and the OTA maintenance window, which by design opens from nowhere else. The
  port had written the right pins into its own BSP and used them nowhere. The
  pin was measured, not guessed (all three candidates logged under a press;
  only GPIO4 moved), and the glass now says `KEY CLOSES` instead of naming a
  button this board does not have.
- Two sender gates that were not actually guarding. The CI bridge called
  `gh run list` without `-R`, so with no default repo set it could answer for
  a *different* repository and report "no green CI" for a commit with two
  green runs — the repo is now derived from `origin`. And the `-dirty` gate
  trusted a version string that CMake computes at configure time and then
  caches, so a build from a dirty tree could carry a clean name and sail
  through; the image is now compared against `git describe --tags --dirty` at
  send time (`TG_OTA_ALLOW_STALE=1` overrides). That is OBS-32, closed.
- The hardware registry can hold a measurement taken on a second board. A
  capability may now declare its own `board:`, defaulting to the file header,
  and a `verification:` unit is matched against *that*. Until now a pin
  measured on the physical 1.54 unit could not be recorded at all, because the
  registry header still named the AMOLED board.

- The panel names all three GPT-5.6 variants. `gpt-5.6-sol` had a typeset
  screen label while its siblings `terra` and `luna` fell through to their
  raw lowercase ids — the price table knew all three, the screen knew one,
  so the agent tile read `gpt-5.6-terra` next to a properly set `OPUS 5`. A
  test now also holds every label inside `TK_AGENT_MODEL_CAP`, reading the
  cap from the firmware header rather than restating it. Spotted on Erik
  Elfström's T-Display-S3 fork. The wider fallthrough — ~110 priced models,
  six named ones, and dated ids that truncate mid-string — is written up as
  OBS-30 rather than fixed here.
- CI's tokenserver job runs the same eleven test modules as `test/run.sh`.
  The lists had drifted four suites apart — `test_value_meter`,
  `test_update_prices`, `test_codex_usage` and `test_interactions` ran only
  in the local gate — which is exactly how a green CI hid a runtime
  `NameError` in the rebased Windows branch (PR #11): the missing
  `test_interactions` catches it immediately.

### Added

- The five-hour session window has its own page, and it is the first one, so
  the number that decides whether a long run finishes needs no swipe. Every
  later `VIEW_*` shifts one step right. The data already reached the device
  (`claudeSessionPct`/`ResetMin`/`HourDeltaPct` parsed into `claude_session`)
  and `USAGE_CARD_FIVE_HOURS` already existed — neither was read by anything.
- `claudeSessionState` (`active`/`idle`/`unknown`) in the `/api/tokens`
  contract. An absent percentage used to mean two different things — no window
  is running, or the probe failed — and the screen could only dash for both.
  The state is derived from what `get_limits()` returned, never from
  `claudeWeekPct`: the week can come from the disk cache long after the probe
  died, the session never can. Only a fresh probe that saw an empty window
  earns the honest `0%` and its `STARTS ON NEXT REQUEST` note. An unknown or
  absent state string parses as `unknown`, so an older tokenserver behaves
  exactly as before.
- The tokenserver reads Claude's OAuth token on Windows. Claude Code has no
  keychain integration there, so `claude login` writes the same
  `{"claudeAiOauth": {...}}` record the macOS keychain holds to a plain file,
  `%USERPROFILE%\.claude\.credentials.json`; the probe now reads it when
  running on Windows and skips the two macOS-only sources (`security`,
  `pgrep` for Claude Desktop's injected token) that cannot exist there. macOS
  behaviour is untouched.

  Two things had to give way for that read to be reachable at all: `fcntl`
  is not importable on Windows, so the module could not even load, and the
  machine-wide single-probe lock was built on `flock`. The import is now
  guarded and the lock takes `msvcrt.locking` where `flock` is missing —
  same non-blocking gate, different syscall — so the 429 guard survives the
  port instead of quietly disappearing with it.

  The Codex half works there too. Its quota read spawns `codex app-server`
  and polled stdout with `select.select`, which on Windows accepts sockets
  and never pipes; it now reads through a queue fed by a daemon thread, the
  same code on every platform. That path had no test at all — every existing
  test mocked the reader out and exercised only the parser — so it now has
  three, driving a real subprocess through the real pipe for the reply,
  timeout and immediate-death cases. Writing them turned up a leak worth
  fixing on its own: the pipes were never closed, leaving three descriptors
  per poll to the garbage collector in a service that polls every 30 s and
  never restarts.

  State and logs moved off the hardcoded `~/Library` paths to a per-platform
  directory — `%LOCALAPPDATA%\VibePulse\` on Windows, unchanged on macOS.
  The old paths worked literally on Windows but planted a `Library` tree in
  the user profile that nothing else on the machine recognises.

  What remains for [#3](https://github.com/niclasvestlund-YT/vibepulse/issues/3)
  is autostart: the launchd plist has no Windows equivalent, and `smoke.py`
  now finds the right state directory but still tells you to run `launchctl`.
  Reported by Erik Elfström, who found it porting a fork to a LilyGO
  T-Display-S3.
- The completion alert finally pulses. The accent outline and icon ring
  breathe (full → 39 % → full, ease-in-out, four 1200 ms cycles filling the
  PULSE phase exactly) and then rest; text and the provider icon stay solid
  for readability. Proven pixel-by-pixel in the simulator and reviewed on
  the physical panel
  ([review](docs/superpowers/reviews/2026-08-14-completion-pulse-physical-motion.md)).
  The static attention gate now permits exactly this one animation and pins
  its shape.

### Changed

- The pages are ordered by how often you look at them instead of by when
  they were written: five-hour session, Claude weekly, burn rate, Claude Max
  Tracker, value, Fable weekly, then Codex weekly, Codex Max Tracker and
  GitHub. The Codex pages no longer sit between the Claude ones, and value
  moved from last to fifth.

  GitHub landing last is not taste. It is the only page that can be compiled
  out (`TK_GITHUB_SCREEN_ENABLED`), and `TK_USAGE_SCREEN_VIEWS` shrinks with
  it — so at index 7 with value behind it, a clone with GitHub disabled had
  the value tile write `ui.tiles[8]` into an eight-element array. Last is the
  only position where switching a page off leaves no hole, and the wiring
  test now says so with the reason attached.

  Nothing but the `VIEW_*` enum had to move: every tile takes its column from
  its constant, and the pager, the edge swipe directions and the simulator's
  ~30 `tokens_show_view` calls all follow. Verified by measuring the pager
  row in fresh 240×240 captures — nine dots, the wide one at 0/1/3/4/5/6/7/8
  for each page in turn — and by an ASan/UBSan build with GitHub compiled
  out, which draws eight dots and stays clean.

## v0.2.1 — 2026-08-13

Server fixes verified live on a real installation the same evening; the
firmware alert fix reaches a device on its next flash.

### Fixed

- Repeated probe failures now slow the probe down (120 → 240 → 480 s cap), so
  a dead token can never again hammer the API every two minutes for hours —
  the pattern that earned tonight's 429 penalty. A successful probe restores
  the normal pace. The root endpoint also reports `rev` and `startedAt`, so a
  stale running process (wrong directory, old code) is visible in one curl.
- The Claude probe backs off on HTTP 429: it stops the cycle immediately (no
  second token source, no header probe — extra traffic only extends the
  penalty) and rests for at least ten minutes, honouring a longer
  `Retry-After` when the API sends one. `claudeProbe` shows
  `usage_http_429 + backoff_until_HH:MM` while resting.
- The Claude probe no longer requires an active 5-hour session window to
  count as successful. Between windows the usage API reports the session row
  with a lapsed reset, and the probe used to discard the still-valid weekly
  numbers, fall back to the header probe, and report its 401 instead — so the
  screen lost all Claude data for the gap after every window ended. Weekly
  and per-model figures now go through on their own; the session field shows
  a dash until the next window opens. The header-probe fallback also appends
  its outcome (`; fallback_http_…`) instead of overwriting the usage status,
  so `claudeProbe` keeps the evidence.
- The tokenserver's Claude probe no longer trusts a stale token frozen into a
  long-lived Claude Desktop child process. `ps eww` reports the environment as
  of process launch, so a Desktop child that outlives its token kept serving
  an expired value that outranked a fresh `/login` in the keychain — the
  screen sat on `http_401` until Claude Desktop was quit. The probe now tries
  each token source in order and falls back on 401/403.
- Firmware: full-screen alerts (NEEDS YOU, DONE, ERROR) now require the state
  change to be fresher than 2 minutes after boot too, not only on the first
  snapshot. Waiting states that are hours old — rediscovered after a
  tokenserver outage or restart — no longer take over the screen; they appear
  in the header only. Reaches a device on its next flash.

### Known

- The alert's pulse phase has no visual effect yet: the 4.8 s PULSE phase and
  the STATIC phase render identical frames, so the alert appears without any
  attention-drawing motion. An actual pulse is motion work gated behind the
  AMOLED review protocol (simulator frames, static physical review, measured
  motion on the panel).

## v0.2.0 — 2026-08-13

One app: VibePulse is the only app in the repository and the screen boots
into it. Corrected README claims (the six real pages, the privacy scope of
what the screen receives and what a lost screen carries). `secrets.h.example`
ships its URLs active with a `DIN-MAC` placeholder instead of commented out.
New `docs/agent-setup.md` runbook for coding agents. Companion apps resolve
during ESP-IDF early expansion; the host test gate runs headless on Linux.

## v0.1.0 — 2026-08-13

First public release. Its tag predates the history cleanup and no longer
builds from a fresh clone; superseded by v0.2.0.
