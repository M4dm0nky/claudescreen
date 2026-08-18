# Port: Waveshare ESP32-S3-Touch-LCD-1.54

Handover for this fork. Upstream targets the **ESP32-S3-Touch-AMOLED-2.16**
(480×480, CO5300 over QSPI, CST9217 touch). This board is a different one:
**240×240, ST7789 over SPI, CST816 touch, GPIO backlight**. Everything below is
what that difference cost and what is still owed.

Written 2026-08-17. Verified on the physical unit unless marked otherwise.

---

## 1. Hardware layer

Waveshare publishes **no BSP component** for this board (checked the Espressif
registry: there are BSPs for amoled_2_06 and the P4 boards, none for
touch_lcd_1_54). So `components/board_lcd_1_54/` stands in, exposing the same
`bsp_*` surface the AMOLED BSP had — `bsp_display_new`, `bsp_touch_new`,
`bsp_display_brightness_init/set`, `bsp_i2c_init/get_handle`, and the
`BSP_LCD_H_RES`/`V_RES` macros. `main/main.c` therefore keeps its shape.

Pins come from the vendor's own ESP-IDF 5.5.1 examples in
[waveshareteam/ESP32-S3-Touch-LCD-1.54](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.54)
(`05_lvgl_example` for display+touch, `02_button_example`, `03_qmi8658_example`).

| Function | Pin |
|---|---|
| Display bus | SPI3_HOST, 40 MHz, mode 0 |
| SCLK / MOSI | GPIO38 / GPIO39 |
| CS / DC / RST | GPIO21 / GPIO45 / GPIO40 |
| Backlight | GPIO46, active high, LEDC PWM |
| I2C (touch + IMU) | port 0, 400 kHz, SCL GPIO41, SDA GPIO42 |
| Touch INT / RST | GPIO48 / GPIO47 |
| Buttons BOOT / PWR / PLUS | GPIO0 / GPIO5 / GPIO4 |

**The button silkscreened KEY is PLUS, GPIO4** — measured on the unit
2026-08-18 by logging all three under a press; BOOT and PWR never moved. It
carries KEY3's three intents (next app, panic, maintenance window) via
`TORGET_KEY_GPIO` in `main/main.c`. Use the `BSP_BUTTON_*` macro, never the
number. See §9.

Two things that are easy to get wrong:

- **The panel ships colour-inverted.** `esp_lcd_panel_invert_color(panel, true)`
  is required, same as the vendor example.
- **RGB565 needs byte swapping.** `esp_lv_adapter` does it automatically when
  the profile interface is `ESP_LV_ADAPTER_PANEL_IF_OTHER`, which `main.c`
  already sets. Do not "fix" this by touching the colour format.

Touch init failure is **non-fatal** (`main.c`, `display_start`). Upstream
`ESP_ERROR_CHECK`ed it, which turned a silent I2C NACK into a boot loop with a
dark screen — that is how the wrong-board diagnosis started. The panel is the
point; touch is a bonus.

## 2. Resolution plumbing

240×240 flows through one chain. Change it in one place, not five:

```
spec/hardware-capabilities.yaml   (capability display.lcd-240, width/height)
  -> tools/vibepulse_studio/design.py   (DISPLAY_CAPABILITY, PERCENT_FONT_PX)
     + design/vibepulse/studio-design.json   (hero geometry tokens)
       -> components/app_tokens/vibepulse_layout.generated.h   (VP_* macros)
```

Regenerate with `python3 tools/vibepulse_studio/design.py --write`.
`--check` fails when the header is stale.

