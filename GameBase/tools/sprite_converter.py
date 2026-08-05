#!/usr/bin/env python3
"""Convert PNG sprite(s) to PROGMEM headers with 4/8/16-bit color + 1-bit mask."""

from __future__ import annotations

import argparse
import math
import os
import sys
from dataclasses import dataclass
from typing import Iterable, List, Optional, Sequence, Tuple

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install pillow", file=sys.stderr)
    sys.exit(1)


Color = Tuple[int, int, int]
Region = Tuple[str, int, int, int, int]


def rgb_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def rgb565_to_rgb(value: int) -> Color:
    r = ((value >> 11) & 0x1F) * 255 // 31
    g = ((value >> 5) & 0x3F) * 255 // 63
    b = (value & 0x1F) * 255 // 31
    return r, g, b


def is_opaque_pixel(r: int, g: int, b: int, a: int, transparent_rgb: Optional[Color]) -> bool:
    if a < 20:
        return False
    if transparent_rgb is not None:
        tr, tg, tb = transparent_rgb
        if abs(r - tr) <= 12 and abs(g - tg) <= 12 and abs(b - tb) <= 12:
            return False
    return True


def max_palette_size(bpp: int) -> int:
    if bpp == 4:
        return 16
    if bpp == 8:
        return 256
    return 0


@dataclass
class EncodedSheet:
    bpp: int
    sheet_width: int
    sheet_height: int
    palette: List[int]
    pixels: List[int]
    mask_rows: List[int]
    regions: List[Region]
    preview: Image.Image


def nearest_palette_index(rgb565: int, palette: List[int]) -> int:
    tr, tg, tb = rgb565_to_rgb(rgb565)
    best_idx = 0
    best_dist = float("inf")
    for idx, color in enumerate(palette):
        sr, sg, sb = rgb565_to_rgb(color)
        dist = (tr - sr) ** 2 + (tg - sg) ** 2 + (tb - sb) ** 2
        if dist < best_dist:
            best_dist = dist
            best_idx = idx
    return best_idx


def build_palette_from_colors(colors: Iterable[int], bpp: int) -> List[int]:
    unique = list(dict.fromkeys(colors))
    limit = max_palette_size(bpp)
    if len(unique) > limit:
        raise ValueError(
            f"Image uses {len(unique)} unique opaque colors but {bpp}-bit allows at most {limit}. "
            "Reduce colors in the source art or choose a higher bit depth."
        )
    return unique


def pack_mask_row(row_bits: Sequence[str]) -> int:
    value = 0
    for bit in row_bits:
        value = (value << 1) | (1 if bit == "1" else 0)
    return value


def encode_mask_rows(width: int, height: int, opaque_grid: Sequence[Sequence[bool]]) -> List[int]:
    mask_rows: List[int] = []
    for y in range(height):
        row_bits = ["1" if opaque_grid[y][x] else "0" for x in range(width)]
        for i in range(0, width, 8):
            chunk = row_bits[i : i + 8]
            while len(chunk) < 8:
                chunk.append("0")
            mask_rows.append(pack_mask_row(chunk))
    return mask_rows


def encode_pixels_16(opaque_colors: Sequence[Sequence[int]], opaque_grid: Sequence[Sequence[bool]]) -> List[int]:
    height = len(opaque_colors)
    width = len(opaque_colors[0]) if height else 0
    pixels: List[int] = []
    for y in range(height):
        for x in range(width):
            pixels.append(opaque_colors[y][x] if opaque_grid[y][x] else 0xFFFF)
    return pixels


def encode_pixels_8(
    opaque_colors: Sequence[Sequence[int]],
    opaque_grid: Sequence[Sequence[bool]],
    palette: List[int],
) -> List[int]:
    height = len(opaque_colors)
    width = len(opaque_colors[0]) if height else 0
    pixels: List[int] = []
    for y in range(height):
        for x in range(width):
            if opaque_grid[y][x]:
                pixels.append(nearest_palette_index(opaque_colors[y][x], palette))
            else:
                pixels.append(0)
    return pixels


