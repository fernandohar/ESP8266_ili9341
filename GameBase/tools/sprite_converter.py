#!/usr/bin/env python3
"""Convert PNG sprite(s) to PROGMEM headers with 4/8/16-bit color + 1-bit mask."""

from __future__ import annotations

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install pillow", file=sys.stderr)
    sys.exit(1)

sys.path.insert(0, os.path.dirname(__file__))
from sprite_encoding import (  # noqa: E402
    compose_sheet,
    encode_sheet,
    preview_stats,
    write_asset_header,
    write_legacy_header,
)


def load_rgba(path: str) -> Image.Image:
    return Image.open(path).convert("RGBA")


def parse_regions(spec: str | None, count: int) -> list[str]:
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
        "--quantize",
        action="store_true",
        help="Merge similar colors when the image exceeds the palette limit",
    )
    parser.add_argument(
        "--max-colors",
        type=int,
        help="Palette cap when quantizing (defaults to the bpp limit)",
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
    encoded = encode_sheet(
        sheet,
        regions,
        args.bpp,
        transparent_rgb,
        quantize=args.quantize,
        max_colors=args.max_colors,
    )

    output_path = args.output
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    if args.legacy:
        write_legacy_header(output_path, args.name, encoded)
    else:
        write_asset_header(output_path, args.name, encoded)

    print(f"Wrote {output_path}")
    print(preview_stats(encoded))
    for label, x, y, w, h in encoded.regions:
        print(f"  region {label}: {w}x{h} at ({x},{y})")

    if args.preview_out and encoded.preview is not None:
        encoded.preview.save(args.preview_out)
        print(f"Preview PNG: {args.preview_out}")

    if args.preview and encoded.preview is not None:
        encoded.preview.show()


if __name__ == "__main__":
    main()
