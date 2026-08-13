# Max Tracker — Physical Static Review (2026-08-13)

## Outcome

The Torget repository's FIRST physical flash was performed 2026-08-13 with
explicit user authorization, from the merged main state (feature branch
worktree-max-tracker at 207e4ed, merged as e366bd4; binary built at that
tree). The static physical gate for the Max Tracker pages and the shared
platform is PASSED. The Solceller firmware copy is retired as the screen's
driver; this repository now owns the glass.

## Flash evidence

- Flash performed with esptool directly (bypassing idf.py overhead) after
  repeated USB enumeration bouncing; write completed, hash verified, hard
  reset via RTS. Log: scratchpad flash-race output, "Hash of data verified".
- Boot verified over the LAN: the device (192.168.1.135) established
  connections to the tokenserver and fetched /api/tokens, /api/agent-status
  and /api/max-tracker (the latter impossible for the previous firmware).

## Physical inspection evidence

User-captured photographs of the physical AMOLED (IMG_0191/0192, reviewed
at full resolution by the controller session) show:

- CODEX · MAX TRACKER page: indigo heat grid populated with real backfilled
  history, exact-red max-day cells, legend with MAX label, stat row
  STREAK 4 DAYS · MAX WEEKS 0 · AVG PEAK 53 % · MAX DAYS 11, six pager
  dots, real provider icon in the shared live header, NO ACTIVE CHAT state.
- CLAUDE · MAX TRACKER page: MAX 20X plan badge right-aligned in the
  eyebrow row, gray activity backfill plus colored recent quota days,
  1 CHAT ACTIVE live-header state (the building session itself), stat row
  STREAK 4 DAYS · MAX WEEKS 0 · AVG PEAK 37 % · MAX DAYS 0.
- Colors, geometry and typography match the simulator captures the
  landmark suite pins; no tofu, no truncation, no layout breakage observed.

Touch/swipe interaction and the launcher long-press were reported working
by the user during inspection. Brightness and viewing-distance judgments
are from photographs and user report, not instrumented measurement.

## Hardware truth discovered (recorded in README + this review)

A computer USB port could not power the RUNNING firmware: the panel's
current draw made the board bounce off the USB bus repeatedly (both with
the previous firmware and the new one) and eventually hang, with no port
enumeration and no network traffic. On its own USB power supply the same
board and firmware boot and run stably. Consequences:

- Flashing should happen in download mode (panel dark, ROM USB stable);
  the direct-esptool race script in this session's scratchpad worked
  around enumeration bouncing.
- Running the firmware from a computer port is not supported; the README's
  manual-setup section now says so.
- Serial-console debugging of the running firmware requires a powered hub
  or a PSU+data split; not verified in this session.

## Gates

- Static physical gate: PASSED for the current static build (quota pages,
  agent monitor states and Max Tracker pages seen live on glass; NEEDS
  YOU/error overlays verified earlier in the simulator matrix only).
- Motion remains gated: no lv_anim exists in these renderers, and motion
  work (including the swipe-navigation effort in progress on main) must
  follow the interaction-performance protocol in the AMOLED skill,
  measuring the shared display pipeline on the physical panel first.