def encode_pixels_4(
    opaque_colors: Sequence[Sequence[int]],
    opaque_grid: Sequence[Sequence[bool]],
    palette: List[int],
) -> List[int]:
    height = len(opaque_colors)
    width = len(opaque_colors[0]) if height else 0
    packed: List[int] = []
    for y in range(height):
        x = 0
        while x < width:
            high = 0
            low = 0
            if opaque_grid[y][x]:
                high = nearest_palette_index(opaque_colors[y][x], palette)
            x += 1
            if x < width and opaque_grid[y][x]:
                low = nearest_palette_index(opaque_colors[y][x], palette)
            packed.append((high << 4) | (low & 0x0F))
            x += 1
    return packed


def load_rgba(path: str) -> Image.Image:
    return Image.open(path).convert("RGBA")


def compose_sheet(
    images: Sequence[Image.Image],
    labels: Sequence[str],
    layout: str,
) -> Tuple[Image.Image, List[Region]]:
    if not images:
        raise ValueError("At least one input image is required")

    if len(images) == 1:
        img = images[0]
        return img, [("full", 0, 0, img.width, img.height)]

    if layout == "horizontal":
        width = sum(img.width for img in images)
        height = max(img.height for img in images)
        sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        regions: List[Region] = []
        x = 0
        for label, img in zip(labels, images):
            sheet.paste(img, (x, 0))
            regions.append((label, x, 0, img.width, img.height))
            x += img.width
        return sheet, regions

    if layout == "vertical":
        width = max(img.width for img in images)
        height = sum(img.height for img in images)
        sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        regions = []
        y = 0
        for label, img in zip(labels, images):
            sheet.paste(img, (0, y))
            regions.append((label, 0, y, img.width, img.height))
            y += img.height
        return sheet, regions

    raise ValueError(f"Unsupported layout: {layout}")


def rasterize_sheet(
    sheet: Image.Image,
    transparent_rgb: Optional[Color],
) -> Tuple[List[List[int]], List[List[bool]]]:
    width, height = sheet.size
    pixels = sheet.load()
    colors: List[List[int]] = []
    opaque_grid: List[List[bool]] = []
    for y in range(height):
        row_colors: List[int] = []
        row_opaque: List[bool] = []
        for x in range(width):
            r, g, b, a = pixels[x, y]
            opaque = is_opaque_pixel(r, g, b, a, transparent_rgb)
            row_colors.append(rgb_to_rgb565(r, g, b))
            row_opaque.append(opaque)
        colors.append(row_colors)
        opaque_grid.append(row_opaque)
    return colors, opaque_grid


def encode_sheet(
    sheet: Image.Image,
    regions: Sequence[Region],
    bpp: int,
    transparent_rgb: Optional[Color],
) -> EncodedSheet:
    colors, opaque_grid = rasterize_sheet(sheet, transparent_rgb)
    width, height = sheet.size

    opaque_values = [
        colors[y][x] for y in range(height) for x in range(width) if opaque_grid[y][x]
    ]
    palette: List[int] = []
    if bpp in (4, 8):
        palette = build_palette_from_colors(opaque_values, bpp)

    if bpp == 16:
        pixels = encode_pixels_16(colors, opaque_grid)
    elif bpp == 8:
        pixels = encode_pixels_8(colors, opaque_grid, palette)
    elif bpp == 4:
        pixels = encode_pixels_4(colors, opaque_grid, palette)
    else:
        raise ValueError("bpp must be 4, 8, or 16")

    mask_rows = encode_mask_rows(width, height, opaque_grid)
    preview = sheet.copy()
    return EncodedSheet(
        bpp=bpp,
        sheet_width=width,
        sheet_height=height,
        palette=palette,
        pixels=pixels,
        mask_rows=mask_rows,
        regions=list(regions),
        preview=preview,
    )


def format_u16_array(values: Sequence[int], per_line: int = 12) -> str:
    lines = []
    for i in range(0, len(values), per_line):
        chunk = values[i : i + per_line]
        lines.append("  " + ", ".join(f"0x{v:04X}" for v in chunk) + ",")
    return "\n".join(lines)


def format_u8_array(values: Sequence[int], per_line: int = 16) -> str:
    lines = []
    for i in range(0, len(values), per_line):
        chunk = values[i : i + per_line]
        lines.append("  " + ", ".join(f"0x{v:02X}" for v in chunk) + ",")
    return "\n".join(lines)


