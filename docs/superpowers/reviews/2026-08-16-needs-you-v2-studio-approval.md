# Needs You v2 — Studio Approval (2026-08-16)

## Outcome

The owner approved the Needs You v2 takeover screens on the **simulator
LVGL captures** at commit 240ec49 ("go with both": raster approval plus the
go-ahead for the wiring pass). This is **Studio approval only**: per the
AMOLED skill it does not authorize a flash, no physical review has
happened, and the flash question must still be asked explicitly when its
moment comes. Touch and KEY3 remain `unit_verified: unknown` in `spec/`.

## What was approved

The six captures in `docs/img/needs-you/` at 240ec49:
`vibepulse-needs-you-{attract,question,approval,private,payoff,none}.png` —
the LVGL rasters of the approved design direction recorded in
`docs/needs-you-investigation.md` ("The approved design direction
(2026-08-16)") and regenerable as concepts via
`tools/mockups/gen_needsyou_v2.py`.

Seven implementation deviations from the concept frames were reviewed and
accepted, all font-raster realities or cosmetics: "ON IT" without the full
stop (protects the shared `plex_headline_48` raster), 14 px eyebrow, 27 px
WAITING headline, 25 px button faces, the two-step mono shrink (40 → 24),
the verbatim-capitalized payoff echo (the design law beat the mockup, as
intended), and the ring gap's clock position.

## The law that binds later changes

From the approved direction, non-negotiable in any future edit: every
touch target ≥ 90 px; APPROVE filled, never outlined; DENY red and present
only where the command is readable; private and payoff buttonless; the
countdown ring maps to the real `hold_ms` fallback time; the payload
verbatim in mono; personality in the chrome only; no text under 14 px; the
payoff echo is the verbatim approved item. Pixel-landmark assertions in
`test/test_vibepulse_visual_landmarks.py` pin the approved geometry.

## Still open after this approval

- On-target `idf.py build` (fonts ~40 KB + mascots ~12 KB flash delta)
- The device verdict POST (screen callback is wired to nothing yet)
- Physical gates, in order: touch + KEY3 verification on the named unit,
  the explicit flash question, then the static physical review — where
  `lv_arc` anti-aliasing and the pixel poses get the real-AMOLED eye
- Payoff auto-return timing (tick-driven, not landmark-testable) untested
