#!/usr/bin/env python3
"""Generate the Whack-a-Mole onsen-floor background for the 240x320 display.

The source reference is already a 3:4 portrait image (768x1024), matching the
display aspect ratio exactly, so this is a straight resize - no cropping or
palette reduction needed (the baked-in TIME/SCORE/HITS wooden sign at the
bottom is decorative framing; the scene overlays live digits on top of it).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

WIDTH = 240
HEIGHT = 320
SOURCE = "assets/whackamole_bg_source.png"
OUTPUT_PNG = "assets/whackamole_bg_preview.png"
OUTPUT_HEADER = "src/image_whackamole_bg.h"


def main():
    if not os.path.exists(SOURCE):
        raise SystemExit(f"Missing background reference: {SOURCE}")

    image = Image.open(SOURCE).convert("RGB")
    image = image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)

    os.makedirs(os.path.dirname(OUTPUT_PNG), exist_ok=True)
    image.save(OUTPUT_PNG)

    width, height, bitmap, mask_rows = image_to_sheet(image)
    write_header(OUTPUT_HEADER, "whackamole_bg", width, height, bitmap, mask_rows)
    print("Generated", OUTPUT_PNG)
    print("Generated", OUTPUT_HEADER)


if __name__ == "__main__":
    main()
