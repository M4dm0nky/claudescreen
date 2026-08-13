# 2026-08-14 — Completion pulse: physical motion review

## Setup

- Build: `claude/repo-audit-polish-xfeobx` @ `293e6b1` (main `45cbc9b` + the
  pulse commit), flashed over USB around 00:15 after a long enumeration
  diagnosis, run from its own wall PSU on the home 2.4 GHz network against
  tokenserver `c5510b5+`.
- Two hardware lessons captured on the way, both now part of the record:
  - The running firmware's TinyUSB stack enumerates as Espressif PID
    `0x8000` **without a serial port** — `ls /dev/cu.usbmodem*` staying
    empty proves nothing about the cable. Download mode requires the
    *silent* BOOT button held through power-on; the button that visibly
    switches apps is KEY3, and holding it does nothing for flashing.
    Empirical identification: with the firmware running, the button that
    does nothing when pressed is BOOT.
  - After esptool's `Hard resetting via RTS pin`, the board reboots on the
    computer port and WiFi fails to join (screen renders, shows NO DATA
    everywhere). A clean power-cycle on the wall PSU restored the full
    pipeline. The wall PSU is the operating mode; the computer port is for
    download mode only.

## Observed on glass

- **End-to-end NEEDS YOU:** a real `AskUserQuestion` in a local Claude Code
  session took over the screen within the poll cycle — Claude accent,
  project name TORGET, `TAP TO DISMISS` responsive. First physically
  verified full-screen attention overlay (previous evidence was
  simulator-matrix only).
- **The pulse ran on the panel:** breathing accent outline and icon ring,
  then rest at full opacity. This is the first motion ever shown by these
  renderers on glass.
- **Negative test:** a tokenserver restart — the exact trigger of the same
  morning's ghost-alert storm — produced no full-screen takeover afterwards.
  The post-boot freshness ceiling (`89f161f`) holds on hardware.
- **Scope of the header confirmed as designed:** only Mac-local sessions
  appear; cloud sessions are invisible to the LAN pipeline and the header
  says so honestly.

## Verdict

- User verdict, recorded verbatim in intent: *"det fungerar bra … får
  tweaka sen"* — a collective approval of the five checkpoints (quiet after
  restart, four breaths, smoothness, black background, instant dismiss),
  with fine-tuning explicitly deferred. The checkpoints were not itemized
  individually; this review records observed-without-objection, not
  instrumented measurement.
- **Motion gate for the completion pulse: PASSED** on that basis. The
  layout-wiring guard's motion exception (one `lv_anim`, pinned shape) may
  merge to main together with this document.
- **Still open, deliberately:** the AMOLED skill's full
  interaction-performance protocol (gesture/redraw instrumentation, 20
  repeats under network stress) was not performed. Any further motion work
  — including tuning this pulse — should start there.
- Copy follow-up queued: the overlay counts agent instances but says
  "CHATS"; renaming to AGENTS is planned as the first OTA-delivered change.
