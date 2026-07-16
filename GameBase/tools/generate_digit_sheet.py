#!/usr/bin/env python3
"""Generate clean filled digit sprites and pack them into sprite_digits.h."""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image, ImageDraw, ImageFont


DIGIT_SEGMENTS = {
    "0": ("top", "bottom", "left", "right"),
    "1": ("one",),
    "2": ("top", "right_top", "middle", "left_bottom", "bottom"),
    "3": ("top", "right", "middle", "bottom"),
    "4": ("left_top", "middle", "right"),
    "5": ("top", "left_top", "middle", "right_bottom", "bottom"),
    "6": ("top", "left", "middle", "bottom", "right_bottom"),
    "7": ("top", "right"),
    "8": ("top", "bottom", "left", "right", "middle"),
    "9": ("top", "left_top", "middle", "right", "bottom"),
}


def draw_digit(draw, digit, x, y, w, h, large=False):
    inset = 2 if large else 1
    thick = 4 if large else 2
    cx = x + w // 2
    top = y + inset
    bottom = y + h - inset - 1
    mid = y + h // 2
    left = x + inset
    right = x + w - inset - 1
    fill = (255, 255, 255, 255)
    outline = (20, 20, 20, 255)

    def bar(x0, y0, x1, y1, color):
        draw.rectangle((x0, y0, x1, y1), fill=color)

    parts = {
        "top": (left, top, right, top + thick - 1),
        "bottom": (left, bottom - thick + 1, right, bottom),
        "left": (left, top, left + thick - 1, bottom),
        "right": (right - thick + 1, top, right, bottom),
        "middle": (left, mid - thick // 2, right, mid + thick // 2),
        "left_top": (left, top, left + thick - 1, mid),
        "left_bottom": (left, mid, left + thick - 1, bottom),
        "right_top": (right - thick + 1, top, right, mid),
        "right_bottom": (right - thick + 1, mid, right, bottom),
        "one": (cx - thick // 2, top, cx + thick // 2 - 1, bottom),
    }

    segments = []
    for name in DIGIT_SEGMENTS[str(digit)]:
        segments.append(parts[name])
        if name == "one":
            segments.append((cx - thick, top, cx + thick, top + thick - 1))

    for sx0, sy0, sx1, sy1 in segments:
        bar(sx0 - 1, sy0 - 1, sx1 + 1, sy1 + 1, outline)
    for sx0, sy0, sx1, sy1 in segments:
        bar(sx0, sy0, sx1, sy1, fill)


def build_sheet(large=False):
    count = 10
    cell_w = 24 if large else 10
    cell_h = 32 if large else 12
    sheet = Image.new("RGBA", (cell_w * count, cell_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    for i in range(count):
        x0 = i * cell_w
        draw_digit(draw, str(i), x0, 0, cell_w, cell_h, large=large)
    return sheet, cell_w, cell_h


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", default="src/sprite_digits.h")
    parser.add_argument("--preview", default="assets/digits_packed_preview.png")
    args = parser.parse_args()

    small_sheet, sw, sh = build_sheet(large=False)
    large_sheet, lw, lh = build_sheet(large=True)

    packed = Image.new("RGBA", (240, 46), (0, 0, 0, 0))
    packed.paste(small_sheet, (0, 0), small_sheet)
    packed.paste(large_sheet, (0, 14), large_sheet)
    os.makedirs(os.path.dirname(args.preview) or ".", exist_ok=True)
    packed.save(args.preview)

    regions = []
    for i in range(10):
        regions.append((f"small{i}", i * sw, 0, sw, sh))
    for i in range(10):
        regions.append((f"large{i}", i * lw, 14, lw, lh))

    width, height, bitmap, mask_rows = image_to_sheet(
        packed,
        transparent_rgb=None,
        fill_digits=False,
        regions=regions,
    )

    write_header(args.output, "sprite_digits", width, height, bitmap, mask_rows, regions)
    print("Generated", args.output)


if __name__ == "__main__":
    main()
