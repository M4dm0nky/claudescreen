# VibePulse as a companion — feature brainstorm

**Written:** 2026-08-13. **Status:** brainstorm. Nothing here is authorized
work, no capability is promoted, no flash is implied.

The brief was: *find features that make this a companion for vibe coders,
not just a mirror of numbers I can already see — and be certain they're not
redundant or too much.* Then: *can the screen answer back?*

This is a filtering document. Ideas that failed are written down as failures
so they don't get re-proposed.

**The three tests**

1. **Not a mirror.** Can you already see it in the terminal, in `ccusage`, or
   in a menu-bar app?
2. **Glanceable.** Does it survive being one dominant thing at 480 x 480,
   read from three metres? `ui-spec.md`: *"En ny informationsrad på skärmen
   är ett regelbrott."*
3. **Honest.** Buildable without inventing a number, with a label that says
   what the number actually measures.

---

## The uncomfortable finding, first

Between May and August 2026 the "agent numbers on a small screen" lane went
from empty to crowded. This matters more than any individual feature idea,
so it goes first.

| What shipped | Overlap |
|---|---|
| **[Token Monitor](https://tokenmonitor.dev/)** — €99 Kickstarter, **480×480, ESP32-S3**, quota + session limits + reset timers + cost for Claude Code, Codex **and** Antigravity | Everything on the Usage and Burn Rate pages |
| **[Clawdmeter](https://www.hackster.io/news/keep-tabs-on-claude-with-the-cute-animated-clawdmeter-744383d44094)** — same Waveshare board, animated mascot, session + weekly | The original inspiration; VibePulse already differentiates |
| **[anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy)** — **official** local BLE API, reference firmware where **A = approve, B = reject** on a pending permission request | NEEDS YOU, *and* the approve-from-device idea |
| **[AgentDeck](https://github.com/puritysb/AgentDeck)** — 26 surfaces, per-session keys, "see which agent is waiting on you", YES/NO/ALWAYS, STOP, mode cycling | The live header, NEEDS YOU, and two-way control |
| AgentMeter, Hermes Meter, ClaudeGauge, m5stack-claude-code-buddy | Usage and alerting |

Plus, on the software side, `ccusage` covers **16 agent sources** for tokens
and cost; `claude-monitor` has done burn-rate depletion prediction in a rich
TUI since 2025; there are at least five macOS menu-bar apps doing 5-hour +
weekly + Opus-week; and there are official Grafana dashboards.

**Read it honestly:** three of the five pages — Usage, Burn Rate, and
arguably the live header — are now table stakes. Adding a seventh page of
numbers walks further into a fight that can't be won on novelty, against a
funded €99 product on the identical panel.

The good news is that the crowd is all standing in the same place. **Every
one of these tools measures what you are *spending*. Not one measures what
agent-driven work is *costing* you.**

---

## The reframe

VibePulse today is a **status mirror**: it shows quota, it shows agent state.
Both real, both useful, both available elsewhere. The value it adds is
*placement*, not information — and placement alone is what the most-cited
Tidbyt review calls *"a fun desk accessory in need of a purpose"*.

A device earns its shelf space when it shows something **you would never open
a dashboard to see**. That's the test, and it's a sharp one:

- *Quota* fails it. You'd check quota anyway; the menu bar already tells you.
- *"How long have my agents been waiting on me today"* passes it hard.
  Nobody would open a dashboard for that, and it's exactly what you can't see
  from where you're sitting.

So the thesis: **VibePulse should become the only device that shows what
agent-driven work is costing you** — your waiting time, your unreviewed
backlog, your fragmentation, your hours. That is defensible against Token
Monitor and against Anthropic's own BLE buddy, because neither can follow it
there without becoming a different product.

Three verbs organise everything below.

| | |
|---|---|
| **Act** | The screen becomes an input device. You answer from the kitchen. |
| **Remember** | The terminal shows *now*, per window. The screen shows *across all sessions, across days*. |
| **Protect** | Warn before the wall, catch the agent flailing, give you a physical stop. |

---

## Can the screen answer back? Yes — and it's a weekend, not a research project

This was the strongest idea in the brief, and the answer is better than
expected. **Do not build terminal keystroke injection.** There is a
supported, structured path.

### The mechanism

Both Claude Code **and** Codex ship a **`PermissionRequest` hook** that fires
exactly when the agent is about to ask you, **blocks** for up to 600 seconds
by default, and takes a structured verdict.

Claude Code's `http` hook type means **the tokenserver can receive the
request and return the verdict with no shell script at all**:

```json
{
  "hooks": {
    "PermissionRequest": [{
      "matcher": ".*",
      "hooks": [{
        "type": "http",
        "url": "http://127.0.0.1:8737/api/approval",
        "timeout": 120,
        "statusMessage": "Waiting for VibePulse…",
        "headers": { "Authorization": "Bearer $VIBEPULSE_TOKEN" },
        "allowedEnvVars": ["VIBEPULSE_TOKEN"]
      }]
    }]
  }
}
```

Claude Code POSTs the event and parses the **response body**. The server
holds the connection open until the device taps. `tokenserver.py:1604`
already uses `ThreadingHTTPServer`, so concurrent held requests work today
without a rewrite. As a bonus the TUI spinner literally reads
"Waiting for VibePulse…" while you walk to the kitchen.

**What you receive** — everything needed to render a decision:

```json
{
  "session_id": "abc123",
  "cwd": "/Users/…/Torget",
  "permission_mode": "default",
  "hook_event_name": "PermissionRequest",
  "tool_name": "Bash",
  "tool_input": { "command": "rm -rf node_modules",
                  "description": "Remove node_modules directory" },
  "permission_suggestions": [ … ]
}
```

`permission_suggestions` carries the literal "always allow" options the TUI
would have offered — which is how a "YES, AND DON'T ASK AGAIN" button
behaves identically to picking it in the terminal.

**What you return** — note this carefully, because a summary of the docs
gets it wrong. `decision` is an **object** discriminated on `behavior`, not a
string, and there is no `escalate` value (verified against the shipped
v2.1.231 binary schema):

```json
{"hookSpecificOutput":{"hookEventName":"PermissionRequest",
 "decision":{"behavior":"allow"}}}
```
```json
{"hookSpecificOutput":{"hookEventName":"PermissionRequest",
 "decision":{"behavior":"deny","message":"Denied from VibePulse"}}}
```

Emitting **no** `decision` (exit 0, empty stdout) leaves the flow unchanged
and the normal terminal prompt renders.

### Three properties that make this correct rather than merely possible

**1. Timeout is fail-safe.** A `PermissionRequest` hook that hits its timeout
is cancelled, its output discarded, and **no decision is rendered** — so the
normal interactive prompt appears and the human decides with full context.
Nothing is ever approved by silence. (`PreToolUse` does *not* have this
property; the docs warn explicitly that a timed-out `PreToolUse` hook does
**not** block the tool call. This is the main reason to prefer
`PermissionRequest`.)

**2. The held connection *is* the pending approval.** This dissolves the
staleness race structurally rather than probabilistically. Neither provider
puts a stable id in the payload — `PermissionRequest` notably has **no
`tool_use_id`** — so the server mints `request_id = uuid4()` on receipt and
parks the connection. The device echoes `request_id` back with its tap.
Present → resolve that exact connection and delete it. Absent → reject the
tap, because it was already answered, timed out, or superseded. **There is no
code path where a tap lands on a different prompt than the one it named.**
Idempotency on flaky WiFi comes free from delete-on-resolve.

**3. There is never a double-offer.** `PermissionRequest` fires *before* the
TUI prompt renders. While the device holds the decision the terminal shows
the spinner, not a half-answered prompt. This is precisely the property
`tmux send-keys` can never have.

### Why not the alternatives

- **`Notification` hook** — cannot block by design, and its
  `permission_prompt` type fires only after ~6 seconds without terminal
  input, with each keystroke deferring it. Good for *"finished, come look"*
  (`idle_prompt`), wrong for answering.
- **`tmux send-keys` / `osascript`** — read-then-act is not atomic, so
  between `capture-pane` and the keystroke the prompt can be answered,
  cancelled, or **replaced by a different one**; `2` might mean "No" on the
  prompt you saw and "Yes, always" on the one that replaced it. It also means
  regex-scraping a TUI whose option ordering is not an API. `osascript` adds
  two separate macOS TCC grants and steals window focus. Close this door.
- **Agent SDK `canUseTool`** — the most capable API (unbounded `await`), the
  worst product fit: it replaces the interactive TUI the user wants to keep.
- **`--permission-prompt-tool`** — non-interactive mode only. Dead end here.

### Safety: the kitchen problem

A physical YES button in a kitchen is a security surface, and the easy threat
is a passing housemate. The harder threat is **you**, tapping YES on a
truncated command while holding a coffee.

**The single strongest mitigation is free and does not live in your code.**
Deny and ask rules are still evaluated, so *a hook returning `allow` cannot
override a matching deny rule.* Putting `Bash(rm -rf *)`, `Bash(sudo *)`,
`Bash(curl * | sh)`, `Write(**/.env)` and `Bash(git push --force *)` in
`permissions.deny` means the device **physically cannot** approve them —
regardless of a bug in the service, a compromised screen, or a hostile LAN
peer. Design so the worst-case authority is bounded by a file, not by your
own code being correct.

Then, in order of value:

1. **Allowlist what's even routable** via the hook's `if` field, so only
   low-risk classes reach the device and everything else falls through to
   the terminal. Treat this as noise reduction, **not** the boundary — the
   docs note `if` is best-effort and *fails open* on unparseable Bash.
2. **Asymmetric buttons.** DENY for anything; APPROVE only for allowlisted
   tools. A deny-only device is still enormously useful and has near-zero
   blast radius.
3. **Show the whole command or don't offer YES.** If it doesn't fit at
   480×480, render it truncated and **disable APPROVE**. Never let anyone
   approve text they cannot see. This is the honesty invariant applied to a
   button, and it's the mitigation most likely to be skipped.
4. **Authenticate the device.** The hook's `Authorization` header
   authenticates *Claude → server*. You separately need *device → server*
   auth: a shared secret in `secrets.h`, HMAC over `(request_id, verdict)`,
   and **bind to the LAN interface instead of `0.0.0.0`**.
5. **Freshness** — reject taps older than ~90 s, and show the age.
6. **Log every verdict** (request id, tool, full command, timestamp). When
   something goes wrong you need to answer "did the shelf do that?"

> ### The blast-radius change, stated plainly
>
> Today the server is `ThreadingHTTPServer` on **`0.0.0.0:8737` with no
> authentication**, and one `do_GET` at `tokenserver.py:1494`. That is
> defensible: the worst case is a LAN neighbour learning your quota
> percentages. **The moment a POST can approve a tool call, the worst case
> becomes a LAN neighbour approving `rm -rf` in your agent.** Auth is not a
> polish item here, it ships with the feature or the feature doesn't ship.

### The privacy tension — decide it deliberately

The current contract is hard: *no prompts, no commands, no message text ever
leave the Mac.* Showing "allow `rm -rf build/`?" **breaks that**, because the
command is exactly what you need in order to decide.

Not a blocker, but it is a **deliberate opt-in widening**, and it must be a
separate switch from the display features — off by default, documented in the
README privacy section, scoped to approval payloads only, and never a side
effect of turning on hooks.

### Shipping order

**v1 (a weekend).** Claude Code only. One `PermissionRequest` HTTP hook, one
new endpoint on the existing threading server, long-poll for pending, verdict
POST with HMAC, three buttons: **APPROVE / DENY / LEAVE IT** — where LEAVE IT
returns no decision immediately and punts to the terminal. Timeout 120 s, not
600: long enough to reach the kitchen, short enough that a forgotten prompt
returns to the terminal while you still remember asking. `agent_status.py`
already distinguishes `waiting_approval` from `waiting_input`, so the state
model largely exists.

**v1.1.** `Notification` with matcher `idle_prompt` for the "finished, no
question" case — this also patches a real gap, see the fixes below.

**v2.** Codex `PermissionRequest`. Same endpoint, near-identical wire format
— but budget for its **startup hook-trust review** (first run prompts the
user to approve the hook, so `docs/agent-setup.md` needs a step) and for the
fact that `updatedInput`, `updatedPermissions` and `interrupt` are reserved
and **fail closed** on Codex. Then the "YES, ALWAYS" button via
`permission_suggestions` echo-back.

**v3 (verify first).** Tapping a numbered option — the thing you actually
described. `AskUserQuestion` is answerable via **`PreToolUse`** (not
`PermissionRequest`) by echoing the `questions` array back with an `answers`
map:

```json
{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow",
 "updatedInput":{"questions":[…],"answers":{"Which framework?":"React"}}}}
```

`"allow"` alone is not sufficient for this tool. ⚠️ This is documented in the
context of `-p` mode; whether an interactive session honours it and skips the
picker needs a **10-minute empirical test** before any UI is designed for it.

**Never:** terminal injection.

### One feature to build before all of them

**The panic stop.** Long-press KEY3 → deny everything pending, and hold a
deny-all flag until cleared.

It is the cheapest item in this document and the best-shaped. It only ever
*denies*, so it needs no allowlist, no trust model, and **no privacy
widening** — denying reveals nothing. It cannot be abused: the worst a
stranger can do is stop your work. It uses KEY3, which is firmware-enabled
and currently does nothing but switch apps. And it makes the two-way plumbing
real without asking anyone to trust a kitchen button with `rm -rf`.

A physical kill switch for your agents is also, bluntly, a better story than
another approve button.

---

## Shortlist — the features that pass all three tests

### 1. The human-latency meter *(remember)* ⭐

**"Your agents waited 34 minutes on you today."**

This is the strongest idea in the report, and the reason is that **nobody
measures it — not one tool, on any surface.** Every agent-latency dashboard
in existence measures the machine: time-to-first-token, tool call time,
retries. Human approval time appears only as "a source of deadlock in the
critical path." The telescope has never been turned around.

It is also well-evidenced as *the* real cost. METR's RCT found experienced
developers were **19% slower** with AI tools, and — the detail that matters —
*they could not report time-on-task because they kept switching to unrelated
work while waiting for the agent.* The waiting-and-switching behaviour broke
the research instrument. DORA 2025 names the same thing as new waste: time
lost to AI latency, context re-explanation, tool hopping.

**VibePulse is uniquely positioned:** it already generates the NEEDS YOU
event and already knows when it's dismissed. The measurement is two
timestamps it already has. No new data source, no OTEL, no git parsing.

Show "blocked right now: 4m 12s" and a daily total. It's also
**Goodhart-safe by construction** — a number you can only game *downward*,
unlike every token metric in the ecosystem.

**Framing matters and is easy to get wrong.** "34 MIN RECOVERABLE" is a
companion. "YOU WASTED 34 MIN" is a device you unplug. Tie the number to what
it argues for — the screen itself — not to guilt.

### 2. Review debt *(protect)*

**Unverified agent output piling up.** DORA, METR and eBay's ReviewDebt
framework all name this as *the* bottleneck of agent-driven work: code
production is now exponential, human review capacity is linear, and the delta
is unverified risk. **There is no ambient display of it anywhere.**

Locally computable and content-free: uncommitted diff lines in agent-touched
repos, unpushed commits, age of the oldest, and AI-authored share via
`Co-Authored-By:` trailer counts. Line counts and ratios only — no filenames,
no content.

Display as a **fill level, not a number** — a tank that darkens as unverified
lines accumulate, plus "oldest: 3 days". This is the calm-tech engine-noise
pattern exactly: silent when normal, impossible to ignore when it changes.

### 3. Go / no-go instead of a forecast *(protect)*

The burn-rate page answers *"what will my usage be at reset?"* The question
you actually have, standing in the kitchen with a big refactor in mind, is
*"can I start this now?"*

Same data, same page, no new row — a **decision** instead of a number:

```
SAFE TO START SOMETHING BIG
SHORT TASKS ONLY
WAIT — 2 H TO RESET
```

This is the clearest example of the mirror/companion difference, it costs no
new data source, and it **removes** a number rather than adding one — the
only kind of change `ui-spec.md` welcomes. Being a reframe of an existing
page, it also dodges the pager-dot and test churn a seventh page costs.

Must degrade to a dash when the forecast is `collecting` or `unavailable`. A
go/no-go that guesses is worse than a percentage.

### 4. "Is it flailing?" — the thrash detector *(protect)*

The device knows *blocked* and *working*. It does not know **stuck**. An
agent re-running a failing edit for twenty minutes looks identical to one
making progress — it is `working` the whole time, nothing alerts you, and the
meter keeps running. Arguably a worse failure than being blocked.

The substrate exists and is unused: `tool_result.is_error` in the transcript
(2 failures measured in one 10-minute session), plus `api_error.attempt > 1`
clusters and `tool_decision` rejects / `user_abort` in OTEL. Every Grafana
dashboard graphs these as ops telemetry. **Nobody has framed them as advice.**

Must be conservative — one failed grep is normal. Bar it at N consecutive
failures of the same tool on the same target, tuned high, and label it
`LOOPING?` — a question, because the classifier genuinely cannot know.

### 5. Fragmentation / WIP *(protect)*

AgentDeck shows *which* agents are running. Nobody shows **whether that's too
many.** WIP-limit evidence is strong (reported +40% throughput, −60% delivery
time; sweet spot around ⅔–¾ of capacity) and the mapping is exact: one
developer running four parallel agents is running 4 WIP against a review
capacity of 1.

In-flight count against a self-set limit, plus daily distinct-project
switches. "You touched 6 projects today" is more actionable than any token
figure — and the count is already on the wire as `2 CHATS ACTIVE`.

---

## Reframe rather than extend: Max Tracker

Max Tracker is the one page nobody else has. It is also the page most exposed
to the vanity-metric critique, and this needs saying plainly:

**"Days you maxed out" is a tokenmaxxing metric wearing a heatmap. It rewards
burn.** In April 2026 a Meta engineer built an internal token leaderboard;
engineers competed for "Token Legend" status; Meta killed it after backlash.
The consensus verdict was that *token usage is the lines-of-code metric of
the AI era — easy to measure, easy to game, disconnected from productivity.*
A red cell currently means "good job hitting the ceiling."

**Keep the grid** — it is genuinely good and glanceable in a way a table
isn't. **Change what a cell means** to an outcome you'd be happy to be judged
on: days ending with zero unreviewed agent diff, days finished before 19:00,
days with median agent-wait under two minutes. Same pixels, same streak
mechanic, opposite incentive.

**And add grace days.** Streaks reliably backfire at the moment they break —
the abstinence-violation effect makes people quit rather than resume.
Duolingo's own data showed that reducing loss-anxiety *increased* long-term
engagement. One streak is a habit tracker; five is a slot machine.

---

## Backlog — real, but not next

- **Trust rate, not volume.** `claude_code.code_edit_tool.decision` gives
  accept vs reject. "You rejected 40% of edits today" runs *opposite* to
  tokenmaxxing — high burn with a high reject rate is the bad day the burn
  chart calls good. Anthropic exposes accept rate only to Team/Enterprise
  admins; individuals cannot see their own.
- **The stop cue.** `claude_code.active_time.total` plus commit-hour drift.
  Late-night commits are both an evidence-backed burnout biomarker and an
  evidence-backed bug source (midnight–04:00 commits are measurably buggier).
  A fixed-cue state change is the highest-evidence intervention available to
  a passive display, and it's the one thing a menu bar structurally cannot do
  — you're not looking at your Mac when you should stop.
- **Cross-session context pressure.** Everyone shows context for the
  *focused* session. With four agents running, nobody shows which is about to
  compact.
- **Per-branch attribution.** `gitBranch` is on 126/126 records, free and
  unused — but per-project breakdown is the one thing existing usage tools do
  well, so it needs a redundancy check before it earns space.
- **Tool-time mix.** Derivable now: 36 durations measured in one session
  (median 0.3 s, p90 2.0 s, max 182 s) by pairing `tool_use` → `tool_result`
  timestamps. Note `toolUseResult.durationMs` is **not** a reliable source
  (present on 1 of 142 records) — the timestamps are. Closer to a mirror than
  the shortlist.
- **Subagent fan-out.** `isSidechain` / `agentId` ignored; subagent tokens
  counted but never attributed.

---

## Deliberately not building

- **More tokens, cost, or quota windows.** `ccusage` owns tokens and cost
  across 16 agents and it is not close. Menu-bar apps and
  `coding_agent_usage_tracker` already do 5+ providers' quota. Adding more is
  a treadmill, not a differentiator.
- **Better burn-rate prediction.** `claude-monitor` has done this in a rich
  TUI since 2025. Confidence intervals are invisible ROI.
- **Context-window % for the focused session.** Statusline scripts,
  `/context`, VS Code extensions and an official Anthropic feature request
  all converge here.
- **Lines-of-code or accept-rate as productivity.** Anthropic's own dashboard
  does it, and it's the metric DORA warns about most.
- **A Grafana panel on a small screen.** Six Prometheus panels at 480×480
  from two metres is unreadable. One idea per frame.
- **More streaks and counters.** Max Tracker already owns streaks; see the
  reframe above.
- **A clock, date, weather, or generic info row.** Explicitly a rule
  violation per `ui-spec.md`.
- **Winning a feature race on generic approve/reject.** Anthropic ships it
  officially over BLE and AgentDeck does it across 26 surfaces with encoders
  and mode cycling. Build the approve path because it's cheap and because you
  want it — but position the *cost-of-work* metrics as the differentiator,
  not this. (Hardware note: the board's `radio.bluetooth-le` is
  `board_wired: yes` but **`firmware_enabled: no`**, so matching the
  first-party BLE route would also mean a BLE/WiFi coexistence budget —
  `ble-provisioning` in `spec/hardware-opportunities.md`.)
- **Lovable.** No local surface exists — the agent runs in the cloud, credits
  are dashboard-only, there is no usage endpoint and no lifecycle webhook.
  It's a feature request to Lovable, not an integration. Worth saying plainly
  in the README FAQ rather than leaving it an open maybe.
- **Sound and haptics, for now.** A soft end-of-day tone is a genuinely good
  idea for a kitchen device, and `completion-audio` is already a registry
  candidate. But the registry is explicit: *"no independent buzzer or
  vibration motor is documented; haptics require external hardware."* The
  speaker is `unit_verified: unknown` and `device-units.yaml` records
  `speaker: unknown`. Only **two** capabilities on this board are
  `unit_verified: yes` — the panel and 2.4 GHz WiFi. Gated until someone
  confirms a speaker is physically attached.
- **OpenTelemetry as the unifying integration.** Tempting and wrong for the
  *alert*: every `gen_ai.*` convention is still "Development", they moved to
  a separate repo (semconv v1.42.0 deprecated them, v1.43.0 ships none), and
  decisively **there is no convention for "agent is blocked awaiting human
  input"** — the one signal this product is built on. Claude Code's *own*
  OTel export is a different matter and is genuinely useful (below), but it
  emits **no quota or rate-limit state at all**, so it complements the
  tokenserver rather than replacing it.

---

## More providers

### The architecture finding

Claude Code, Codex, Cursor and Gemini CLI have **independently converged on
the same hook contract**: JSON config keyed by event name,
`{"type":"command","command":"…"}` handlers, a JSON blob on stdin carrying
`session_id` / `transcript_path` / `cwd` / `hook_event_name`, and a
`hookSpecificOutput` return. Codex's event enum is close to a copy of Claude
Code's.

**So: one hook receiver plus a per-provider event-name mapping table — not
four integrations.** Activity and blocked-on-input come nearly free per
provider after the first.

**Quota does not converge**, and for some providers it doesn't exist locally
at all. Model quota as a capability that can be *absent* — the dash
convention already handles that correctly, so a provider without quota
degrades honestly instead of showing a blank gauge.

| Provider | Activity | Blocked | Quota | Call |
|---|---|---|---|---|
| **Codex** | hooks | `PermissionRequest` (blocking!) | rollout JSONL | **Do first** — near-free reuse |
| **Gemini CLI** | hooks + local OTel file | `Notification` / `ToolPermission` (advisory) | ⚠️ none | **Best new provider** |
| **Cursor** | hooks (1.7+) | partial only | ✗ none locally | Activity-only, say so in the UI |
| **OpenCode** | event bus | `permission.asked` | local server | Community contribution |
| **Lovable** | ✗ | ✗ | ✗ | Not possible |

**Gemini specifics.** The only non-Anthropic tool scoring on all three axes.
Telemetry writes straight to a **local file** — no collector:

```json
{ "telemetry": { "enabled": true, "target": "local",
                 "outfile": ".gemini/telemetry.log" } }
```

Two traps. **`logPrompts` defaults to `true`** — it must be explicitly
disabled or the integration writes user prompts to disk, violating this
project's privacy contract on day one. And **there is no programmatic
remaining-quota surface**: the honest options are counting requests against a
known daily cap (free 1000/day, AI Pro 1500, Ultra 2000, API-key free 250) or
showing a dash. Those caps have changed repeatedly, so they belong in config,
not constants — and a *derived* percentage from a *configured* cap deserves a
hard look from the honesty invariant before it reaches the glass.

### The seams, concretely

- One real abstraction: `AgentStatusService._sources()` returns
  `(provider, root, glob, classifier)` tuples.
- Blockers are literal two-key dicts at `agent_status.py:350`, `357-358`,
  `917`, `1101`, plus `if provider != "codex"` at `:1063` and `MODEL_LABELS`.
- ⚠️ **`quota_cache.py:16` and `usage_history.py:23` carry
  `_PROVIDERS = {"claude","codex"}` allowlists that drop unknown providers
  *silently*.** A third provider will appear to work while quietly recording
  no history and no forecast. This is the trap most likely to cost an
  evening.
- `max_tracker.backfill_step` has two hardcoded handler calls, but
  `_advance_one_file` beneath it is already generic — making the caller
  table-driven is the clean move.
- Device side is friendlier than expected: **`tokens_parse.c` tolerates
  unknown keys** (only *duplicate known* keys reject), so additive JSON is
  safe against already-flashed screens. `agent_status_parse.c:538-541` and
  `max_tracker_parse.c:189-192` do carry literal provider arrays.

---

## Fix first: three things already wrong

Not features — credibility. Two touch the honesty invariant directly. All
three were measured in this session.

### 1. `effort` never populates for Claude — one line

`agent_status.py:173` reads `message.get("effort")`. On Claude Code 2.1.231
the field is at the **record top level**. Measured on a live transcript:

```
assistant records:      66
effort at TOP level:    66
effort inside message:   0
```

`/api/agent-status` therefore serves `effort: null` for every Claude job.
Read the top level, fall back to nested so both layouts work.

### 2. `dayTokens` is Claude-only but doesn't say so

`_compute()` (`tokenserver.py:199`) walks only `projects_dir`. Codex volume
never enters `dayTokens`, `monthTokens`, `daySessions` or
`dayTokensPerHour` — though Codex's `token_count` events carry it in
`payload.info`, and `codex_rollout.py` already opens exactly those lines and
discards `info`. A generic name over a provider-specific number is the
honesty invariant's own third clause. Adding Codex volume is the better fix
and the parser is already at the right line.

### 3. The quota probe uses an endpoint that fights back

`api.anthropic.com/api/oauth/usage` is undocumented and rate-limits polling
hard — the direct cause of the 429 penalty that all of v0.2.1's fixes worked
around. Claude Code's **statusLine** receives quota on stdin with **no
network call at all**:

```json
"rate_limits": {
  "five_hour": { "used_percentage": 23.5, "resets_at": 1738425600 },
  "seven_day": { "used_percentage": 41.2, "resets_at": 1738857600 }
}
```

Pro/Max only, only after the session's first API response, each window
independently absent — which the dash convention already handles. This
doesn't just remove a failure mode, it removes the *reason* for the backoff
ladder, the keychain fallback chain and the token-source rotation. **It
deletes the most complex, most failure-prone part of the server.**

### Bonus, related to v1.1 above

**`result` records don't exist in real interactive transcripts.** Measured
record types in a live session: `assistant`, `user`, `attachment`,
`queue-operation`, `last-prompt`. No `result`. The `done`/`error` branch in
`classify_claude` is effectively dead outside SDK mode, and "done" can only
ever be inferred from `stop_reason == "end_turn"`. The `Notification` hook
with matcher `idle_prompt` / `agent_completed` is the real fix.

---

## Suggested order

1. **The three fixes** — especially statusLine.
2. **Panic stop.** First two-way feature. Deny-only, so no trust model, no
   allowlist, no privacy widening.
3. **The human-latency meter.** Needs no new data source and it is the thesis
   of the whole reframe.
4. **`PermissionRequest` v1** — APPROVE / DENY / LEAVE IT, with auth and
   deny-rules documented.
5. **Go/no-go**, then **thrash detector**.
6. **Codex hooks.** Same receiver, second provider.
7. **Review debt**, **Max Tracker reframe**, then **Gemini CLI**.

Steps 2–3 are the ones that change what VibePulse *is*. Everything before
them makes the current thing trustworthy; everything after is breadth.

---

## Evidence

Measured here, so it can be re-checked:

- `effort` location — 66/66 assistant records at top level, Claude Code
  2.1.231.
- `dayTokens` scope — `_compute()` globs `projects_dir` only.
- Live-transcript record types — `assistant`, `user`, `attachment`,
  `queue-operation`, `last-prompt`. No `result`.
- `gitBranch` — 126/126 records. `tool_result.is_error` — 2 in ten minutes.
- Tool durations — 36 pairs derived from timestamps; `durationMs` on 1 of
  142 records.
- Server — one `do_GET` at `tokenserver.py:1494`, `ThreadingHTTPServer` on
  `0.0.0.0:8737`, no auth.
- Poll cadences — agent-status 1 Hz, tokens 30 s, max-tracker 5 min.
- Touch — single point; only `LV_EVENT_CLICKED` and `LV_EVENT_LONG_PRESSED`
  bound anywhere.
- Hardware — `display.amoled` and `radio.wifi-24` are the only capabilities
  at `unit_verified: yes`; `radio.bluetooth-le` is wired but not
  firmware-enabled; KEY3 is firmware-enabled and only switches apps.

Hook schemas for Claude Code were verified against the **shipped v2.1.231
binary**, not only the docs — which matters, because the `decision` object
shape differs from how the docs summarise it. Codex claims come from source
at `openai/codex @ 1da59ad2571`. Claims about Cursor, Lovable, OpenCode, Amp
and OpenTelemetry come from search summaries and third-party writeups because
those domains were blocked by this session's egress proxy — **the Cursor
Admin API surface and the Lovable "no usage endpoint" conclusion are the two
most worth confirming by hand** before either is promised to anyone.
