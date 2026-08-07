#!/usr/bin/env python3
"""Build the Cat Bus Cross playfield background from the concept artwork.

The source is the two-panel concept sheet; only its right-hand "gameplay view"
panel is real art. That panel already has the soot, Mei, the direction arrows
and a caption drawn into it, so this script paints them back out and leaves a
clean board: forest + Goal sign on top, five dirt lanes, a start strip, and the
side scenery. Everything removed here is drawn at runtime as an Avatar.

Inpainting copies pixels from the same rows a fixed distance to the side. The
board is horizontally stratified - a dirt row stays dirt all the way across, a
grass row stays grass - so a sideways shift always lands on matching material.
Patches are alpha-feathered at the edges so the seam disappears into the
surrounding texture. Two areas need different treatment: the score plate is a
smooth horizontal gradient (one clean column stretched across), and the caption
balloon sits on unstratified foliage (mirror-tiled from a clean strip).

    python3 tools/generate_catbus_bg.py

Order matters: each entry may rely on earlier ones having already cleaned the
area it samples from. Mei in particular reads from the lane the soot just left.
"""

import argparse
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from sprite_encoding import (  # noqa: E402
    compose_sheet,
    encode_sheet,
    preview_stats,
    write_asset_header,
)

# The gameplay panel inside the concept sheet, and the mockup chrome above it.
PANEL_BOX = (516, 6, 1017, 675)
TITLE_H = 44

OUT_W = 240
OUT_H = 320

# Slightly under the 8-bit limit: RGB565 rounding can only merge palette
# entries, never add them, so this always lands within 256.
PALETTE_SIZE = 250

FEATHER = 10

# (x0, y0, x1, y1, source_dx): rects are padded past the sprite by FEATHER so
# the alpha ramp falls on clean material rather than on what we are erasing.
ERASE = [
    (60, 183, 180, 248, 185),    # lane 5: soot + arrow
    (246, 238, 362, 302, -160),  # lane 4: arrow + soot
    (75, 294, 192, 357, 185),    # lane 3: soot + arrow
    (265, 350, 380, 416, -180),  # lane 2: arrow + soot
    # Lane 1 reaches past Mei for its source; she is still standing at 188-306.
    (68, 408, 196, 478, 242),    # lane 1: soot + arrow
    (188, 428, 306, 572, -118),  # Mei, spanning lane 1 into the start strip
]

# Placeholder "12" on the score plate. The plate is a flat horizontal gradient
# below its top bevel, so one interior column stretches across cleanly. Barely
# any feather here: the digits run right up to the edge of the usable area.
SCORE_TEXT = (62, 61, 115, 95)
SCORE_CLEAN_COLUMN = 117
SCORE_FEATHER = 2

# A soot to lift out of the art before it gets painted over, and the size it is
# drawn at in game. The proportion to a lane is kept the same as in the concept.
SOOT_BOX = (64, 193, 132, 238)
SOOT_OUT_H = 21
SOOT_COLORS = 16

# Caption balloon, replaced with foliage mirror-tiled from the strip beside it.
# Its top edge is pinned to where the start strip meets the grass, so no ramp is
# needed there; the bottom nearly touches the frame, so that ramp stays short.
CAPTION = (108, 544, 378, 654)
FOLIAGE_SRC = (364, 544, 412, 654)
CAPTION_FEATHER = (16, 0, 16, 3)


def feather_mask(width, height, feather=FEATHER):
    """Opaque in the middle, ramping to transparent over the edges.

    `feather` is either one width for all sides or (left, top, right, bottom).
    A side of 0 keeps a hard edge, which is what you want when the patch
    boundary already coincides with a real edge in the art.
    """
    if isinstance(feather, int):
        left = top = right = bottom = feather
    else:
        left, top, right, bottom = feather

    def ramp(i, span, near, far):
        a = i / near if near else 1.0
        b = (span - 1 - i) / far if far else 1.0
        return min(1.0, a, b)

    rows = [ramp(y, height, top, bottom) for y in range(height)]
    cols = [ramp(x, width, left, right) for x in range(width)]
    data = bytearray(width * height)
    for y in range(height):
        base = y * width
        fy = rows[y]
        for x in range(width):
            data[base + x] = int(255 * min(fy, cols[x]))
    return Image.frombytes("L", (width, height), bytes(data))


def erase_by_shift(img, x0, y0, x1, y1, dx):
    """Replace a rect with pixels from the same rows, dx pixels to the side."""
    patch = img.crop((x0 + dx, y0, x1 + dx, y1))
    img.paste(patch, (x0, y0), feather_mask(x1 - x0, y1 - y0))


def erase_by_column(img, x0, y0, x1, y1, src_x, feather=FEATHER):
    """Replace a rect by stretching one clean column across it."""
    column = img.crop((src_x, y0, src_x + 1, y1)).resize((x1 - x0, y1 - y0))
    img.paste(column, (x0, y0), feather_mask(x1 - x0, y1 - y0, feather))


def mirror_tile(img, box, src_box, feather=FEATHER):
    """Cover box with alternately mirrored copies of src_box."""
    x0, y0, x1, y1 = box
    strip = img.crop(src_box)
    flipped = strip.transpose(Image.FLIP_LEFT_RIGHT)
    tiled = Image.new("RGB", (x1 - x0, y1 - y0))
    x = 0
    i = 0
    while x < tiled.size[0]:
        tiled.paste(strip if i % 2 == 0 else flipped, (x, 0))
        x += strip.size[0]
        i += 1
    img.paste(tiled, (x0, y0), feather_mask(x1 - x0, y1 - y0, feather))


def is_dirt(color):
    r, g, b = color
    return r > 140 and g > 110 and b < 185 and r > b + 22


