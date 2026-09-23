#!/usr/bin/env python3
"""Cut the Totoro poster down to the slide puzzle's board size and encode it.

The poster is 2:3, and so is the puzzle board (3 cols x 4 rows of 56x63 tiles),
so the whole picture fits with no crop and no visible distortion - which matters
because the title text is one of the few high-contrast landmarks a player can use
to place the bottom row.

Median cut is applied here rather than left to the encoder: this is photographic
art with tens of thousands of distinct colours, and encode_sheet's fallback
reduction is an O(n^2) pairwise merge that would never finish (same reason
generate_catbus_bg.py pre-quantizes).

Keep the tile geometry in step with PUZZLE_TILE_W / PUZZLE_TILE_H in
src/Scene_SlidePuzzle.h - the scene indexes the sheet by tile, so a size change
here silently shifts every tile.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from sprite_encoding import compose_sheet, encode_sheet, preview_stats, write_header

from PIL import Image

SOURCE = "assets/puzzle_totoro_source.png"
OUTPUT_PNG = "assets/puzzle_totoro.png"
OUTPUT_HEADER = "src/image_puzzle_totoro.h"
NAME = "puzzle_totoro"

# Must match src/Scene_SlidePuzzle.h.
COLS, ROWS = 3, 4
TILE_W, TILE_H = 56, 63
MAX_COLORS = 256


def main():
    if not os.path.exists(SOURCE):
        raise SystemExit(f"Missing source: {SOURCE}")

    size = (COLS * TILE_W, ROWS * TILE_H)
    src = Image.open(SOURCE).convert("RGB")
    board = src.resize(size, Image.Resampling.LANCZOS)
    board = board.quantize(
        colors=MAX_COLORS, method=Image.MEDIANCUT, dither=Image.NONE
    ).convert("RGB")
    board.save(OUTPUT_PNG)

    sheet, regions = compose_sheet([board.convert("RGBA")], ["full"], "horizontal")
    encoded = encode_sheet(sheet, regions, bpp=8, transparent_rgb=None)
    write_header(OUTPUT_HEADER, NAME, encoded)

    print(f"Generated {OUTPUT_HEADER} ({size[0]}x{size[1]})")
    print(f"Generated {OUTPUT_PNG}")
    print(preview_stats(encoded))


if __name__ == "__main__":
    main()
