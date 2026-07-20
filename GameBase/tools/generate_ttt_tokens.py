#!/usr/bin/env python3
"""Build the Tic-Tac-Toe player tokens (Mei-head = O, Cat-Bus-head = X).

Both sources are busy illustrations with non-uniform backgrounds (sky for
Mei, dark watercolour for the Cat Bus), so instead of trying to key out the
background we crop a square around each face and apply a circular alpha mask
with a thin coloured ring - giving two clean, game-token-style circular
markers that read well over the grass board. Packed into a 2-cell sprite
sheet consumed by SpriteSheet/Avatar in Scene_TicTacToe.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image, ImageDraw

OUTPUT_HEADER = "src/sprite_ttt_tokens.h"
PREVIEW = "assets/sprite_ttt_tokens_preview.png"
CELL = 46
RING = 2

# label, source path, crop box (square-ish around the face), ring color
TOKENS = [
    ("MEI",    "assets/ttt_mei_source.png",    (285, 35, 715, 465), (240, 90, 150)),   # O
    ("CATBUS", "assets/ttt_catbus_source.png", (250, 70, 410, 230), (80, 190, 235)),   # X
]


def make_circular_token(source_path, box, ring_rgb):
    src = Image.open(source_path).convert("RGB")
    crop = src.crop(box)
    # Square it (center-crop the longer side) so the circle isn't distorted.
    w, h = crop.size
    side = min(w, h)
    left = (w - side) // 2
    top = (h - side) // 2
    crop = crop.crop((left, top, left + side, top + side))

    # High-res render then downsample for a smooth anti-aliased circle edge.
    hi = CELL * 8
    face = crop.resize((hi - RING * 16, hi - RING * 16), Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (hi, hi), (0, 0, 0, 0))
    canvas.paste(face, (RING * 8, RING * 8))

    mask = Image.new("L", (hi, hi), 0)
    md = ImageDraw.Draw(mask)
    md.ellipse((0, 0, hi - 1, hi - 1), fill=255)
    canvas.putalpha(mask)

    draw = ImageDraw.Draw(canvas)
    draw.ellipse((RING * 4, RING * 4, hi - 1 - RING * 4, hi - 1 - RING * 4),
                 outline=ring_rgb + (255,), width=RING * 8)

    return canvas.resize((CELL, CELL), Image.Resampling.LANCZOS)


def main():
    cols = len(TOKENS)
    sheet = Image.new("RGBA", (cols * CELL, CELL), (0, 0, 0, 0))
    regions = []
    for i, (label, path, box, ring) in enumerate(TOKENS):
        token = make_circular_token(path, box, ring)
        sheet.paste(token, (i * CELL, 0), token)
        regions.append((label, i * CELL, 0, CELL, CELL))

    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    sheet.save(PREVIEW)

    width, height, bitmap, mask_rows = image_to_sheet(sheet, regions=regions)
    write_header(OUTPUT_HEADER, "sprite_ttt_tokens", width, height, bitmap, mask_rows, regions)
    print(f"Packed {len(TOKENS)} tokens into {OUTPUT_HEADER}")


if __name__ == "__main__":
    main()