def write_legacy_header(path: str, name: str, encoded: EncodedSheet) -> None:
    if encoded.bpp != 16:
        raise ValueError("Legacy header output supports 16-bit sprites only")

    guard = f"_{name.upper()}_H_"
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(f"#ifndef {guard}\n#define {guard}\n#include <Arduino.h>\n\n")
        handle.write(f"#define {name.upper()}_WIDTH {encoded.sheet_width}\n")
        handle.write(f"#define {name.upper()}_HEIGHT {encoded.sheet_height}\n\n")
        handle.write(f"const unsigned short {name}[{len(encoded.pixels)}] PROGMEM={{\n")
        handle.write(format_u16_array(encoded.pixels))
        handle.write("\n};\n\n")
        handle.write(f"const uint8_t {name}Mask[{len(encoded.mask_rows)}] PROGMEM={{\n")
        handle.write(format_u8_array(encoded.mask_rows))
        handle.write("\n};\n")

        if len(encoded.regions) > 1 or (
            encoded.regions and encoded.regions[0][0] != "full"
        ):
            handle.write(
                f"\nstatic const SpriteSheetRegion {name}Regions[] PROGMEM = {{\n"
            )
            for label, x, y, w, h in encoded.regions:
                handle.write(f"  {{ {x}, {y}, {w}, {h} }}, // {label}\n")
            handle.write("};\n")

        handle.write("\n#endif\n")


def write_asset_header(path: str, name: str, encoded: EncodedSheet) -> None:
    guard = f"_{name.upper()}_H_"
    pixel_type = "uint16_t" if encoded.bpp == 16 else "uint8_t"
    bpp_enum = f"SPRITE_BPP_{encoded.bpp}"

    with open(path, "w", encoding="utf-8") as handle:
        handle.write(f"#ifndef {guard}\n#define {guard}\n#include <Arduino.h>\n")
        handle.write('#include "SpriteAsset.h"\n\n')
        handle.write(f"#define {name.upper()}_BPP {encoded.bpp}\n")
        handle.write(f"#define {name.upper()}_SHEET_WIDTH {encoded.sheet_width}\n")
        handle.write(f"#define {name.upper()}_SHEET_HEIGHT {encoded.sheet_height}\n")
        handle.write(f"#define {name.upper()}_PALETTE_COUNT {len(encoded.palette)}\n")
        handle.write(f"#define {name.upper()}_BITMAP_COUNT {len(encoded.regions)}\n\n")

        if encoded.palette:
            handle.write(
                f"static const uint16_t {name}Palette[{len(encoded.palette)}] PROGMEM = {{\n"
            )
            handle.write(format_u16_array(encoded.palette))
            handle.write("\n};\n\n")

        handle.write(
            f"static const {pixel_type} {name}Pixels[{len(encoded.pixels)}] PROGMEM = {{\n"
        )
        if encoded.bpp == 16:
            handle.write(format_u16_array(encoded.pixels))
        else:
            handle.write(format_u8_array(encoded.pixels))
        handle.write("\n};\n\n")

        handle.write(f"static const uint8_t {name}Mask[{len(encoded.mask_rows)}] PROGMEM = {{\n")
        handle.write(format_u8_array(encoded.mask_rows))
        handle.write("\n};\n\n")

        handle.write(f"static const SpriteBitmapRegion {name}Bitmaps[{len(encoded.regions)}] PROGMEM = {{\n")
        for label, x, y, w, h in encoded.regions:
            handle.write(f"  {{ {x}, {y}, {w}, {h} }}, // {label}\n")
        handle.write("};\n\n")

        palette_ref = "NULL"
        if encoded.palette:
            palette_ref = f"{name}Palette"

        handle.write(f"static const SpriteAsset {name} PROGMEM = {{\n")
        handle.write(f"  {bpp_enum},\n")
        handle.write(f"  {name.upper()}_SHEET_WIDTH,\n")
        handle.write(f"  {name.upper()}_SHEET_HEIGHT,\n")
        handle.write(f"  {name.upper()}_PALETTE_COUNT,\n")
        handle.write(f"  {palette_ref},\n")
        handle.write(f"  {name}Pixels,\n")
        handle.write(f"  {name}Mask,\n")
        handle.write("};\n\n")

        if encoded.bpp == 16:
            handle.write(f"#define {name.upper()}_WIDTH {name.upper()}_SHEET_WIDTH\n")
            handle.write(f"#define {name.upper()}_HEIGHT {name.upper()}_SHEET_HEIGHT\n")
            handle.write(
                f"static const unsigned short *{name}LegacyBitmap = "
                f"(const unsigned short *){name}Pixels;\n"
            )
            handle.write(f"static const uint8_t *{name}LegacyMask = {name}Mask;\n")

        handle.write("\n#endif\n")


