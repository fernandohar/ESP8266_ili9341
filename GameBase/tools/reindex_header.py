#!/usr/bin/env python3
"""Re-encode an existing legacy RGB565 PROGMEM header as an indexed SpriteAsset header."""

from __future__ import annotations

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
from sprite_encoding import (  # noqa: E402
    EncodedSheet,
    choose_bpp,
    encode_raster,
    preview_stats,
    write_asset_header,
)


def parse_u16_array(text: str, symbol: str) -> list[int]:
    match = re.search(
        rf"const unsigned short {re.escape(symbol)}\[(\d+)\] PROGMEM=\{{(.*?)\}};",
        text,
        re.S,
    )
    if not match:
        raise ValueError(f"Could not find RGB565 array '{symbol}'")
    body = match.group(2)
    return [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{4})", body)]


def parse_u8_array(text: str, symbol: str) -> list[int] | None:
    match = re.search(
        rf"const uint8_t {re.escape(symbol)}\[(\d+)\] PROGMEM=\{{(.*?)\}};",
        text,
        re.S,
    )
    if not match:
        return None
    body = match.group(2)
    values: list[int] = []
    for token in re.findall(r"0x([0-9A-Fa-f]+)", body):
        values.append(int(token, 16) & 0xFF)
    return values


def parse_dimensions(text: str) -> tuple[int, int]:
    width = re.search(r"#define\s+\w+_WIDTH\s+(\d+)", text)
    height = re.search(r"#define\s+\w+_HEIGHT\s+(\d+)", text)
    if not width or not height:
        raise ValueError("Could not parse WIDTH/HEIGHT defines")
    return int(width.group(1)), int(height.group(1))


def parse_symbol(text: str) -> str:
    match = re.search(r"const unsigned short (\w+)\[", text)
    if not match:
        raise ValueError("Could not find bitmap symbol name")
    return match.group(1)


def parse_regions(text: str, symbol: str) -> list[tuple[str, int, int, int, int]]:
    match = re.search(
        rf"static const SpriteSheetRegion {re.escape(symbol)}Regions\[\] PROGMEM = \{{(.*?)\}};",
        text,
        re.S,
    )
    if not match:
        return [("full", 0, 0, 0, 0)]

    regions = []
    for line in match.group(1).splitlines():
        row = re.search(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}\s*,?\s*(?://\s*(.*))?", line)
        if row:
            label = row.group(5).strip() if row.group(5) else f"region{len(regions)}"
            regions.append((label, int(row.group(1)), int(row.group(2)), int(row.group(3)), int(row.group(4))))
    return regions or [("full", 0, 0, 0, 0)]


def mask_bit(mask_rows: list[int], width: int, x: int, y: int) -> bool:
    row_bytes = (width + 7) // 8
    byte = mask_rows[y * row_bytes + (x >> 3)]
    return bool(byte & (0x80 >> (x & 7)))


def decode_legacy(
    pixels: list[int],
    width: int,
    height: int,
    mask_rows: list[int] | None,
) -> tuple[list[list[int]], list[list[bool]]]:
    colors: list[list[int]] = []
    opaque_grid: list[list[bool]] = []
    for y in range(height):
        row_colors: list[int] = []
        row_opaque: list[bool] = []
        for x in range(width):
            value = pixels[y * width + x]
            if mask_rows is not None:
                opaque = mask_bit(mask_rows, width, x, y)
            else:
                opaque = value != 0xFFFF
            row_colors.append(value)
            row_opaque.append(opaque)
        colors.append(row_colors)
        opaque_grid.append(row_opaque)
    return colors, opaque_grid


def reindex_header(
    input_path: str,
    output_path: str,
    indexed: str,
    quantize: bool,
    max_colors: int | None,
) -> EncodedSheet:
    text = open(input_path, encoding="utf-8").read()
    symbol = parse_symbol(text)
    width, height = parse_dimensions(text)
    pixels = parse_u16_array(text, symbol)
    if len(pixels) != width * height:
        raise ValueError(f"Pixel count {len(pixels)} != {width}x{height}")

    mask_rows = parse_u8_array(text, f"{symbol}Mask")
    colors, opaque_grid = decode_legacy(pixels, width, height, mask_rows)
    opaque_values = [
        colors[y][x] for y in range(height) for x in range(width) if opaque_grid[y][x]
    ]
    unique_count = len(set(opaque_values))
    bpp = choose_bpp(unique_count, indexed, quantize, max_colors)
    if indexed != "auto" and indexed != "off":
        bpp = int(indexed)

    encoded = encode_raster(colors, opaque_grid, width, height, bpp, quantize, max_colors)
    regions = parse_regions(text, symbol)
    if regions[0][3] == 0 and regions[0][4] == 0:
        regions = [("full", 0, 0, width, height)]
    encoded.regions = regions
    write_asset_header(output_path, symbol, encoded)
    return encoded


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert legacy RGB565 headers to SpriteAsset format")
    parser.add_argument("input", help="Existing legacy .h file")
    parser.add_argument("-o", "--output", help="Output path (default: overwrite input)")
    parser.add_argument(
        "--indexed",
        choices=["off", "auto", "4", "8", "16"],
        default="auto",
        help="Target bit depth (default: auto)",
    )
    parser.add_argument(
        "--quantize",
        action="store_true",
        help="Merge colors when the image exceeds the palette limit",
    )
    parser.add_argument(
        "--max-colors",
        type=int,
        help="Palette cap when quantizing (defaults to bpp limit)",
    )
    args = parser.parse_args()

    if args.indexed == "off":
        parser.error("Use --indexed 4, 8, 16, or auto to re-encode")

    output = args.output or args.input
    encoded = reindex_header(args.input, output, args.indexed, args.quantize, args.max_colors)
    print(f"Wrote {output}")
    print(preview_stats(encoded))


if __name__ == "__main__":
    main()
