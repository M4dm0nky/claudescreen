# Release: the OTA platform (2026-08-14)

One day, one arc: from a frozen panel at dawn to a screen that updates
itself over the air, asks permission with big buttons on the glass, and
rolls back bad images on its own.

## Highlights

- **The freeze diagnosed and fixed.** A companion app's RAM load plus the
  OTA foundation's boot footprint starved the panel flush's DMA bounce
  buffer (largest free block < 11,520 B ⇒ endless NO_MEM). The companion
  is now explicit opt-in (`-DTORGET_WITH_BUDDY=ON`), mirrored in the
  simulator, pinned by tests.
- **A/B over-the-air updates, proven in ten live deliveries.** A 3-second
  KEY3 hold or a tap on the UPDATE pill opens a ten-minute maintenance
  window; a bearer token, SHA-256 and a metadata gate guard the upload;
  the image lands in the inactive slot; a boot-health gate must approve
  within 15 seconds or the bootloader rolls back. USB-C is never written
  by an OTA and remains the rescue path.
- **The UPDATE READY notice.** The tokenserver announces the newest build
  on the existing quota poll; the screen takes over with the waiting
  version and two large buttons — UPDATE NOW / LATER. Hourly reminders
  until installed, silence when versions match, and never a takeover
  while an update is already running.
- **The OTA ring.** READY (a clockwise-draining egg timer with mm:ss),
  INSTALLING (clockwise fill, percent, the incoming image's own version),
  VERIFYING (SHA-256), RESTARTING. Physically reviewed and refined over
  three rounds.
- **The boot screen.** The wordmark over WIFI → TIME → DATA, each lit by
  its real signal, torn down the moment the first fetch lands. No more
  NO DATA flash at power-on. Physically approved.
- **A bulletproofed tokenserver.** Dead OAuth tokens are never resent; a
  machine-wide probe lock allows at most one upstream prober per Mac; the
  429 penalty persists across restarts; used-today is computed from
  cached cycle timestamps and survives blackouts; scoped weekly pools
  show whenever they hold real usage; 15 calls/hour baseline.
- **Sender gates.** The binary is chosen at the moment of upload, its
  embedded version is read from the app descriptor and always announced,
  and `-dirty` builds are refused without `TG_OTA_ALLOW_DIRTY=1` — the
  lesson from the archived ghost build that froze the glass.
- **Attention alarms breathe for 45 seconds** (4.8 s was missed in
  practice); DONE cards pulse their whole visibility window.
- **Documentation.** `docs/ota.md` (the full loop and consent model), a
  README section, new symptom-table rows in the agent runbook, and
  CLAUDE.md ground rules.

## Verified on the physical device

Ten OTA deliveries (including the first touch-consented and the first
fully chained one), a PENDING_VERIFY boot with the health gate live and
approving at 9.1 s (console-logged), automatic window re-arm after OTA,
the rollback net armed, and both the boot and OTA screens reviewed on
glass.

## Known remaining

- The value page (page seven) is being reworked — not part of this
  release.
- A CI bridge in the pusher (refuse builds without green CI) — ordered,
  next session.
- The ring's breathing pulse awaits its motion review.
- A pusher daemon so the UPDATE tap always has a deliverer.
