#!/usr/bin/env python3
"""Convert a PNG to a PROGMEM sprite sheet header with optional region metadata."""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install pillow", file=sys.stderr)
    sys.exit(1)


def rgb_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_opaque_pixel(r, g, b, a, transparent_rgb=None):
    if a < 20:
        return False
    if transparent_rgb is not None:
        tr, tg, tb = transparent_rgb
        if abs(r - tr) <= 12 and abs(g - tg) <= 12 and abs(b - tb) <= 12:
            return False
    return True


def is_outline_pixel(r, g, b, a):
    return a >= 20 and r < 140 and g < 140 and b < 140


def flood_fill_digits(image, transparent_rgb=None, regions=None):
    pixels = image.load()
    width, height = image.size
    filled = set()

    fill_targets = []
    if regions:
        for _label, x, y, w, h in regions:
            fill_targets.append((x, y, w, h))
    else:
        fill_targets.append((0, 0, width, height))

    for x0, y0, w, h in fill_targets:
        seeds = []
        cx = x0 + w // 2
        cy = y0 + h // 2
        seeds.append((cx, cy))
        seeds.append((x0 + 2, y0 + 2))
        seeds.append((x0 + w - 3, y0 + 2))

        for sx, sy in seeds:
            if not (x0 <= sx < x0 + w and y0 <= sy < y0 + h):
                continue
            r, g, b, a = pixels[sx, sy]
            if is_outline_pixel(r, g, b, a):
                continue
            if not is_opaque_pixel(r, g, b, a, transparent_rgb):
                stack = [(sx, sy)]
                visited = set()
                while stack:
                    x, y = stack.pop()
                    if (x, y) in visited:
                        continue
                    if not (x0 <= x < x0 + w and y0 <= y < y0 + h):
                        continue
                    visited.add((x, y))
                    pr, pg, pb, pa = pixels[x, y]
                    if is_outline_pixel(pr, pg, pb, pa):
                        continue
                    if is_opaque_pixel(pr, pg, pb, pa, transparent_rgb) and not (
                        pr > 200 and pg > 200 and pb > 200
                    ):
                        continue
                    pixels[x, y] = (255, 255, 255, 255)
                    filled.add((x, y))
                    stack.extend([(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)])

    return filled


def image_to_sheet(path_or_image, transparent_rgb=None, fill_digits=False, regions=None):
    if isinstance(path_or_image, Image.Image):
        image = path_or_image.convert("RGBA")
    else:
        image = Image.open(path_or_image).convert("RGBA")
    filled_pixels = set()
    if fill_digits:
        filled_pixels = flood_fill_digits(image, transparent_rgb, regions)
    width, height = image.size
    pixels = image.load()

    bitmap = []
    mask_rows = []
    for y in range(height):
        row_bits = []
        for x in range(width):
            r, g, b, a = pixels[x, y]
            opaque = is_opaque_pixel(r, g, b, a, transparent_rgb) or (x, y) in filled_pixels
            bitmap.append(rgb_to_rgb565(r, g, b) if opaque else 0xFFFF)
            row_bits.append("1" if opaque else "0")

        for i in range(0, width, 8):
            chunk = row_bits[i : i + 8]
            while len(chunk) < 8:
                chunk.append("0")
            mask_rows.append(int("".join(chunk), 2))

    return width, height, bitmap, mask_rows


def format_array(values, per_line=12):
    lines = []
    for i in range(0, len(values), per_line):
        chunk = values[i : i + per_line]
        lines.append("  " + ", ".join(f"0x{v:04X}" for v in chunk) + ",")
    return "\n".join(lines)


def write_header(
    output_path,
    name,
    width,
    height,
    bitmap,
    mask_rows,
    regions=None,
):
    guard = f"_{name.upper()}_H_"
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(f"#ifndef {guard}\n#define {guard}\n#include <Arduino.h>\n\n")
        f.write(f"#define {name.upper()}_WIDTH {width}\n")
        f.write(f"#define {name.upper()}_HEIGHT {height}\n\n")
        f.write(f"const unsigned short {name}[{len(bitmap)}] PROGMEM={{\n")
        f.write(format_array(bitmap))
        f.write("\n};\n\n")
        f.write(f"const uint8_t {name}Mask[{len(mask_rows)}] PROGMEM={{\n")
        f.write(format_array(mask_rows, per_line=16))
        f.write("\n};\n")

        if regions:
            f.write(f"\nstatic const SpriteSheetRegion {name}Regions[] PROGMEM = {{\n")
            for label, x, y, w, h in regions:
                f.write(f"  {{ {x}, {y}, {w}, {h} }}, // {label}\n")
            f.write("};\n")

        f.write(f"\n#endif\n")


def main():
    parser = argparse.ArgumentParser(description="Convert PNG to sprite sheet header")
    parser.add_argument("input")
    parser.add_argument("-o", "--output", required=True)
    parser.add_argument("-n", "--name", required=True)
    parser.add_argument(
        "--transparent",
        help="RGB color treated as transparent, e.g. 255,255,255",
    )
    parser.add_argument(
        "--regions",
        help="Region spec: label,x,y,w,h;label2,x,y,w,h",
    )
    parser.add_argument(
        "--fill-digits",
        action="store_true",
        help="Flood-fill hollow digit interiors with solid white",
    )
    args = parser.parse_args()

    transparent_rgb = None
    if args.transparent:
        transparent_rgb = tuple(int(v) for v in args.transparent.split(","))

    regions = None
    if args.regions:
        regions = []
        for item in args.regions.split(";"):
            if not item.strip():
                continue
            parts = item.split(",")
            label = parts[0]
            x, y, w, h = (int(v) for v in parts[1:])
            regions.append((label, x, y, w, h))

    width, height, bitmap, mask_rows = image_to_sheet(
        args.input, transparent_rgb, args.fill_digits, regions
    )

    write_header(args.output, args.name, width, height, bitmap, mask_rows, regions)


if __name__ == "__main__":
    main()
