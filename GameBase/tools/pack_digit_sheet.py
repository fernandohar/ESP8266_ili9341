#!/usr/bin/env python3
"""Crop tightly-bounded digits and pack into a clean spaced sprite sheet."""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import flood_fill_digits, image_to_sheet, write_header

try:
    from PIL import Image
except ImportError:
    print("Pillow is required", file=sys.stderr)
    sys.exit(1)

# Tight source bounds measured from the digit strip art.
SMALL_SOURCE = [
    (85, 8, 7, 10),
    (93, 8, 5, 10),
    (99, 8, 8, 10),
    (107, 8, 6, 10),
    (113, 8, 7, 10),
    (120, 8, 8, 10),
    (128, 8, 7, 10),
    (135, 8, 7, 10),
    (142, 8, 7, 10),
    (149, 8, 6, 10),
]

LARGE_SOURCE = [
    (15, 25, 21, 30),
    (39, 25, 15, 30),
    (57, 25, 33, 30),
    (96, 25, 15, 30),
    (117, 25, 27, 30),
    (150, 25, 15, 30),
    (168, 25, 18, 30),
    (189, 25, 18, 30),
    (207, 25, 15, 30),
    (216, 25, 9, 30),
]


def trim_bounds(image, box):
    x0, y0, w, h = box
    x1 = x0 + w - 1
    y1 = y0 + h - 1
    px = image.load()

    def is_ink(r, g, b, a):
        if a < 20:
            return False
        if r > 240 and g > 240 and b > 240:
            return False
        return True

    while x0 <= x1 and all(not is_ink(*px[x0, y]) for y in range(y0, y1 + 1)):
        x0 += 1
    while x1 >= x0 and all(not is_ink(*px[x1, y]) for y in range(y0, y1 + 1)):
        x1 -= 1
    while y0 <= y1 and all(not is_ink(*px[x, y0]) for x in range(x0, x1 + 1)):
        y0 += 1
    while y1 >= y0 and all(not is_ink(*px[x, y1]) for x in range(x0, x1 + 1)):
        y1 -= 1
    return (x0, y0, x1 - x0 + 1, y1 - y0 + 1)


def pack_row(image, boxes, cell_w, cell_h, y_offset, pad=2):
    row = Image.new("RGBA", (cell_w * 10, cell_h), (255, 255, 255, 0))
    regions = []
    for i, box in enumerate(boxes):
        crop = image.crop((box[0], box[1], box[0] + box[2], box[1] + box[3]))
        dest_x = i * cell_w + max(0, (cell_w - crop.width) // 2)
        dest_y = y_offset + max(0, (cell_h - crop.height) // 2)
        row.paste(crop, (dest_x, dest_y), crop)
        regions.append((f"d{i}", i * cell_w, y_offset, cell_w, cell_h))
    return row, regions


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("-o", "--output", required=True)
    parser.add_argument("--preview", help="Optional packed PNG preview path")
    args = parser.parse_args()

    source = Image.open(args.input).convert("RGBA")
    filled = flood_fill_digits(
        source,
        (255, 255, 255),
        [("d", x, y, w, h) for x, y, w, h in SMALL_SOURCE + LARGE_SOURCE],
    )

    small_boxes = [trim_bounds(source, box) for box in SMALL_SOURCE]
    large_boxes = [trim_bounds(source, box) for box in LARGE_SOURCE]

    # Digit 9 extends left of the naive 222..224 slice; widen using ink scan.
    lx, ly, lw, lh = large_boxes[9]
    if lw < 10:
        px = source.load()
        y0, y1 = LARGE_SOURCE[9][1], LARGE_SOURCE[9][1] + LARGE_SOURCE[9][3] - 1
        ink_x = [
            x
            for x in range(200, source.width)
            if any(
                px[x, y][3] >= 20
                and not (px[x, y][0] > 240 and px[x, y][1] > 240 and px[x, y][2] > 240)
                for y in range(y0, y1 + 1)
            )
        ]
        if ink_x:
            x0 = min(ink_x)
            x1 = max(ink_x)
            large_boxes[9] = (x0, y0, x1 - x0 + 1, y1 - y0 + 1)

    small_row, small_regions = pack_row(source, small_boxes, 10, 12, 0)
    large_row, large_regions = pack_row(source, large_boxes, 24, 32, 14)

    packed = Image.new("RGBA", (240, 46), (255, 255, 255, 0))
    packed.paste(small_row, (0, 0), small_row)
    packed.paste(large_row, (0, 0), large_row)

    if args.preview:
        packed.save(args.preview)

    packed.save(args.input.replace(".png", "_packed.png"))

    regions = []
    for i in range(10):
        regions.append((f"small{i}",) + small_regions[i][1:])
    for i in range(10):
        regions.append((f"large{i}",) + large_regions[i][1:])

    width, height, bitmap, mask_rows = image_to_sheet(
        packed,
        transparent_rgb=(255, 255, 255),
        fill_digits=True,
        regions=regions,
    )

    write_header(args.output, "sprite_digits", width, height, bitmap, mask_rows, regions)
    print("Packed digit sheet written to", args.output)
    for label, x, y, w, h in regions:
        print(f"  {label}: {x},{y},{w},{h}")


if __name__ == "__main__":
    main()
