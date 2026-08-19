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

## 5. Auto-rotation — on since 2026-08-19, partially verified

`TORGET_AUTOROTATE 1`. It was off since the port because "every quarter turn
landed wrong". Four things were wrong, and only one of them was a calibration
constant:

| What | Was | Is | How it was found |
|---|---|---|---|
| `BOOT_ROTATION` | implicit 3 (`ROTATE_270`) | **0** | this board sends no MADCTL at boot, so the panel starts in its own zero — the image is upright every day, which the inherited 3 contradicted |
| touch mirror edge | `479` | `TORGET_SCREEN_W/H - 1` | 480−1 from the AMOLED board; every rotated tap landed 240 px off-glass |
| `SG_QUAD_UP` | 1 | **1** (unchanged) | measured, one pose at a time — the port's guess that the IMU sits differently was wrong |
| `SG_QUAD_DIR` | −1 | **+1** | image turned the wrong way; one quarter turn the wrong way reads as 180° |

Measured on `torget-lcd-154-01`, each pose confirmed before the reading:
upright = quadrant 1, 90° CW = 0, 180° = 3, 90° CCW = 2. One clockwise quarter
turn lowers the quadrant by one, three times running.

**The panel gap is the part that bites.** See §11 — rotation goes through the
ESP-IDF driver, never through a raw `0x36` write.

**Verified on the glass so far:** upright (mode 0, gap 0/0) and 90° clockwise
(mode 3, gap 80/0), both inspected by the user and correct — right way up,
full-bleed, no remnant of the previous mode. **Not yet inspected:** 180°
(mode 2, gap 0/80) and 90° counter-clockwise (mode 1, gap 0/0). Their gap
values follow the same rule and are expected to hold, but expected is not
verified. Two thirty-second checks close this out; until then do not claim the
four-position gate as passed.

**Also still owed:** `TOUCH_CW` has not been re-checked since the image
direction flipped. Tap a known corner in each rotated pose. Never change the
image side alone.

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
3. **Auto-rotation** — on since 2026-08-19, but only two of four poses have
   been looked at, and the touch side has not been re-checked since the image
   direction changed. See §5 and §11.
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

## 11. The 80 pixels the driver was never allowed to add (2026-08-19)

The ST7789 on this module has **240 x 320 of GRAM**; the glass shows 240 x 240.
Mirror the axis that falls on the 320 side and the visible window moves by
**80 px**. The ESP-IDF driver handles exactly this — it keeps `x_gap`/`y_gap`
and adds them to every window in `panel_st7789_draw_bitmap()`.

`torget_display_rotation_set()` used to write MADCTL raw:

```c
esp_lcd_panel_io_tx_param(s_panel_io, 0x36, &MADCTL[rotation], 1);
```

which goes **past** the driver. So no gap was ever set — there was not a single
`esp_lcd_panel_set_gap()` call in the repo. LVGL kept drawing to 0..239, the
tiles landed 80 px off, and the part of GRAM nothing overwrote still held the
previous mode's image, which the new MADCTL now scanned out rotated. On the
glass: the old picture standing behind a new one that is not centred. One
fault, two symptoms — and easy to misread as two separate bugs.

The raw write was also a slow fuse: the driver's own `madctl_val` went stale,
so any later `esp_lcd_panel_mirror/swap_xy/invert_color` would have computed
from a wrong value and destroyed the rotation, in a place nobody would connect
to rotation.

Rotation now goes through `esp_lcd_panel_swap_xy()`, `esp_lcd_panel_mirror()`
and `esp_lcd_panel_set_gap()`, with MV/MX/MY and the gap in one table beside
each other. And `apply_rotation()` invalidates `lv_layer_top()` and
`lv_layer_sys()` as well as the active screen — the OTA takeover and boot
screen live on the top layer and would otherwise keep pixels drawn under the
old address mapping.

## Provenance

Fork of [niclasvestlund-YT/vibepulse](https://github.com/niclasvestlund-YT/vibepulse),
MIT, © Niclas Vestlund. Upstream issue #5 ("Hardware ports: other boards and
panel sizes") is open and labelled `help wanted`; the maintainer offers to help
map details for a port. `upstream` remote points there.