The platform's own screen size lives in `platform/torget_app.h` as
`TORGET_SCREEN_W/H` (used by the simulator and the OTA/boot overlays, which are
platform-level and must not depend on the app's design tokens). `main/main.c`
carries a `_Static_assert` pairing it to `BSP_LCD_H_RES/V_RES`, so a board swap
cannot silently drift from the layout.

The browser Studio (`tools/vibepulse_studio/web/studio.js`) keeps its **own
copy** of the geometry contract (`PERCENT_FONT_PX`,
`PERCENT_RENDERED_LINE_HEIGHT`, the MIN_* steps). It must be updated in step
with `design.py` — `test/test_vibepulse_studio_wiring.py` checks that.

## 3. Text fitting — read this before touching any label

The panel halved in each direction and text started getting cut mid-glyph.
Fixing those one at a time as they were spotted **did not work**: simulator
captures only exercise whatever string the fixture happens to carry, and
`"FABLE · WEEK"` fits where `"WEEKLY · ALL MODELS"` does not.

`tools/text_fit.py` measures instead. It parses the committed LVGL fonts
(`glyph_dsc[].adv_w` is the advance in 1/16 px; `cmaps[]` maps codepoints to
glyph ids) and checks every string against the box the firmware gives it.
Kerning is ignored, so every number is an upper bound.

```
python3 tools/text_fit.py          # table of all 60 elements
python3 test/test_text_fits_panel.py
```

It found **14 overflows and one missing glyph** on a layout that looked fine in
captures. Rules that came out of that:

- A **bounded** string (an enumerated status word, a caption) that overflows is
  a bug — every value it can take is known, so it could have been made to fit.
- A **free-form** string (a model name off the network, a clamp-ceiling amount)
  may ellipsize. `label()` in `usage_screen.c` now uses `LV_LABEL_LONG_DOT`, so
  overflow reads as "…" rather than a half-drawn letter.
- **A missing glyph renders as a hollow rectangle.** `plex_num_84` carries the
  OTA clock and has no `%`; `plex_num_38` likewise. Both shipped a box on the
  glass before the tool caught them. Check the range in
  `platform/fonts/fetch-and-convert.sh` before reusing a font.

When adding or moving a label, add it to `ELEMENTS` in `text_fit.py` with its
worst-case strings. An element that is not in the table is not checked.

## 4. Fonts added

Generated with `lv_font_conv`; recipes are in
`platform/fonts/fetch-and-convert.sh`.

- `plex_hero_84` — the quota hero. A separate font rather than widening
  `plex_num_84`, because that one carries the OTA ring's mm:ss and must stay
  bit-identical.
- `plex_money_56` — the value hero at panel scale.
- `plex_ui_12` — regenerated with `U+2248` (`≈`) for the burn-rate detail line.

**Source fonts:** IBM has moved the TTFs; the URL in the script 404s and GitHub
rate-limits. Use the npm package instead — `lv_font_conv` reads woff:

```
npm pack @ibm/plex-sans && tar xzf ibm-plex-sans-*.tgz
cp package/fonts/complete/woff/IBMPlexSans-{Bold,SemiBold}.woff platform/fonts/src/
```

Verified 2026-08-17: a font regenerated from woff is **bit-identical** to the
committed TTF-generated file apart from the path in the opts comment.

## 5. Deliberately off

**Auto-rotation** (`TORGET_AUTOROTATE 0` in `main/main.c`). The IMU answers on
this board — but `rotation.c`'s constants (`SG_QUAD_UP`, `SG_QUAD_DIR`,
`TOUCH_CW`, and the 0xA0 boot orientation) are calibrated for the AMOLED
board's IMU mounting and for a panel that sits rotated in its case. Neither
holds here, so every quarter turn landed wrong. On the old board the IMU was
unreachable anyway, so off is also what the port inherited.

To enable: run the P24 four-position test, one constant at a time, and pair
each MADCTL mode with its touch flags. Never change one side alone.

## 6. Build, flash, measure

```sh
export PATH="/opt/homebrew/bin:$PATH"      # cmake/ninja
source ~/esp/esp-idf/export.sh             # ESP-IDF 5.5
idf.py build
idf.py -p /dev/cu.usbmodem2101 flash       # confirm the port first
```

Gotchas, all hit at least once:

- **`idf.py monitor` needs a TTY** and fails in non-interactive shells. Read the
  serial directly instead:
  `~/.espressif/python_env/idf5.5_py3.13_env/bin/python` with `pyserial`.
- **Font and source globs are evaluated at configure time.** After adding a
  font, `idf.py reconfigure` (and re-run `cmake` for the simulator) or the link
  fails with an undefined symbol.
- **The simulator downloads LVGL from GitHub**, which rate-limits. Point it at
  the copy the firmware already fetched:
  ```sh
  cmake -S sim -B sim/build -G Ninja \
    -DFETCHCONTENT_SOURCE_DIR_LVGL="$PWD/managed_components/lvgl__lvgl"
  ninja -C sim/build
  SDL_VIDEODRIVER=dummy TORGET_CAPTURE_DIR=/tmp/caps \
    ./sim/build/torget-sim --vibepulse-static-qa
  ```
  Create the capture directory first — it is not created for you.
- Captures are BMP; `sips -s format png` to view them.

Look at the **long** fixtures, not just the pretty ones: `claude-all`,
`needs-you-question-long`, `value-wide`, `tracker-codex-full`. The short ones
are what hid the overflows.

## 7. The Mac is not optional

Asked and settled 2026-08-17, so it does not get re-litigated:

The panel cannot read its data "from the website" and drop the Mac.

- **Agent status** (the whole NEEDS YOU feature) comes from Claude Code hooks
  running on the Mac. No website knows it.
- **Codex usage** is read from Codex's local app-server.
- **Claude quota** needs the OAuth token from the macOS keychain. Putting it on
  the device means a stolen panel leaks account access — `secrets.h` documents
  that this is deliberately avoided.

