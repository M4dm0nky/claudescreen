#!/usr/bin/env python3
"""Measure every on-screen string against the box the firmware gives it.

The panel shrank from 480x480 to 240x240 and text started getting cut mid
glyph. Eyeballing simulator captures only catches the strings the fixtures
happen to carry -- "FABLE · WEEK" fits where "WEEKLY · ALL MODELS" does not.
This measures instead of looking.

The committed LVGL fonts carry their own metrics: glyph_dsc[].adv_w is the
advance width in 1/16 px, and cmaps[] maps codepoints to glyph ids. That is
everything needed to compute a string's rendered width exactly. Kerning is
ignored on purpose -- it only ever pulls glyphs closer, so every number here
is an upper bound, which is the safe direction for an overflow audit.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "platform/fonts"
LAYOUT_HEADER = ROOT / "components/app_tokens/vibepulse_layout.generated.h"
USAGE_SCREEN = ROOT / "components/app_tokens/usage_screen.c"
AGENT_MONITOR = ROOT / "components/app_tokens/agent_monitor.c"


class FontError(Exception):
    pass


# --------------------------------------------------------------- font metrics

_GLYPH_RE = re.compile(r"\.adv_w\s*=\s*(\d+)")
_CMAP_RE = re.compile(
    r"\.range_start\s*=\s*(\d+),\s*\.range_length\s*=\s*(\d+),\s*"
    r"\.glyph_id_start\s*=\s*(\d+),\s*"
    r"\.unicode_list\s*=\s*(\w+),.*?\.list_length\s*=\s*(\d+),\s*"
    r"\.type\s*=\s*LV_FONT_FMT_TXT_CMAP_(\w+)",
    re.S,
)


def _unicode_lists(source: str) -> dict[str, list[int]]:
    lists: dict[str, list[int]] = {}
    for name, body in re.findall(
        r"static const uint16_t (unicode_list_\d+)\[\]\s*=\s*\{(.*?)\};",
        source, re.S,
    ):
        lists[name] = [int(v, 0) for v in re.findall(r"0x[0-9a-fA-F]+|\d+", body)]
    return lists


def load_font(path: Path) -> dict[int, float]:
    """Return {codepoint: advance width in px} for one generated LVGL font."""
    source = path.read_text(encoding="utf-8", errors="replace")

    block = re.search(
        r"glyph_dsc\[\]\s*=\s*\{(.*?)\n\};", source, re.S)
    if not block:
        raise FontError(f"{path.name}: no glyph_dsc[]")
    advances = [int(v) for v in _GLYPH_RE.findall(block.group(1))]

    lists = _unicode_lists(source)
    cmap_block = re.search(r"cmaps\[\]\s*=\s*\{(.*?)\n\};", source, re.S)
    if not cmap_block:
        raise FontError(f"{path.name}: no cmaps[]")

    widths: dict[int, float] = {}
    for start, length, gid_start, list_name, _len, kind in _CMAP_RE.findall(
            cmap_block.group(1)):
        start, length, gid_start = int(start), int(length), int(gid_start)
        if kind == "FORMAT0_TINY":
            # Contiguous: codepoint start+i maps to glyph gid_start+i.
            for i in range(length):
                gid = gid_start + i
                if gid < len(advances):
                    widths[start + i] = advances[gid] / 16.0
        elif kind == "SPARSE_TINY":
            # Sparse: the i-th listed offset maps to glyph gid_start+i.
            for i, offset in enumerate(lists.get(list_name, [])):
                gid = gid_start + i
                if gid < len(advances):
                    widths[start + offset] = advances[gid] / 16.0
        else:
            raise FontError(f"{path.name}: unhandled cmap type {kind}")
    return widths


_FONT_CACHE: dict[str, dict[int, float]] = {}


def font(name: str) -> dict[int, float]:
    if name not in _FONT_CACHE:
        path = FONT_DIR / f"{name}.c"
        if not path.is_file():
            raise FontError(f"no such font: {name}")
        _FONT_CACHE[name] = load_font(path)
    return _FONT_CACHE[name]


class MissingGlyph(Exception):
    def __init__(self, name: str, char: str):
        super().__init__(f"{name} has no glyph for {char!r} (U+{ord(char):04X})")
        self.font_name = name
        self.char = char


def measure(text: str, font_name: str, letter_space: int = 0) -> float:
    """Rendered width in px. Raises MissingGlyph when the raster lacks a char.

    A missing glyph is not a rounding detail: LVGL draws a hollow box, which
    is how the value hero shipped a placeholder rectangle instead of "%".
    """
    metrics = font(font_name)
    total = 0.0
    for char in text:
        code = ord(char)
        if code not in metrics:
            raise MissingGlyph(font_name, char)
        total += metrics[code]
    if len(text) > 1:
        total += letter_space * (len(text) - 1)
    return total


# ------------------------------------------------------------ layout constants

def layout_tokens() -> dict[str, int]:
    tokens: dict[str, int] = {}
    for name, value in re.findall(
            r"#define\s+(VP_\w+)\s+(\d+)",
            LAYOUT_HEADER.read_text(encoding="utf-8")):
        tokens[name] = int(value)
    return tokens


T = layout_tokens()

# Derived box widths. Each mirrors a #define in the C source; ASSERT_DEFINES
# below re-reads that source so the two can never drift apart silently.
TAKEOVER_X = 10
TAKEOVER_W = T["VP_SCREEN_W"] - 2 * TAKEOVER_X
PROVIDER_NAME_W = 92
STAT_COL_W = (T["VP_CONTENT_W"] - 8) // 2
MT_CELL, MT_GAP, MT_WEEKS = 8, 2, 20
MT_GRID_W = MT_WEEKS * (MT_CELL + MT_GAP) - MT_GAP
MT_STAT_COL_W = MT_GRID_W // 4
MT_LEGEND_LABEL_W = 38
CONTEXT_W = T["VP_CONTENT_W"] - 36 - PROVIDER_NAME_W
PS_MARGIN = 12
PS_CONTENT_W = T["VP_SCREEN_W"] - 2 * PS_MARGIN
OTA_MARGIN = 16
OTA_PILL_W = T["VP_SCREEN_W"] - 2 * OTA_MARGIN

ASSERT_DEFINES = [
    (USAGE_SCREEN, r"#define PROVIDER_NAME_W 92"),
    (USAGE_SCREEN, r"#define STAT_COL_W \(\(VP_CONTENT_W - 8\) / 2\)"),
    (USAGE_SCREEN, r"#define MT_STAT_COL_W \(MT_GRID_W / 4\)"),
    (USAGE_SCREEN, r"#define MT_LEGEND_LABEL_W 38"),
    (AGENT_MONITOR, r"#define TAKEOVER_X 10"),
    (AGENT_MONITOR, r"#define TAKEOVER_W \(VP_SCREEN_W - 2 \* TAKEOVER_X\)"),
]


def check_defines() -> list[str]:
    problems = []
    for path, pattern in ASSERT_DEFINES:
        if not re.search(pattern, path.read_text(encoding="utf-8")):
            problems.append(f"{path.name}: '{pattern}' no longer present")
    return problems


# ------------------------------------------------------------------- elements

# Worst-case strings come from the policy/presenter layer. Bounded sets are
# listed in full; free-form text (project names, questions, commands) is
# represented by a realistic long sample and must carry LONG_DOT in the C code.

QUOTA_LABELS = ["WEEKLY", "WEEKLY · ALL MODELS", "FABLE · WEEK", "OPUS 5 · WEEK"]
# usage_live_policy.c emits the model alone (it is what carries information);
# the halo beside the icon already says something is running.
LIVE_CONTEXT = [
    "IDLE", "1 AGENT", "255 AGENTS", "NOW", "GPT-5.6 SOL",
    "NO DATA", "STALE", "LIVE",
]
RESET_SHORT = ["–", "2D 4H", "23H 59M", "59M", "999D 23H"]
DELTA = ["–", "+100%", "+12%"]
PERCENT = ["–", "100%", "73%"]
FORECAST_HEADLINE = [
    "LEARNING PACE", "SPEED UP", "ON PACE", "UNAVAILABLE",
]
FORECAST_DETAIL = [
    "FORECAST NOT READY", "5.2× CURRENT PACE TO MAX OUT",
    "≈ CURRENT PACE TO MAX OUT", "RUNS OUT AT RESET",
    "NO RELIABLE FORECAST", "RUNS OUT SUN 23:59", "2D 4H EARLY",
]
VALUE_VERDICT = [
    "NO DATA", "NO PRICED USAGE THIS MONTH", "UNPRICED",
    "SOME MODELS ARE NOT PRICED", "SET YOUR PLAN COST",
    "YOUR PLAN IS CHEAPER", "THE API WOULD BE CHEAPER",
]
VALUE_ATTRIB = ["CLAUDE $280 · CODEX $32", "CLAUDE $9999999 · CODEX $9999999"]
VALUE_MONEY = ["–", "$9999999", "999.9×", "1.42×"]
COMPLETION_DETAIL = [
    "CLAUDE IS WAITING", "CODEX NEEDS ATTENTION", "CLAUDE FINISHED",
    "255 AGENTS NEED ATTENTION", "255 AGENTS FINISHED",
]
COMPLETION_TITLE = ["DONE", "NEEDS YOU", "ERROR"]
MT_CAPTIONS = ["STREAK", "WEEKS", "PEAK", "DAYS"]
MT_VALUES = ["–", "999"]
MT_UNITS = ["", "DAYS", "%"]
OTA_WORDS = ["UPDATE READY", "INSTALLING", "VERIFYING", "RESTARTING", "READY"]
OTA_DETAILS = ["KEY3 CLOSES", "SHA-256", ""]
NY_FOOTER = ["1 MORE OPTION IN TERMINAL", "255 MORE OPTIONS IN TERMINAL"]
PROJECT = ["VIBEPULSE", "SOME-LONG-PROJECT"]  # TK_AGENT_PROJECT_CAP = 17

# name, font, box width, letter_space, strings, ellipsizes
#
# `ellipsizes` = the string is genuinely unbounded (a model name off the
# network, a clamp-ceiling amount) AND its label uses LV_LABEL_LONG_DOT, so
# overflow degrades to "…" instead of a half glyph. A BOUNDED string that
# overflows is still a failure even with DOT: we know every value it can take,
# so it could have been made to fit.
ELEMENTS = [
    # --- quota page -------------------------------------------------------
    ("quota.provider_name", "plex_ui_21", PROVIDER_NAME_W, 2, ["CLAUDE", "CODEX"], False),
    ("quota.live_context", "plex_ui_12", CONTEXT_W, 1, LIVE_CONTEXT, True),
    ("quota.label", "plex_ui_16", T["VP_CONTENT_W"], 2, QUOTA_LABELS, False),
    ("quota.percent", "plex_hero_84", T["VP_SCREEN_W"] - 16, -4, PERCENT, False),
    ("quota.stat_value.today", "plex_ui_21", STAT_COL_W, 0, DELTA, False),
    ("quota.stat_value.reset", "plex_ui_21", STAT_COL_W, 0, RESET_SHORT, False),
    ("quota.stat_caption", "plex_ui_12", STAT_COL_W, 1, ["USED TODAY", "TO RESET"], False),

    # --- analytics header (value / burn / tracker) ------------------------
    ("analytics.heading", "plex_ui_16", 112, 2, ["VALUE", "BURN RATE"], False),
    ("analytics.top_right", "plex_ui_12", T["VP_CONTENT_W"] - 112, 0,
     ["MONTH TO DATE", "WEEKLY"], False),
    ("analytics.bottom_right", "plex_ui_12", T["VP_CONTENT_W"], 2,
     ["AT LIST API PRICES", "FORECAST"], False),

    # --- burn rate --------------------------------------------------------
    ("burn.label", "plex_ui_12", T["VP_CONTENT_W"], 2,
     ["CLAUDE · ALL MODELS", "CODEX · WEEKLY"], False),
    ("burn.headline", "plex_attention_25", T["VP_CONTENT_W"], -1, FORECAST_HEADLINE, False),
    ("burn.detail", "plex_ui_12", T["VP_CONTENT_W"], 0, FORECAST_DETAIL, False),

    # --- value page -------------------------------------------------------
    ("value.verdict", "plex_ui_12", T["VP_CONTENT_W"], 0, VALUE_VERDICT, False),
    ("value.hero", "plex_money_35", T["VP_CONTENT_W"], -2, VALUE_MONEY, False),
    ("value.attribution", "plex_ui_12", T["VP_CONTENT_W"], 0, VALUE_ATTRIB, True),
    ("value.stat", "plex_money_35", STAT_COL_W, 0, ["$9999999"], True),
    ("value.caption", "plex_ui_12", T["VP_CONTENT_W"] // 3, 1,
     ["VIA API", "BREAK", "YOU PAID"], False),

    # --- max tracker ------------------------------------------------------
    ("tracker.eyebrow", "plex_ui_16", 145, 1, ["MAX TRACKER"], False),
    ("tracker.plan_badge", "plex_ui_12", T["VP_CONTENT_W"] - 145, 0,
     ["PRO", "MAX 20X"], False),
    ("tracker.legend_max", "plex_ui_12", MT_LEGEND_LABEL_W, 0, ["MAX"], False),
    ("tracker.stat_caption", "plex_ui_12", MT_STAT_COL_W - 1, 0, MT_CAPTIONS, False),
    ("tracker.stat_value", "plex_ui_21", MT_STAT_COL_W, 0, MT_VALUES, False),
    ("tracker.stat_unit", "plex_ui_12", MT_STAT_COL_W, 0, MT_UNITS, False),

    # --- completion takeover ---------------------------------------------
    ("done.provider", "plex_ui_12", TAKEOVER_W, 3, ["CLAUDE", "CODEX"], False),
    ("done.title", "plex_attention_25", TAKEOVER_W, 0, COMPLETION_TITLE, False),
    ("done.project", "plex_ui_16", TAKEOVER_W, 2, PROJECT, False),
    ("done.detail", "plex_ui_12", TAKEOVER_W, 1, COMPLETION_DETAIL, False),
    ("done.dismiss", "plex_ui_12", TAKEOVER_W, 2, ["TAP TO DISMISS"], False),

    # --- needs you --------------------------------------------------------
    ("ny.attract_word", "plex_attention_25", T["VP_SCREEN_W"], 0, ["NEEDS YOU"], False),
    ("ny.attract_project", "plex_ui_16", T["VP_SCREEN_W"], 2, PROJECT, False),
    ("ny.attract_tap", "plex_ui_12", T["VP_SCREEN_W"], 2, ["TAP TO ANSWER"], False),
    ("ny.card_title", "plex_ui_16", TAKEOVER_W - 16, 0, ["New auth layer"], True),
    ("ny.card_sub", "plex_ui_12", TAKEOVER_W - 16, 0, ["Cleaner architecture"], True),
    ("ny.footer", "plex_ui_12", T["VP_SCREEN_W"], 0, NY_FOOTER, False),
    ("ny.private_title", "plex_ui_16", T["VP_SCREEN_W"], 1, ["SOMETHING IS WAITING"], False),
    ("ny.private_sub", "plex_ui_12", T["VP_SCREEN_W"], 0, ["Details stay on the Mac"], False),
    ("ny.private_tap", "plex_ui_12", T["VP_SCREEN_W"], 2,
     ["TAP TO ANSWER AT YOUR DESK"], False),
    ("ny.payoff_word", "plex_headline_48", T["VP_SCREEN_W"], 0, ["ON IT"], False),
    ("ny.payoff_echo", "plex_ui_12", T["VP_SCREEN_W"], 0, ["Ship it now"], True),
    ("ny.button", "plex_ui_16", (TAKEOVER_W - 4) // 2, 0,
     ["APPROVE", "DENY", "LEAVE IT"], False),

    # --- github page ------------------------------------------------------
    ("github.heading", "plex_ui_16", 80, 2, ["GITHUB"], False),
    ("github.project", "plex_ui_12", T["VP_CONTENT_W"] - 80, 0,
     ["claudescreen", "a-rather-long-project-name"], True),
    ("github.provenance", "plex_ui_12", T["VP_CONTENT_W"], 2,
     ["LIVE", "CACHED", "WAITING"], False),
    ("github.stars_label", "plex_ui_12", T["VP_CONTENT_W"], 3, ["STARS"], False),
    ("github.stars", "plex_hero_84", T["VP_SCREEN_W"] - 16, -4, ["9999"], False),
    ("github.forks_label", "plex_ui_12", T["VP_CONTENT_W"], 2, ["FORKS"], False),
    ("github.forks", "plex_ui_21", T["VP_CONTENT_W"], 0, ["999999", "1.2M"], False),

    # --- star popup -------------------------------------------------------
    ("star.repo", "plex_ui_12", PS_CONTENT_W - 30, 0,
     ["marcohoch/claudescreen", "niclasvestlund-YT/vibepulse"], True),
    ("star.actor", "plex_ui_16", PS_CONTENT_W, 0, ["@githubfan", "Someone"], True),
    ("star.count", "plex_ui_21", T["VP_SCREEN_W"] - 86 - PS_MARGIN, 0,
     ["999999 stars"], False),
    ("star.dismiss", "plex_ui_12", PS_CONTENT_W, 2, ["TAP TO DISMISS"], False),

    # --- boot screen (platform) ------------------------------------------
    ("boot.wordmark", "plex_attention_25", T["VP_SCREEN_W"], 0, ["VIBEPULSE"], False),
    ("boot.step", "plex_ui_16", 72, 2, ["WIFI", "TIME", "DATA"], False),

    # --- OTA overlay (platform) ------------------------------------------
    ("ota.word", "plex_attention_25", T["VP_SCREEN_W"], 0, OTA_WORDS, False),
    ("ota.detail", "plex_ui_12", T["VP_SCREEN_W"], 2, OTA_DETAILS, False),
    ("ota.version", "plex_ui_12", T["VP_SCREEN_W"], 2, ["v0.6.0-9-g9bab42e"], False),
    ("ota.pill", "plex_ui_16", OTA_PILL_W, 2, ["LATER", "UPDATE NOW"], False),
    ("ota.center", "plex_num_84", T["VP_SCREEN_W"], 0, ["100", "62", "09:59"], False),
    ("ota.pctsign", "plex_stat_35", 40, 0, ["%"], False),
]


def audit() -> list[tuple]:
    """Return one row per element: (name, worst string, width, box, verdict)."""
    rows = []
    for name, font_name, box, letter_space, strings, free_form in ELEMENTS:
        worst_text, worst_width, missing = "", -1.0, None
        for text in strings:
            try:
                width = measure(text, font_name, letter_space)
            except MissingGlyph as error:
                missing = error
                continue
            if width > worst_width:
                worst_text, worst_width = text, width
        if missing is not None:
            verdict = "MISSING GLYPH"
        elif worst_width <= box:
            verdict = "fits"
        elif free_form:
            verdict = "ellipsized"
        else:
            verdict = "OVERFLOW"
        rows.append((name, font_name, worst_text, worst_width, box, verdict))
    return rows


def main(argv: list[str]) -> int:
    problems = check_defines()
    for problem in problems:
        print(f"STALE CONSTANT: {problem}")

    rows = audit()
    width_name = max(len(r[0]) for r in rows)
    bad = 0
    for name, font_name, text, width, box, verdict in rows:
        ok = verdict in ("fits", "ellipsized")
        flag = "" if ok else "  <<<"
        if not ok:
            bad += 1
        print(f"{name:<{width_name}}  {font_name:<18} "
              f"{width:6.1f} / {box:4d} px  {verdict:<22} {text!r}{flag}")
    print()
    print(f"{len(rows)} elements, {bad} need attention, "
          f"{len(problems)} stale constants")
    return 1 if (bad or problems) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