def extract_soot(panel):
    """Cut one soot out of a lane: the largest blob of non-dirt inside the box.

    Taking the largest connected component rather than every non-dirt pixel
    keeps stray pebbles and the lane's grass edging out of the sprite, and
    naturally keeps the soot's white eyes, which are enclosed by its body.
    """
    x0, y0, x1, y1 = SOOT_BOX
    w, h = x1 - x0, y1 - y0
    px = panel.load()
    solid = [[not is_dirt(px[x0 + x, y0 + y]) for x in range(w)] for y in range(h)]

    seen = [[False] * w for _ in range(h)]
    best = []
    for sy in range(h):
        for sx in range(w):
            if not solid[sy][sx] or seen[sy][sx]:
                continue
            blob = []
            stack = [(sx, sy)]
            seen[sy][sx] = True
            while stack:
                x, y = stack.pop()
                blob.append((x, y))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < w and 0 <= ny < h and solid[ny][nx] and not seen[ny][nx]:
                            seen[ny][nx] = True
                            stack.append((nx, ny))
            if len(blob) > len(best):
                best = blob

    cut = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out = cut.load()
    total = [0, 0, 0]
    for x, y in best:
        rgb = px[x0 + x, y0 + y]
        out[x, y] = rgb + (255,)
        for i in range(3):
            total[i] += rgb[i]
    mean = tuple(v // len(best) for v in total)

    xs = [p[0] for p in best]
    ys = [p[1] for p in best]
    cut = cut.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1))

    # Downscale color and alpha separately. Resizing RGBA directly would blend
    # the edge pixels toward transparent black and leave a dark halo, so the
    # transparent side is flooded with the soot's own average color first.
    scale = SOOT_OUT_H / cut.size[1]
    size = (max(1, round(cut.size[0] * scale)), SOOT_OUT_H)
    alpha = cut.getchannel("A")
    filled = Image.new("RGB", cut.size, mean)
    filled.paste(cut.convert("RGB"), (0, 0), alpha)

    small = filled.resize(size, Image.LANCZOS)
    small = small.quantize(colors=SOOT_COLORS, method=Image.MEDIANCUT, dither=Image.NONE)
    small = small.convert("RGBA")
    small.putalpha(alpha.resize(size, Image.LANCZOS).point(lambda v: 255 if v >= 128 else 0))
    return small


def load_panel(source_path):
    return Image.open(source_path).convert("RGB").crop(PANEL_BOX)


def build_board(source_path):
    """Return the cleaned panel at source resolution."""
    panel = load_panel(source_path)

    for x0, y0, x1, y1, dx in ERASE:
        erase_by_shift(panel, x0, y0, x1, y1, dx)
    erase_by_column(panel, *SCORE_TEXT, SCORE_CLEAN_COLUMN, SCORE_FEATHER)
    mirror_tile(panel, CAPTION, FOLIAGE_SRC, CAPTION_FEATHER)
    return panel


def build_background(source_path, colors=PALETTE_SIZE):
    board = build_board(source_path)
    board = board.crop((0, TITLE_H, board.size[0], board.size[1]))
    board = board.resize((OUT_W, OUT_H), Image.LANCZOS)
    # Reduce to a palette here rather than in encode_sheet: this is photographic
    # art with tens of thousands of distinct colors, and the encoder's fallback
    # reduction is an O(n^2) pairwise merge that would never finish. Median cut
    # gets the same result in a moment, and the preview then shows exactly the
    # colors the device will display.
    return board.quantize(colors=colors, method=Image.MEDIANCUT, dither=Image.NONE).convert("RGB")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", default="assets/catbus_cross_concept.png")
    ap.add_argument("-o", "--output", default="src/image_catbus_cross_bg.h")
    ap.add_argument("-n", "--name", default="catbus_cross_bg")
    ap.add_argument("--preview", default="assets/catbus_cross_bg_preview.png")
    ap.add_argument("--soot-output", default="src/sprite_catbus_soot.h")
    ap.add_argument("--soot-name", default="sprite_catbus_soot")
    ap.add_argument("--soot-preview", default="assets/sprite_catbus_soot_preview.png")
    ap.add_argument("--board-out", help="Write the cleaned panel at source resolution")
    ap.add_argument("--bpp", type=int, choices=[4, 8, 16], default=8)
    ap.add_argument("--colors", type=int, default=PALETTE_SIZE)
    ap.add_argument("--no-header", action="store_true", help="Preview only")
    args = ap.parse_args()

    if args.board_out:
        build_board(args.source).save(args.board_out)
        print(f"board   -> {args.board_out}")

    soot = extract_soot(load_panel(args.source))
    if args.soot_preview:
        soot.save(args.soot_preview)
        print(f"preview -> {args.soot_preview} ({soot.size[0]}x{soot.size[1]})")

    bg = build_background(args.source, args.colors)
    if args.preview:
        bg.save(args.preview)
        print(f"preview -> {args.preview}")
    if args.no_header:
        return

    sheet, regions = compose_sheet([soot], ["soot"], "horizontal")
    encoded = encode_sheet(sheet, regions, 4, None)
    write_asset_header(args.soot_output, args.soot_name, encoded)
    print(f"header  -> {args.soot_output}")
    print(preview_stats(encoded))

    sheet, regions = compose_sheet([bg.convert("RGBA")], ["board"], "horizontal")
    encoded = encode_sheet(sheet, regions, args.bpp, None)
    write_asset_header(args.output, args.name, encoded)
    print(f"header  -> {args.output} ({OUT_W}x{OUT_H})")
    print(preview_stats(encoded))


if __name__ == "__main__":
    main()
