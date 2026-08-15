"""The VibePulse Claude pet, as SVG rects from the real asset's 16x16 grid,
with emotive poses for the Needs You concepts. Cell-accurate to
components/app_tokens/assets/source/claude-pet-white.png."""

# Base grid (x, y, w, h) in cells, from the extracted 16x16 raster:
# body rows 3-10, full-width arm band rows 7-8, eyes at (4,5) and (11,5)
# 2 cells tall, legs at x 3,5,10,12 rows 11-12.

def mascot(x, y, cell, colour="#D97757", pose="neutral", bg="#000000"):
    """Return SVG for the pet with its top-left at (x, y)."""
    rects = []

    def cellrect(cx, cy, cw, ch, col):
        rects.append(
            f'<rect x="{x + cx * cell}" y="{y + cy * cell}" '
            f'width="{cw * cell}" height="{ch * cell}" fill="{col}"/>')

    # body block rows 3-10 (x2..x13), arm band rows 7-8 full width
    cellrect(2, 3, 12, 8, colour)
    cellrect(0, 7, 16, 2, colour)

    # legs
    for lx in (3, 5, 10, 12):
        cellrect(lx, 11, 1, 2, colour)

    # eyes: punch out with background colour
    if pose == "neutral":
        cellrect(4, 5, 1, 2, bg)
        cellrect(11, 5, 1, 2, bg)
    elif pose == "asking":
        # one eye normal, one widened - the head-tilt look without a tilt
        cellrect(4, 5, 1, 2, bg)
        cellrect(10.5, 4.5, 2, 2.5, bg)
    elif pose == "happy":
        # delighted ^ ^ eyes: chevrons, not slits (slits read as asleep)
        for ex in (3.2, 10.2):
            cellrect(ex, 6.0, 0.9, 0.9, bg)
            cellrect(ex + 0.85, 5.2, 0.9, 0.9, bg)
            cellrect(ex + 1.7, 6.0, 0.9, 0.9, bg)
    elif pose == "alert":
        # wide eyes
        cellrect(3.5, 4.5, 2, 2.5, bg)
        cellrect(10.5, 4.5, 2, 2.5, bg)

    if pose == "asking":
        # one raised arm: a column above the arm band on the right
        cellrect(14, 4, 2, 3, colour)

    return "".join(rects)


def mascot_width(cell):
    return 16 * cell


def mascot_height(cell):
    return 13 * cell  # rows 0-12 used; row 3 is the visual top


def bubble_question(x, y, cell, colour="#D97757", bg="#000000"):
    """A chunky pixel '?' in a pixel speech bubble, same cell language."""
    parts = []

    def c(cx, cy, cw, ch, col):
        parts.append(
            f'<rect x="{x + cx * cell}" y="{y + cy * cell}" '
            f'width="{cw * cell}" height="{ch * cell}" fill="{col}"/>')

    # bubble body 9x9 with a 2-cell tail bottom-left
    c(0, 0, 9, 9, colour)
    c(1, 9, 2, 1, colour)
    c(1, 10, 1, 1, colour)
    # classic 5x7 pixel '?', punched out in bg, centred
    q = ["_###_",
         "#___#",
         "____#",
         "__##_",
         "__#__",
         "_____",
         "__#__"]
    for ry, row in enumerate(q):
        for rx, ch in enumerate(row):
            if ch == "#":
                c(2 + rx, 1 + ry, 1, 1, bg)
    return "".join(parts)
