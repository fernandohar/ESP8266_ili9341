#!/usr/bin/env python3
"""Pack soot-sprite "mole" blobs for Whack-a-Mole into a uniform sprite sheet.

This source PNG is a dense, seamless texture where most soot creatures
touch or overlap their neighbours, so most connected components are giant
fused clusters of several creatures rather than a single one. We only keep
components whose bounding box is a plausible single-creature size and
roughly circular (not an elongated chain of several fused blobs), then rank
those by pixel count so the biggest genuinely isolated creatures win.

The source also has no real alpha channel (flat white background), so for
each kept blob we label every connected dark region during the flood-fill
pass, then crop keeps only pixels that either (a) belong to that blob's own
label, or (b) are enclosed by it and unreachable from the crop border (its
eyes) - everything else, including a neighbour's silhouette poking into the
same rectangle, becomes transparent. The result is converted to RGBA before
packing so image_to_sheet() can key off real alpha rather than a global
color.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

SOURCE = "assets/soot_mole_source.png"
OUTPUT_HEADER = "src/sprite_soot_mole.h"
PREVIEW = "assets/sprite_soot_mole_preview.png"
CELL = 44
COLS = 5
MAX_GLYPHS = 10
BG_THRESHOLD = 235

# A lone soot creature's bounding box in the 894x894 source is roughly
# square and falls in this size range; fused multi-creature clusters are
# either much bigger or noticeably elongated, so this filters them out.
MIN_SIZE = 20
MAX_SIZE = 160
MIN_ASPECT = 0.6
MAX_ASPECT = 1.6


def is_soot_pixel(r, g, b):
    return r < BG_THRESHOLD or g < BG_THRESHOLD or b < BG_THRESHOLD


def label_blobs(image):
    """Flood-fill every connected dark region, tagging each pixel in
    `labels` with its blob id (-1 for background). Returns (labels, blobs)
    where blobs is a list of (id, min_x, min_y, max_x, max_y, count)."""
    pixels = image.load()
    w, h = image.size
    labels = [[-1] * w for _ in range(h)]
    blobs = []
    next_id = 0

    for y in range(h):
        for x in range(w):
            if labels[y][x] != -1:
                continue
            r, g, b = pixels[x, y][:3]
            if not is_soot_pixel(r, g, b):
                continue

            blob_id = next_id
            next_id += 1
            stack = [(x, y)]
            min_x = max_x = x
            min_y = max_y = y
            count = 0

            while stack:
                cx, cy = stack.pop()
                if cx < 0 or cy < 0 or cx >= w or cy >= h or labels[cy][cx] != -1:
                    continue
                cr, cg, cb = pixels[cx, cy][:3]
                if not is_soot_pixel(cr, cg, cb):
                    continue
                labels[cy][cx] = blob_id
                count += 1
                min_x = min(min_x, cx)
                max_x = max(max_x, cx)
                min_y = min(min_y, cy)
                max_y = max(max_y, cy)
                stack.extend([(cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)])

            bw = max_x - min_x + 1
            bh = max_y - min_y + 1
            if count < 60 or bw < 12 or bh < 12:
                continue
            if not (MIN_SIZE <= bw <= MAX_SIZE and MIN_SIZE <= bh <= MAX_SIZE):
                continue
            aspect = bw / bh
            if not (MIN_ASPECT <= aspect <= MAX_ASPECT):
                continue
            blobs.append((blob_id, min_x, min_y, max_x + 1, max_y + 1, count))

    blobs.sort(key=lambda item: item[5], reverse=True)
    return labels, blobs[:MAX_GLYPHS]


def isolate_blob(source, labels, blob_id, box):
    """Crop box from source, keeping only this blob's own pixels plus any
    region enclosed by it (eyes); everything reachable from the crop border
    without crossing this blob's pixels (background OR another blob) becomes
    transparent."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    src_pixels = source.load()

    is_ours = [[labels[y0 + y][x0 + x] == blob_id for x in range(w)] for y in range(h)]

    reachable = [[False] * w for _ in range(h)]
    stack = []
    for x in range(w):
        stack.append((x, 0))
        stack.append((x, h - 1))
    for y in range(h):
        stack.append((0, y))
        stack.append((w - 1, y))

    while stack:
        x, y = stack.pop()
        if x < 0 or y < 0 or x >= w or y >= h or reachable[y][x] or is_ours[y][x]:
            continue
        reachable[y][x] = True
        stack.extend([(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)])

    rgba = Image.new("RGBA", (w, h))
    out = rgba.load()
    for y in range(h):
        for x in range(w):
            r, g, b = src_pixels[x0 + x, y0 + y][:3]
            out[x, y] = (255, 255, 255, 0) if reachable[y][x] else (r, g, b, 255)
    return rgba


def main():
    source = Image.open(SOURCE).convert("RGB")
    labels, blobs = label_blobs(source)
    if not blobs:
        raise SystemExit("No soot blobs found in source image")

    rows = (len(blobs) + COLS - 1) // COLS
    sheet_w = COLS * CELL
    sheet_h = rows * CELL
    packed = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))
    regions = []

    for i, (blob_id, x0, y0, x1, y1, _count) in enumerate(blobs):
        glyph = isolate_blob(source, labels, blob_id, (x0, y0, x1, y1))
        fitted = glyph.resize((CELL - 4, CELL - 4), Image.Resampling.LANCZOS)
        cell_x = (i % COLS) * CELL
        cell_y = (i // COLS) * CELL
        dest_x = cell_x + (CELL - fitted.width) // 2
        dest_y = cell_y + (CELL - fitted.height) // 2
        packed.paste(fitted, (dest_x, dest_y), fitted)
        regions.append((f"MOLE{i}", cell_x, cell_y, CELL, CELL))

    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    packed.save(PREVIEW)

    width, height, bitmap, mask_rows = image_to_sheet(packed, regions=regions)
    write_header(OUTPUT_HEADER, "sprite_soot_mole", width, height, bitmap, mask_rows, regions)
    print(f"Packed {len(blobs)} soot-mole sprites into {OUTPUT_HEADER}")


if __name__ == "__main__":
    main()
