#!/usr/bin/env python3
"""Needs You v2 — the APPROVED design direction (2026-08-16), as exact
480x480 concept frames.

Approved as CONCEPTS: the LVGL raster remains the visual authority, Studio
approval of the LVGL captures is still required, and nothing here authorizes
a flash. Regenerate with: python3 tools/mockups/gen_needsyou_v2.py

The direction encodes two expert critiques plus the owner's less-is-more
audit:
- mascot back: 80px in a 96px countdown RING (header), full hero on private
- the ring IS the timer (deletes "118s left" text)
- APPROVE filled, 96px tall; all targets >= 90px
- DENY differentiated in restrained red; LEAVE IT gray
- payload is hero mono type on pure black; BASH demoted to a chip
- one tracked-microcaps line per screen; 3 type sizes
- unified CLAUDE NEEDS YOU; project demoted to footer
- personality in the chrome, never in payload/buttons/timer
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from needsyou_mascot import mascot

BG = "#000000"
TEXT = "#FFFFFF"
MUTED = "#9298A2"
DIM = "#5C687B"
HAIR = "#202328"
CLAUDE = "#D97757"
RED = "#E5484D"

FONT = "IBM Plex Sans,-apple-system,Segoe UI,Roboto,sans-serif"
MONO = "IBM Plex Mono,ui-monospace,SFMono-Regular,Menlo,monospace"
W = H = 480


def head(title, frame=CLAUDE):
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
        f'viewBox="0 0 {W} {H}" role="img" aria-label="{title}">'
        f'<title>{title}</title>'
        f'<rect width="{W}" height="{H}" fill="{BG}"/>'
        f'<rect x="14" y="14" width="452" height="452" rx="40" fill="none" '
        f'stroke="{frame}" stroke-width="2"/>'
        f'<g font-family="{FONT}">'
    )


TAIL = "</g></svg>"


def t(x, y, s, size=17, fill=TEXT, weight=400, ls=0, anchor="start",
      font=None):
    return (
        f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" '
        f'font-weight="{weight}" letter-spacing="{ls}" text-anchor="{anchor}"'
        + (f' font-family="{font}"' if font else "") + f'>{s}</text>')


def ring(cx, cy, r, frac, stroke=8):
    """Depleting countdown ring: full at frac=1, empty at 0. Starts at 12
    o'clock, drains clockwise."""
    import math
    circumference = 2 * math.pi * r
    dash = circumference * frac
    return (
        f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{HAIR}" '
        f'stroke-width="{stroke}"/>'
        f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{CLAUDE}" '
        f'stroke-width="{stroke}" stroke-linecap="round" '
        f'stroke-dasharray="{dash:.1f} {circumference:.1f}" '
        f'transform="rotate(-90 {cx} {cy})"/>')


def header_mascot(pose="asking", frac=0.8):
    """80px mascot inside a 96px countdown ring, left-anchored."""
    s = ring(80, 80, 44, frac)
    # cell 4 -> 64px wide, integer scale; visual centre row ~7.75 lands at 80
    s += mascot(48, 49, 4, colour=CLAUDE, pose=pose, bg=BG)
    return s


def buttons_full(approve_y=248):
    s = f'<rect x="24" y="{approve_y}" width="432" height="96" rx="18" fill="{CLAUDE}"/>'
    s += t(240, approve_y + 60, "APPROVE", 28, BG, 700, 2.4, "middle")
    leave_y = approve_y + 106
    s += (f'<rect x="24" y="{leave_y}" width="432" height="90" rx="18" '
          f'fill="none" stroke="{DIM}" stroke-width="2"/>')
    s += t(240, leave_y + 56, "LEAVE IT", 24, MUTED, 600, 2, "middle")
    return s


def buttons_split(approve_y=248):
    s = f'<rect x="24" y="{approve_y}" width="432" height="96" rx="18" fill="{CLAUDE}"/>'
    s += t(240, approve_y + 60, "APPROVE", 28, BG, 700, 2.4, "middle")
    row_y = approve_y + 108
    s += (f'<rect x="24" y="{row_y}" width="208" height="90" rx="18" '
          f'fill="none" stroke="{RED}" stroke-width="2"/>')
    s += t(128, row_y + 56, "DENY", 24, RED, 700, 2, "middle")
    s += (f'<rect x="248" y="{row_y}" width="208" height="90" rx="18" '
          f'fill="none" stroke="{DIM}" stroke-width="2"/>')
    s += t(352, row_y + 56, "LEAVE IT", 24, MUTED, 600, 2, "middle")
    return s


