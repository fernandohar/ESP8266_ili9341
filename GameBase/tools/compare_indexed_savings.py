#!/usr/bin/env python3
"""Estimate flash savings if existing legacy headers were converted to indexed color."""

from __future__ import annotations

import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
from reindex_header import decode_legacy, parse_dimensions, parse_symbol, parse_u16_array, parse_u8_array
from sprite_encoding import choose_bpp, preview_stats
from sprite_encoding import EncodedSheet, encode_raster


def file_size(path: str) -> int:
    return os.path.getsize(path)


def analyze_header(path: str) -> dict:
    text = open(path, encoding="utf-8").read()
    if "SpriteAsset" in text and "PROGMEM" in text:
        return {"path": path, "already_indexed": True}

    symbol = parse_symbol(text)
    width, height = parse_dimensions(text)
    pixels = parse_u16_array(text, symbol)
    mask_rows = parse_u8_array(text, f"{symbol}Mask")
    colors, opaque_grid = decode_legacy(pixels, width, height, mask_rows)
    opaque_values = [
        colors[y][x] for y in range(height) for x in range(width) if opaque_grid[y][x]
    ]
    unique = len(set(opaque_values))
    bpp = choose_bpp(unique, "auto", quantize=True, max_colors=None)
    encoded = encode_raster(colors, opaque_grid, width, height, bpp, True, None)
    encoded.regions = [("full", 0, 0, width, height)]
    baseline = width * height * 2
    new_pixels = len(encoded.pixels) * (1 if bpp < 16 else 2)
    new_total = new_pixels + len(encoded.palette) * 2 + len(encoded.mask_rows)
    old_total = file_size(path)
    return {
        "path": path,
        "already_indexed": False,
        "width": width,
        "height": height,
        "unique_colors": unique,
        "auto_bpp": bpp,
        "old_file_bytes": old_total,
        "new_est_bytes": new_total,
        "saved": old_total - new_total,
    }


def main() -> None:
    headers = sorted(glob.glob("src/sprite_*.h") + glob.glob("src/image_*.h"))
    if not headers:
        headers = sorted(glob.glob("GameBase/src/sprite_*.h") + glob.glob("GameBase/src/image_*.h"))

    rows = [analyze_header(path) for path in headers]
    rows = [row for row in rows if not row.get("already_indexed")]
    rows.sort(key=lambda row: row["saved"], reverse=True)

    print(f"{'header':40} {'unique':>6} {'bpp':>3} {'old KB':>8} {'new KB':>8} {'saved KB':>9}")
    print("-" * 80)
    total_saved = 0
    for row in rows:
        total_saved += max(row["saved"], 0)
        print(
            f"{os.path.basename(row['path']):40} "
            f"{row['unique_colors']:6d} "
            f"{row['auto_bpp']:3d} "
            f"{row['old_file_bytes']/1024:8.1f} "
            f"{row['new_est_bytes']/1024:8.1f} "
            f"{row['saved']/1024:9.1f}"
        )
    print("-" * 80)
    print(f"Estimated total savings (auto bpp + quantize): {total_saved/1024:.1f} KB")


if __name__ == "__main__":
    os.chdir(os.path.join(os.path.dirname(__file__), ".."))
    main()
