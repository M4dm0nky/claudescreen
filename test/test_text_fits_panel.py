#!/usr/bin/env python3
"""No string may be cut mid glyph on the panel the registry names.

The 480 -> 240 port shipped fourteen overflows that simulator captures did
not reveal, because the fixtures happened to carry the short variants
("FABLE · WEEK" fits where "WEEKLY · ALL MODELS" does not). Looking is not a
test. This measures every element against its box using the committed font
metrics, so the next resolution change reports its damage instead of hiding
it behind a half-drawn letter.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.text_fit import ELEMENTS, audit, check_defines, measure  # noqa: E402


class TextFitsPanelTests(unittest.TestCase):
    def test_derived_widths_still_match_the_firmware_defines(self):
        # The audit mirrors a handful of C #defines. If one is renamed or
        # retuned, every measurement below silently drifts — catch that here
        # rather than trusting numbers taken from a stale source.
        self.assertEqual(check_defines(), [])

    def test_every_element_fits_or_ellipsizes(self):
        offenders = [
            (name, font, text, f"{width:.1f}", box, verdict)
            for name, font, text, width, box, verdict in audit()
            if verdict not in ("fits", "ellipsized")
        ]
        self.assertEqual(offenders, [], "text does not fit its box")

    def test_every_listed_string_has_a_glyph_in_its_font(self):
        # A missing glyph renders as a hollow rectangle, which is how the
        # value hero once shipped a box instead of "%".
        missing = []
        for name, font, _box, letter_space, strings, _dot in ELEMENTS:
            for text in strings:
                try:
                    measure(text, font, letter_space)
                except Exception as error:  # MissingGlyph
                    missing.append(f"{name}: {error}")
        self.assertEqual(missing, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
