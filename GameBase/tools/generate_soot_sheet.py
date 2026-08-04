#!/usr/bin/env python3
"""Pack soot sprites from the source sheet into uniform cells."""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

SOURCE = "assets/soot_source.png"
OUTPUT_HEADER = "src/sprite_soot.h"
PREVIEW = "assets/sprite_soot_preview.png"
CELL = 16
COLS = 8
MAX_GLYPHS = 16


def is_soot_pixel(r, g, b):
    return r < 235 or g < 235 or b < 235


def find_blobs(image):
    pixels = image.load()
    w, h = image.size
    visited = [[False] * w for _ in range(h)]
    blobs = []

    for y in range(h):
        for x in range(w):
            if visited[y][x]:
                continue
            r, g, b = pixels[x, y][:3]
            if not is_soot_pixel(r, g, b):
                continue

            stack = [(x, y)]
            min_x = max_x = x
            min_y = max_y = y
            count = 0

            while stack:
                cx, cy = stack.pop()
                if cx < 0 or cy < 0 or cx >= w or cy >= h or visited[cy][cx]:
                    continue
                cr, cg, cb = pixels[cx, cy][:3]
                if not is_soot_pixel(cr, cg, cb):
                    continue
                visited[cy][cx] = True
                count += 1
                min_x = min(min_x, cx)
                max_x = max(max_x, cx)
                min_y = min(min_y, cy)
                max_y = max(max_y, cy)
                stack.extend([(cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)])

            if count < 40:
                continue
            if (max_x - min_x) < 8 or (max_y - min_y) < 8:
                continue
            blobs.append((min_x, min_y, max_x + 1, max_y + 1, count))

    blobs.sort(key=lambda item: item[4], reverse=True)
    return blobs[:MAX_GLYPHS]


def main():
    source = Image.open(SOURCE).convert("RGBA")
    blobs = find_blobs(source)
    if not blobs:
        raise SystemExit("No soot blobs found in source image")

    rows = (len(blobs) + COLS - 1) // COLS
    sheet_w = COLS * CELL
    sheet_h = rows * CELL
    packed = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))
    regions = []

    for i, (x0, y0, x1, y1, _count) in enumerate(blobs):
        glyph = source.crop((x0, y0, x1, y1))
        fitted = glyph.resize((CELL - 2, CELL - 2), Image.Resampling.LANCZOS)
        cell_x = (i % COLS) * CELL
        cell_y = (i // COLS) * CELL
        dest_x = cell_x + (CELL - fitted.width) // 2
        dest_y = cell_y + (CELL - fitted.height) // 2
        packed.paste(fitted, (dest_x, dest_y), fitted)
        regions.append((f"SOOT{i}", cell_x, cell_y, CELL, CELL))

    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    packed.save(PREVIEW)

    width, height, bitmap, mask_rows = image_to_sheet(
        packed,
        transparent_rgb=(0, 0, 0),
        fill_digits=False,
        regions=regions,
    )
    write_header(OUTPUT_HEADER, "sprite_soot", width, height, bitmap, mask_rows, regions)
    print(f"Packed {len(blobs)} soot sprites into {OUTPUT_HEADER}")


if __name__ == "__main__":
    main()
