#!/usr/bin/env python3
"""Pack LEVEL UP font glyphs into a uniform sprite sheet."""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

SOURCE = "assets/font_levelup_source.png"

# Tight bounds measured from font_levelup_source.png
GLYPH_BOUNDS = {
    "A": (21, 93, 45, 39),
    "B": (96, 93, 43, 39),
    "C": (167, 93, 47, 39),
    "D": (242, 93, 43, 39),
    "E": (317, 93, 42, 39),
    "F": (390, 93, 40, 39),
    "G": (457, 93, 43, 39),
    "H": (526, 93, 48, 39),
    "I": (606, 93, 39, 39),
    "J": (680, 93, 38, 39),
    "K": (21, 151, 47, 43),
    "L": (95, 151, 43, 43),
    "M": (164, 151, 51, 43),
    "N": (242, 151, 43, 43),
    "O": (317, 151, 42, 43),
    "P": (390, 151, 40, 43),
    "Q": (457, 151, 43, 43),
    "R": (529, 151, 44, 43),
    "S": (605, 151, 41, 43),
    "T": (676, 151, 43, 43),
    "U": (21, 205, 45, 40),
    "V": (94, 205, 46, 40),
    "W": (167, 205, 47, 40),
    "X": (241, 205, 46, 40),
    "Y": (315, 205, 44, 40),
    "Z": (389, 205, 40, 40),
    "?": (457, 205, 43, 40),
    "!": (540, 205, 20, 40),
    "#": (602, 205, 46, 40),
    ".": (678, 205, 18, 40),
    "-": (704, 205, 15, 40),
}

ORDER = list("ABCDEFGHIJKLMNOPQRSTUVWXYZ?!#.-")
CELL_W = 24
CELL_H = 20
COLS = 8
ROWS = 4
CELL_PADDING = 1


def fit_glyph(glyph, cell_w, cell_h, padding=CELL_PADDING):
    w, h = glyph.size
    max_w = cell_w - padding * 2
    max_h = cell_h - padding * 2
    scale = min(max_w / w, max_h / h)
    new_w = max(1, int(round(w * scale)))
    new_h = max(1, int(round(h * scale)))
    return glyph.resize((new_w, new_h), Image.Resampling.LANCZOS), new_w, new_h


def main():
    output = "src/sprite_letters.h"
    preview = "assets/sprite_letters_preview.png"
    source = Image.open(SOURCE).convert("RGBA")

    sheet_w = COLS * CELL_W
    sheet_h = ROWS * CELL_H
    packed = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))

    regions = []
    for i, ch in enumerate(ORDER):
        x0, y0, w, h = GLYPH_BOUNDS[ch]
        glyph = source.crop((x0, y0, x0 + w, y0 + h))
        fitted, fw, fh = fit_glyph(glyph, CELL_W, CELL_H)
        cell_x = (i % COLS) * CELL_W
        cell_y = (i // COLS) * CELL_H
        dest_x = cell_x + (CELL_W - fw) // 2
        dest_y = cell_y + (CELL_H - fh) // 2
        packed.paste(fitted, (dest_x, dest_y), fitted)
        label = "HASH" if ch == "#" else ("DOT" if ch == "." else ("DASH" if ch == "-" else ch))
        regions.append((label, cell_x, cell_y, CELL_W, CELL_H))

    os.makedirs(os.path.dirname(preview), exist_ok=True)
    packed.save(preview)

    width, height, bitmap, mask_rows = image_to_sheet(
        packed,
        transparent_rgb=(0, 0, 0),
        fill_digits=False,
        regions=regions,
    )
    write_header(output, "sprite_letters", width, height, bitmap, mask_rows, regions)
    print("Generated", output)


if __name__ == "__main__":
    main()
