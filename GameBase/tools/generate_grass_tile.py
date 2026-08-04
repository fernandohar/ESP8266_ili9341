#!/usr/bin/env python3
"""Crop a small 50x50 grass tile for use as a repeating (tiled) background.

Cropped from a clean, rock/leaf-free patch of the grass reference. The raw
crop is made seamless with the classic offset-and-feather trick so that when
the scene repeats it across X and Y there are no hard seams. Emits a tiny
PROGMEM header (50x50 = ~5 KB) that replaces the ~150 KB full-screen
image_tictactoe_bg.h.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

SOURCE = "assets/tictactoe_bg_elements_source.png"
OUTPUT_PNG = "assets/grass_tile_preview.png"
OUTPUT_HEADER = "src/image_grass_tile.h"
TILE = 50
# Clean green patch located in the grass reference (see generate notes).
CROP = (190, 150, 190 + 120, 150 + 120)


def make_seamless(img):
    """Offset by half, then feather the cross seam with a mirrored blend."""
    w, h = img.size
    ox, oy = w // 2, h // 2
    offset = Image.new("RGB", (w, h))
    offset.paste(img.crop((w - ox, h - oy, w, h)), (0, 0))
    offset.paste(img.crop((0, h - oy, w - ox, h)), (ox, 0))
    offset.paste(img.crop((w - ox, 0, w, h - oy)), (0, oy))
    offset.paste(img.crop((0, 0, w - ox, h - oy)), (ox, oy))

    # Blend the seam cross with the mirror of the original to soften it.
    mirror = offset.transpose(Image.FLIP_LEFT_RIGHT).transpose(Image.FLIP_TOP_BOTTOM)
    px = offset.load()
    mx = mirror.load()
    feather = 10
    for y in range(h):
        for x in range(w):
            dx = min(abs(x - ox), feather)
            dy = min(abs(y - oy), feather)
            near = min(feather - dx, feather - dy)
            if near <= 0:
                continue
            t = near / feather * 0.5
            r, g, b = px[x, y]
            mr, mg, mb = mx[x, y]
            px[x, y] = (int(r * (1 - t) + mr * t),
                        int(g * (1 - t) + mg * t),
                        int(b * (1 - t) + mb * t))
    return offset


def main():
    if not os.path.exists(SOURCE):
        raise SystemExit(f"Missing source: {SOURCE}")

    src = Image.open(SOURCE).convert("RGB")
    patch = src.crop(CROP)
    patch = make_seamless(patch)
    tile = patch.resize((TILE, TILE), Image.Resampling.LANCZOS)

    os.makedirs(os.path.dirname(OUTPUT_PNG), exist_ok=True)
    # Save a 5x5 tiled preview so seams are easy to spot.
    preview = Image.new("RGB", (TILE * 5, TILE * 5))
    for gy in range(5):
        for gx in range(5):
            preview.paste(tile, (gx * TILE, gy * TILE))
    preview.save(OUTPUT_PNG)

    width, height, bitmap, mask_rows = image_to_sheet(tile)
    write_header(OUTPUT_HEADER, "grass_tile", width, height, bitmap, mask_rows)
    print("Generated", OUTPUT_HEADER)
    print("Generated", OUTPUT_PNG)


if __name__ == "__main__":
    main()
