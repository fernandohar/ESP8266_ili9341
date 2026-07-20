#!/usr/bin/env python3
"""Crop the stick tic-tac-toe grid ("#") and emit it as a transparent overlay.

The grid is drawn on a black background in the source; we key that black out
so the grass tiled background shows through the gaps. The scene draws this as
a single Avatar over the tiled grass, with Mei/Cat-Bus tokens on top.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

SOURCE = "assets/tictactoe_bg_elements_source.png"
OUTPUT_PNG = "assets/ttt_grid_preview.png"
OUTPUT_HEADER = "src/sprite_ttt_grid.h"
CROP = (542, 146, 919, 528)
SIZE = 210


def main():
    if not os.path.exists(SOURCE):
        raise SystemExit(f"Missing source: {SOURCE}")

    src = Image.open(SOURCE).convert("RGB")
    grid = src.crop(CROP).resize((SIZE, SIZE), Image.Resampling.LANCZOS)

    os.makedirs(os.path.dirname(OUTPUT_PNG), exist_ok=True)
    grid.save(OUTPUT_PNG)

    width, height, bitmap, mask_rows = image_to_sheet(grid, transparent_rgb=(0, 0, 0))
    write_header(OUTPUT_HEADER, "sprite_ttt_grid", width, height, bitmap, mask_rows)
    print("Generated", OUTPUT_HEADER)


if __name__ == "__main__":
    main()
