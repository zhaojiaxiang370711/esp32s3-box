"""Check actual generated glyph maps, so new UI copy cannot silently lose Chinese."""
import pathlib
import re
import unittest

BOARD = pathlib.Path(__file__).resolve().parents[2] / 'main/boards/alientek/atk-dnesp32s3-box'


def glyphs(size):
    source = (BOARD / f'box_menu_font_{size}.c').read_text()
    mappings = re.search(r'cmaps\[\]\s*=\s*\{(.*?)\n\};', source, re.S).group(1)
    result = set()
    for block in re.findall(r'\{([^{}]+)\}', mappings):
        start = int(re.search(r'\.range_start = (\d+)', block).group(1))
        length = int(re.search(r'\.range_length = (\d+)', block).group(1))
        if 'CMAP_FORMAT0_TINY' in block:
            result.update(range(start, start + length))
        else:
            name = re.search(r'\.unicode_list = (\w+)', block).group(1)
            values = re.search(name + r'\[\]\s*=\s*\{(.*?)\}', source, re.S).group(1)
            result.update(start + int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', values))
    return result, int(re.search(r'\.line_height = (\d+)', source).group(1))


class BoxFontTests(unittest.TestCase):
    def test_all_ui_characters_exist_in_both_sizes(self):
        source = (BOARD / 'box_menu_display.cc').read_text()
        required = set(range(32, 127)) | {ord(c) for c in source if ord(c) > 127}
        for size in (14, 20):
            present, _ = glyphs(size)
            self.assertFalse(required - present, f'{size}px missing: ' + ''.join(chr(c) for c in sorted(required - present)))

    def test_line_heights_fit_settings_rows_and_footer(self):
        _, small = glyphs(14)
        _, title = glyphs(20)
        self.assertLessEqual(214 + small, 240 - 4)
        self.assertLessEqual(2 + small, 26 - 4)
        self.assertLessEqual(31 + title, 65 - 4)
