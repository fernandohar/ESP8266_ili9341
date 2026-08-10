#!/usr/bin/env python3
"""Generate the Pet Totoro forest-clearing background for the 240x320 display.

Expects a portrait pixel-art source (3:4) such as assets/pet_totoro_forest_bg_source.png.
Downscales with nearest-neighbour so chunky pixels stay crisp, then encodes as an
8-bit indexed SpriteAsset with palette quantization for flash savings.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_to_spritesheet import load_processed_image
from sprite_encoding import encode_sheet, preview_stats, write_asset_header

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, "assets", "pet_totoro_forest_bg_source.png")
OUTPUT_PNG = os.path.join(ROOT, "assets", "pet_totoro_forest_bg_preview.png")
OUTPUT_HEADER = os.path.join(ROOT, "src", "image_pet_totoro_forest_bg.h")
NAME = "pet_totoro_forest_bg"
WIDTH = 240
HEIGHT = 320


def resize_portrait_nearest(image):
    """Fit the source to the display with nearest-neighbour sampling."""
    w, h = image.size
    target_ratio = WIDTH / HEIGHT
    if w / h > target_ratio:
        slice_w = int(h * target_ratio)
        left = (w - slice_w) // 2
        image = image.crop((left, 0, left + slice_w, h))
    elif w / h < target_ratio:
        slice_h = int(w / target_ratio)
        top = (h - slice_h) // 2
        image = image.crop((0, top, w, top + slice_h))
    return image.resize((WIDTH, HEIGHT), Image.Resampling.NEAREST)


def detect_ground_y(image):
    """Find the grass/dirt boundary near the bottom (pet foot line)."""
    pixels = image.load()
    w, h = image.size
    for y in range(h - 1, int(h * 0.65), -1):
        browns = greens = 0
        for x in range(0, w, 4):
            r, g, b = pixels[x, y][:3]
            if g > r + 6 and g > b + 4:
                greens += 1
            if r > g + 8 and r > b + 5:
                browns += 1
        if browns > greens // 2 and browns >= 8:
            return max(y - 12, int(h * 0.72))
    return h - 48


def quantize_pixel_art(image, max_colors):
    """Median-cut quantize before encode_sheet (its merge step is too slow at 240x320)."""
    rgb = image.convert("RGB")
    sample = rgb.copy()
    sample.thumbnail((128, 128), Image.Resampling.NEAREST)
    palette_img = sample.quantize(colors=max_colors, method=Image.MEDIANCUT, dither=Image.NONE)
    return rgb.quantize(palette=palette_img, dither=Image.NONE).convert("RGBA")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default=SOURCE)
    parser.add_argument("--max-colors", type=int, default=48)
    args = parser.parse_args()

    if not os.path.exists(args.source):
        raise SystemExit(f"Missing background source: {args.source}")

    image = load_processed_image(args.source, None, False, None).convert("RGBA")
    image = resize_portrait_nearest(image)
    image = quantize_pixel_art(image, args.max_colors)

    os.makedirs(os.path.dirname(OUTPUT_PNG), exist_ok=True)
    image.convert("RGB").save(OUTPUT_PNG)

    regions = [("full", 0, 0, WIDTH, HEIGHT)]
    encoded = encode_sheet(
        image,
        regions,
        bpp=8,
        transparent_rgb=None,
        quantize=False,
        max_colors=args.max_colors,
    )
    write_asset_header(OUTPUT_HEADER, NAME, encoded)

    ground_y = detect_ground_y(image.convert("RGB"))
    print("Generated", OUTPUT_PNG)
    print("Generated", OUTPUT_HEADER)
    print(preview_stats(encoded))
    print(f"Suggested PET_GROUND_Y={ground_y}")


if __name__ == "__main__":
    main()
