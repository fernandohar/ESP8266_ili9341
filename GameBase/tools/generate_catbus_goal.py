#!/usr/bin/env python3
"""Build a side-view Cat Bus sprite for the lane-crossing mini-game goal."""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

OUTPUT = "src/sprite_catbus_goal.h"
PREVIEW = "assets/sprite_catbus_goal_preview.png"
SOURCE = "assets/ttt_catbus_source.png"
# Side profile crop from the source illustration (x0, y0, x1, y1).
CROP = (8, 28, 466, 272)
TARGET_W = 120
TARGET_H = 44


def main():
    src = Image.open(SOURCE).convert("RGBA")
    crop = src.crop(CROP)

    # Key out near-white paper background.
    pixels = crop.load()
    w, h = crop.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if r > 230 and g > 230 and b > 220:
                pixels[x, y] = (r, g, b, 0)

    scaled = crop.resize((TARGET_W, TARGET_H), Image.Resampling.LANCZOS)
    width, height, bitmap, mask_rows = image_to_sheet(scaled)
    write_header(OUTPUT, "sprite_catbus_goal", width, height, bitmap, mask_rows)

    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    scaled.save(PREVIEW)
    print(f"Wrote {OUTPUT} ({width}x{height})")


if __name__ == "__main__":
    main()