def write_header(path: str, name: str, encoded: EncodedSheet, legacy: bool) -> None:
    if legacy:
        write_legacy_header(path, name, encoded)
    else:
        write_asset_header(path, name, encoded)


def preview_stats(encoded: EncodedSheet) -> str:
    pixel_bytes = len(encoded.pixels) * (2 if encoded.bpp == 16 else 1)
    palette_bytes = len(encoded.palette) * 2
    mask_bytes = len(encoded.mask_rows)
    rgb565_bytes = encoded.sheet_width * encoded.sheet_height * 2
    saved = rgb565_bytes - (pixel_bytes + palette_bytes)
    return (
        f"Sheet: {encoded.sheet_width}x{encoded.sheet_height}, "
        f"{encoded.bpp}-bit, palette={len(encoded.palette)} colors\n"
        f"  pixels: {pixel_bytes} B\n"
        f"  palette: {palette_bytes} B\n"
        f"  mask: {mask_bytes} B\n"
        f"  vs RGB565 baseline ({rgb565_bytes} B pixels): "
        f"{'saved' if saved > 0 else 'cost'} {abs(saved)} B"
    )


def parse_regions(spec: Optional[str], count: int) -> List[str]:
    if not spec:
        return [f"frame{i}" for i in range(count)]
    labels = [part.strip() for part in spec.split(",") if part.strip()]
    if len(labels) != count:
        raise ValueError(f"Expected {count} region labels, got {len(labels)}")
    return labels


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert PNG sprite(s) to GameBase PROGMEM headers (4/8/16-bit + mask)."
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        help="One PNG for a single sprite, or multiple PNGs that share one palette",
    )
    parser.add_argument("-o", "--output", required=True, help="Output .h path")
    parser.add_argument("-n", "--name", required=True, help="C symbol prefix, e.g. sprite_foo")
    parser.add_argument(
        "--bpp",
        type=int,
        choices=[4, 8, 16],
        default=8,
        help="Bits per pixel (default: 8)",
    )
    parser.add_argument(
        "--layout",
        choices=["horizontal", "vertical"],
        default="horizontal",
        help="How to pack multiple input PNGs into one sheet",
    )
    parser.add_argument(
        "--regions",
        help="Comma-separated region labels for multiple inputs, e.g. idle,walk,blink",
    )
    parser.add_argument(
        "--transparent",
        help="RGB color treated as transparent, e.g. 255,0,255",
    )
    parser.add_argument(
        "--legacy",
        action="store_true",
        help="Emit legacy 16-bit header compatible with existing GameBase sprites",
    )
    parser.add_argument(
        "--preview",
        action="store_true",
        help="Open a visual preview window (requires a display or X forwarding)",
    )
    parser.add_argument(
        "--preview-out",
        help="Write a PNG preview of the composed sheet",
    )
    args = parser.parse_args()

    if args.legacy and args.bpp != 16:
        parser.error("--legacy requires --bpp 16")

    transparent_rgb = None
    if args.transparent:
        transparent_rgb = tuple(int(v) for v in args.transparent.split(","))

    images = [load_rgba(path) for path in args.inputs]
    labels = parse_regions(args.regions, len(images))
    sheet, regions = compose_sheet(images, labels, args.layout)
    encoded = encode_sheet(sheet, regions, args.bpp, transparent_rgb)

    output_path = args.output
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    write_header(output_path, args.name, encoded, args.legacy)

    print(f"Wrote {output_path}")
    print(preview_stats(encoded))
    for label, x, y, w, h in encoded.regions:
        print(f"  region {label}: {w}x{h} at ({x},{y})")

    if args.preview_out:
        encoded.preview.save(args.preview_out)
        print(f"Preview PNG: {args.preview_out}")

    if args.preview:
        encoded.preview.show()


if __name__ == "__main__":
    main()
