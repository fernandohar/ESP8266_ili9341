#!/usr/bin/env python3
"""Generate the Pet Totoro room background for the 240x320 display."""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import image_to_sheet, write_header

from PIL import Image

WIDTH = 240
HEIGHT = 320
OUTPUT_PNG = "assets/pet_totoro_bg_preview.png"
OUTPUT_HEADER = "src/image_pet_totoro_bg.h"
ROOM_REF = "assets/pet_totoro_room_ref.png"
ACORN_REF = "assets/acorn_catch_bg_preview.png"

# Positive shifts the crop window toward the bottom (keeps the foreground rug,
# trims ceiling/lamp) when the reference is taller than the 3:4 display.
ROOM_VERT_BIAS = 70
# Horizontal recentre when the reference is wider than the display.
ROOM_CENTER_BIAS = 0

# Earthy Ghibli palette sampled from acorn_catch_bg_preview + warm room tones.
PALETTE = [
    (5, 18, 27),
    (4, 25, 31),
    (57, 59, 53),
    (90, 85, 70),
    (120, 98, 68),
    (150, 118, 78),
    (176, 145, 102),
    (205, 180, 140),
    (220, 200, 165),
    (235, 220, 185),
    (96, 132, 84),
    (72, 116, 70),
    (58, 98, 62),
    (40, 74, 48),
    (30, 52, 36),
    (180, 210, 190),
    (140, 185, 165),
    (110, 155, 135),
    (70, 105, 88),
    (45, 68, 58),
    (235, 170, 90),
    (210, 140, 70),
    (170, 105, 55),
    (120, 72, 40),
    (85, 52, 30),
    (60, 38, 22),
    (170, 120, 80),
    (200, 160, 110),
]


def nearest_palette(rgb):
    r, g, b = rgb[:3]
    best = PALETTE[0]
    best_dist = 1e9
    for color in PALETTE:
        dr = r - color[0]
        dg = g - color[1]
        db = b - color[2]
        dist = dr * dr + dg * dg + db * db
        if dist < best_dist:
            best_dist = dist
            best = color
    return best


def quantize_image(image):
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            pixels[x, y] = nearest_palette(pixels[x, y])
    return image


def crop_room_portrait(ref):
    """Centre-crop the reference to the display's 3:4 aspect ratio."""
    w, h = ref.size
    target_ratio = WIDTH / HEIGHT

    if w / h > target_ratio:
        slice_w = int(h * target_ratio)
        left = (w - slice_w) // 2 + ROOM_CENTER_BIAS
        left = max(0, min(left, w - slice_w))
        return ref.crop((left, 0, left + slice_w, h))

    slice_h = int(w / target_ratio)
    top = (h - slice_h) // 2 + ROOM_VERT_BIAS
    top = max(0, min(top, h - slice_h))
    return ref.crop((0, top, w, top + slice_h))


def pixelize(image):
    small = image.resize((WIDTH // 2, HEIGHT // 2), Image.Resampling.LANCZOS)
    return small.resize((WIDTH, HEIGHT), Image.Resampling.NEAREST)


def harmonize_with_acorn(image):
    if not os.path.exists(ACORN_REF):
        return image
    overlay = Image.new("RGB", image.size, nearest_palette((57, 59, 53)))
    return Image.blend(image, overlay, 0.06)


def build_from_reference():
    if not os.path.exists(ROOM_REF):
        raise SystemExit(f"Missing room reference: {ROOM_REF}")

    ref = Image.open(ROOM_REF).convert("RGB")
    portrait = crop_room_portrait(ref)
    image = pixelize(portrait)
    image = quantize_image(image)
    return image


def main():
    image = build_from_reference()
    image = harmonize_with_acorn(image)
    os.makedirs(os.path.dirname(OUTPUT_PNG), exist_ok=True)
    image.save(OUTPUT_PNG)

    width, height, bitmap, mask_rows = image_to_sheet(image)
    write_header(OUTPUT_HEADER, "pet_totoro_bg", width, height, bitmap, mask_rows)
    print("Generated", OUTPUT_PNG)
    print("Generated", OUTPUT_HEADER)


if __name__ == "__main__":
    main()