Moving the service to an always-on Linux host does not work either: it finds no
Claude token there (no keychain; the credentials file is read only on Windows —
upstream issue #2), and the hooks must live where Claude Code runs.

What is left is the sleep window. When the Mac sleeps the panel shows its last
values and marks them stale, which is honest but not live. Mitigation is a Mac
power setting, not firmware.

## 8. Open

1. **Exact-raster landmark suite** — `test/test_vibepulse_visual_landmarks.py`
   (~1000 lines) asserts pixel coordinates against 480×480 captures. 87
   failures (84, plus the three five-hour session captures added 2026-08-18 —
   same single cause, `(240, 240) != (480, 480)`); `./test/run.sh` exits 1
   because of it. Every crop region and lit-
   pixel threshold has to be re-derived for 240×240 and confirmed by eye. This
   is the only thing standing between the gate and green — all 31 other suites
   pass.

   Note for whoever does the re-derivation: `test_pager_shows_one_dot_per_view`
   dies inside `dot_runs()` on the 480-era `PAGER_ROW_Y` before it ever reads
   its `active_index` values, so those numbers are unverified by the suite even
   when they are right. They were corrected by hand for the 2026-08-18
   reordering and checked by measuring the pager row in real 240×240 captures
   (`PAGER_Y + 3`, nine runs, the active one 18 px). Re-run that measurement
   rather than trusting the constants.
2. ~~**Touch alignment**~~ — **checked on the glass 2026-08-18, and it is
   correct.** `swap_xy/mirror_x/mirror_y` all 0 is right for this board. A
   temporary build logged `lv_indev_get_point` on each new press; three taps
   into three corners gave `(1, 12)`, `(239, 10)`, `(13, 237)` for top-left,
   top-right and bottom-left of a 240 x 240 panel. No axis swapped, none
   mirrored. The UPDATE pill was then operated by finger with the console
   attached and answered on the first try (`notisen besvarad med JA —
   fönstret öppnas`). **Do not "fix" these flags** — the day this was
   measured, a dead-feeling takeover had already been blamed on them twice.
   See §10 for what was actually wrong.
3. **Auto-rotation** — see §5.
4. ~~**The capability registry still calls itself the AMOLED board**~~ —
   fixed 2026-08-18. A capability may now declare its own `board:`, and
   `hardware_registry.py` matches a verification unit against *that* rather
   than the file header (which stays the default for every inherited entry).
   `display.lcd-240` and `input.key-lcd-1-54` name this board, the physical
   unit is registered as `torget-lcd-154-01`, and the KEY pin is
   `unit_verified: "yes"` against a real physical-test source. A registry that
   cannot hold a measurement teaches people not to measure.

## 9. The silent button (2026-08-18)

The port carried `main.c` over unchanged, including `gpio_get_level(GPIO_NUM_18)`
— the AMOLED board's KEY3. On this board GPIO18 is not a button, so with an
internal pull-up it reads "released" forever. Every KEY3 intent was dead: app
switching, the Needs You panic, and — the one that hurt — the OTA maintenance
window, which by design can be opened from nowhere else. The panel showed
UPDATE READY, the pusher waited, and neither could reach the other.

What made it invisible: this port's own BSP defines `BSP_BUTTON_BOOT/PWR/PLUS`
and **used them nowhere**, the pin table above listed them correctly, and the
boot log printed `KEY3 rå nivå vid boot: 1` — a pin nobody had connected,
truthfully reporting "released". A dead input has no error path. It just never
fires.

The repair: `TORGET_KEY_GPIO` (= `BSP_BUTTON_PLUS`) in `main/main.c`, the boot
log now prints the pin number with the level, and the on-glass detail during an
open window says `KEY CLOSES` rather than naming a button this board does not
have.

## 10. The takeover that could not be answered (2026-08-18)

The same afternoon, the panel showed the UPDATE READY takeover — UPDATE NOW and
LATER, UPDATE NOW highlighted — and **nothing on the glass responded to a
finger**, while the KEY hold was independently dead (§9). The board looked
hung. It was not.

Two suspects were named and both were wrong, which is the useful part:

- **The touch axes** (§8.2) — measured and correct. See above.
- **The pill's wiring** — `ota_ui.c` creates it `LV_OBJ_FLAG_CLICKABLE`, binds
  `LV_EVENT_CLICKED`, and the NOTICE renderer un-hides it. Read line by line,
  then proven live: with the console attached the pill answered on the first
  press and opened the window.

What is left is the one thing that leaves no trace on the glass: on that boot
the CST816 had not come up. `main.c` treats a touch-init failure as non-fatal
on purpose — the panel is the point, and a board that boots display-only beats
one that panics in a loop — but the only evidence is a single `ESP_LOGE` line
on a console nobody is watching. **A panel with dead touch is visually
identical to one with working touch.** Add the dead button, and the device had
no way at all to say what was wrong.

The lesson is not "make touch fatal" (a flaky touch chip must never roll back a
healthy image). It is that a full-screen takeover offering two buttons is a
promise, and the firmware knows at boot whether it can keep it. Tracked as
OBS-31 in `docs/observability-backlog.md`, whose fix — render the notice
without pills and name the physical way out instead — covers this trigger as
well as the missing-token one it was written for.

## Provenance

Fork of [niclasvestlund-YT/vibepulse](https://github.com/niclasvestlund-YT/vibepulse),
MIT, © Niclas Vestlund. Upstream issue #5 ("Hardware ports: other boards and
panel sizes") is open and labelled `help wanted`; the maintainer offers to help
map details for a port. `upstream` remote points there.