# ------------------------------------------------------------------ question
def question():
    s = head("Question: mascot asks, ring counts down, approve is a slab")
    s += header_mascot(pose="asking", frac=0.82)
    s += t(148, 58, "CLAUDE NEEDS YOU · VIBEPULSE", 15, CLAUDE, 700, 2)
    s += t(148, 94, "Which authentication", 27, TEXT, 600)
    s += t(148, 128, "approach?", 27, TEXT, 600)
    # the recommendation: an outlined quote from Claude, not a gray slab
    s += (f'<rect x="24" y="140" width="432" height="92" rx="14" fill="none" '
          f'stroke="{HAIR}" stroke-width="2"/>')
    s += t(44, 168, "CLAUDE RECOMMENDS", 14, CLAUDE, 600)
    s += t(44, 196, "New auth layer", 28, TEXT, 700)
    s += t(44, 222, "Cleaner architecture", 17, MUTED, 400)
    s += buttons_full(244)
    s += t(240, 452, "1 MORE OPTION IN TERMINAL", 14, DIM, 600, 1.5,
           "middle")
    return s + TAIL


# ------------------------------------------------------------------ approval
def approval():
    s = head("Approval: the command is the hero, deny is red, ring is time")
    s += header_mascot(pose="neutral", frac=0.65)
    s += t(148, 58, "CLAUDE NEEDS YOU · VIBEPULSE", 15, CLAUDE, 700, 2)
    s += t(148, 94, "Run the test suite", 27, TEXT, 600)
    # payload zone: hero mono on pure black, tool as a chip
    s += (f'<rect x="24" y="146" width="58" height="26" rx="7" fill="none" '
          f'stroke="{HAIR}" stroke-width="1.5"/>')
    s += t(53, 164, "bash", 14, MUTED, 600, 0.5, "middle", MONO)
    s += t(24, 218, "npm test", 40, TEXT, 600, 0, "start", MONO)
    s += buttons_split(252)
    return s + TAIL


# ------------------------------------------------------------------- private
def private():
    """No buttons on purpose: with nothing readable there is no decision to
    make here. Tap anywhere hands the prompt to the terminal immediately;
    KEY3 long-press remains the emergency deny-everything."""
    s = head("Private: the mascot holds the secret; tap sends it to the desk")
    s += ring(240, 152, 72, 0.73, stroke=9)
    s += '<g opacity="0.6">' + mascot(184, 96, 7, colour=CLAUDE, pose="neutral", bg=BG) + "</g>"
    s += t(240, 300, "SOMETHING IS WAITING", 32, TEXT, 700, 0.5, "middle")
    s += t(240, 340, "Details stay on the Mac", 18, MUTED, 400, 0, "middle")
    s += t(240, 434, "TAP TO ANSWER AT YOUR DESK", 17, DIM, 600, 2, "middle")
    return s + TAIL


# ------------------------------------------------------------------- attract
def attract():
    """Stage A: what the room sees. Essentially the shipped alert, with the
    ring now carrying the countdown. Tap anywhere -> decision screen."""
    s = head("Attract stage: what you see from the sofa", frame=CLAUDE)
    s += ring(240, 150, 78, 0.82, stroke=9)
    s += mascot(176, 96, 8, colour=CLAUDE, pose="alert", bg=BG)
    s += t(240, 306, "NEEDS YOU", 54, TEXT, 700, 0, "middle")
    s += t(240, 348, "VIBEPULSE", 21, CLAUDE, 700, 3, "middle")
    s += t(240, 436, "TAP TO ANSWER", 17, DIM, 600, 2, "middle")
    return s + TAIL


# -------------------------------------------------------------------- payoff
def payoff():
    """The tap payoff, as a static concept (motion is a later, separately
    gated step). Crisp, quick, no gloating."""
    s = head("Payoff beat after APPROVE: on it, back to work")
    s += mascot(176, 120, 8, colour=CLAUDE, pose="happy", bg=BG)
    # a few orange spark pixels, hand-placed
    for x, y, w in ((150, 110, 10), (322, 96, 12), (346, 180, 8),
                    (128, 196, 8), (306, 236, 10)):
        s += f'<rect x="{x}" y="{y}" width="{w}" height="{w}" fill="{CLAUDE}"/>'
    s += t(240, 330, "ON IT.", 54, TEXT, 700, 0, "middle")
    s += t(240, 372, "new auth layer", 19, MUTED, 400, 0, "middle")
    return s + TAIL


FRAMES = {
    "needs-you-v2-attract": attract,
    "needs-you-v2-question": question,
    "needs-you-v2-approval": approval,
    "needs-you-v2-private": private,
    "needs-you-v2-payoff": payoff,
}

if __name__ == "__main__":
    out = pathlib.Path(__file__).resolve().parents[2] / "docs" / "img" / "mockups"
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in FRAMES.items():
        svg = fn()
        (out / f"{name}.svg").write_text(svg, encoding="utf-8")
        print(f"{name}.svg  {len(svg):>6} bytes")
