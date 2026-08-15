"""The native GitHub mark must stay derived from its reviewable sources."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ProjectStarAssetTests(unittest.TestCase):
    def test_github_mark_is_native_precoloured_24_pixel_i4(self):
        pgm_path = ROOT / "components/app_tokens/assets/github-mark-24.pgm"
        tokens = pgm_path.read_text().split()
        self.assertEqual(tokens[:4], ["P2", "24", "24", "255"])
        source_pixels = [int(value) for value in tokens[4:]]
        self.assertEqual(len(source_pixels), 24 * 24)

        palette = bytearray()
        for level in range(16):
            palette.extend((0x97, 0x8C, 0x85, round(level * 255 / 15)))
        packed = bytearray()
        for offset in range(0, len(source_pixels), 2):
            high = round(source_pixels[offset] * 15 / 255)
            low = round(source_pixels[offset + 1] * 15 / 255)
            packed.append((high << 4) | low)
        expected = bytes(palette + packed)

        c_source = (ROOT / "components/app_tokens/project_star_assets.c").read_text()
        array = re.search(
            r"github_mark_24_data\[\]\s*=\s*\{(.*?)\};",
            c_source,
            re.DOTALL,
        )
        self.assertIsNotNone(array)
        compiled_pixels = [
            int(value, 16)
            for value in re.findall(r"0x[0-9a-fA-F]{2}", array.group(1))
        ]
        self.assertEqual(bytes(compiled_pixels), expected)
        self.assertEqual(len(compiled_pixels), 64 + 24 * 24 // 2)
        self.assertIn(".cf = LV_COLOR_FORMAT_I4", c_source)
        self.assertIn(".w = GITHUB_MARK_W", c_source)
        self.assertIn(".h = GITHUB_MARK_H", c_source)
        self.assertIn(".stride = GITHUB_MARK_STRIDE", c_source)

        svg = (ROOT / "components/app_tokens/assets/github-mark-24.svg").read_text()
        self.assertIn('fill="#858c97"', svg)

        popup = (ROOT / "components/app_tokens/project_star_popup.c").read_text()
        self.assertNotIn("image_recolor", popup)

        self.assertEqual(source_pixels[0], 0)
        self.assertEqual(source_pixels[-1], 0)
        self.assertIn(255, source_pixels)
        self.assertTrue(any(0 < value < 255 for value in source_pixels))


if __name__ == "__main__":
    unittest.main()
