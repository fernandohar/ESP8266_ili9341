#!/usr/bin/env python3
"""Build the Totoro pet sprite sheet from the layered pose/eye source art.

`assets/totoro_parts_source.png` is a worksheet, not a sheet: the top row holds
7 bodies with blank faces, the second row holds 5 detachable eye/mouth strips,
and the rows below are the artist's reference of the two combined.

The two stay separate here too. Baking every body x every face would cost five
copies of the body art; instead the sheet keeps 7 body cells plus 5 eye cells,
and the scene hangs the eye on the body as an Attachment. Adding a face later
costs one small cell.

Layout rules that the runtime depends on:

* Every body cell is `CELL_W` wide with the eye centre exactly in the middle,
  so mirroring a cell for the walk-right animation leaves the face in place.
* The eye cell is centred in the body cell too, so the same attach offset works
  flipped or not. Only the vertical offset varies per pose, and the generator
  emits it as a table.
* Bodies are bottom-aligned in the cell, so feet stay on the ground line and
  the head bobs naturally through the walk cycle.

    python3 tools/generate_totoro_pet_sheet.py
    python3 tools/generate_totoro_pet_sheet.py --scale 2 --bpp 4
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install pillow", file=sys.stderr)
    sys.exit(1)

sys.path.insert(0, os.path.dirname(__file__))
from sprite_encoding import encode_sheet, preview_stats, write_asset_header  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, "assets", "totoro_parts_source.png")
OUTPUT = os.path.join(ROOT, "src", "sprite_totoro_pet.h")
PREVIEW = os.path.join(ROOT, "assets", "sprite_totoro_pet_preview.png")
NAME = "sprite_totoro_pet"

# Tight bounding boxes (x0, y0, x1, y1) of each piece in the source worksheet,
# with the meaning the artist assigned to each body.
POSE_BOXES = {
    1: (1, 3, 39, 44),      # standing
    2: (46, 4, 85, 45),     # dancing / walking toward the viewer
    3: (102, 5, 132, 44),   # hungry (paws clutching the belly)
    4: (195, 6, 234, 46),   # sitting, facing front
    5: (249, 7, 286, 48),   # walking left, frame A
    6: (301, 7, 331, 49),   # walking left, frame B
    7: (350, 10, 384, 49),  # sitting, facing left
}

EYE_BOXES = {
    1: (10, 82, 29, 88),    # shut, flat mouth
    2: (53, 80, 74, 90),    # shut, laughing open mouth
    3: (106, 82, 127, 89),  # wide open, neutral
    4: (156, 82, 176, 88),  # shut, wavy sad mouth
    5: (203, 81, 223, 88),  # wide open, smiling
}

# Where the eye strip sits on each body, as (centre x, top y) in pose-local
# pixels. Recovered from the artist's combined reference rows, which the eyes
# match exactly at these anchors.
EYE_ANCHORS = {
    1: (20, 16),
    2: (20, 16),
    3: (16, 13),
    4: (21, 16),
    5: (16, 15),
    6: (13, 15),
    7: (13, 15),
}

# One sheet cell per body, in cell order.
POSE_CELLS = [1, 2, 3, 4, 5, 6, 7]

# The region table the scene indexes into. Entries 0..5 keep the ordering the
# junior/adult sheets already use (TOTORO_RGN_* in Scene_PetTotoro) so sheets
# stay interchangeable; the rest are specific to this art. Several regions
# deliberately alias the same cell.
BODY_REGIONS = [
    ("walk_a", 5),
    ("walk_b", 6),
    ("sit", 4),
    ("stand", 1),
    ("sleep", 4),
    ("blink", 1),
    ("hungry", 3),
    ("dance", 2),
    ("sit_side", 7),
]


def crop(source, box):
    x0, y0, x1, y1 = box
    return source.crop((x0, y0, x1 + 1, y1 + 1))


def eye_left_in_cell(eye_width, cell_width):
    """Mirror the artist's own centring rule, which biases odd widths left."""
    return cell_width // 2 - (eye_width + 1) // 2


def build_sheet(source, scale):
    bodies = {i: crop(source, POSE_BOXES[i]) for i in POSE_CELLS}
    eyes = {i: crop(source, EYE_BOXES[i]) for i in sorted(EYE_BOXES)}

    # Body cell: widest half-span on either side of the eye centre, doubled so
    # the eye centre lands on the cell's midline and mirroring is a no-op.
    half = max(
        max(EYE_ANCHORS[i][0] for i in POSE_CELLS),
        max(bodies[i].width - EYE_ANCHORS[i][0] for i in POSE_CELLS),
    )
    cell_w = half * 2
    cell_h = max(img.height for img in bodies.values())

    eye_cell_w = max(img.width for img in eyes.values())
    eye_cell_h = max(img.height for img in eyes.values())
    if eye_cell_w % 2:
        eye_cell_w += 1

    sheet_w = max(cell_w * len(POSE_CELLS), eye_cell_w * len(eyes))
    sheet_h = cell_h + eye_cell_h
    sheet = Image.new("RGBA", (sheet_w * scale, sheet_h * scale), (0, 0, 0, 0))

    def paste(img, x, y):
        if scale != 1:
            img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        sheet.alpha_composite(img, (x * scale, y * scale))

    cell_x = {}
    eye_offset_y = {}
    for i, pose in enumerate(POSE_CELLS):
        img = bodies[pose]
        top = cell_h - img.height  # bottom-align: feet on the cell floor
        paste(img, i * cell_w + half - EYE_ANCHORS[pose][0], top)
        cell_x[pose] = i * cell_w
        eye_offset_y[pose] = top + EYE_ANCHORS[pose][1]

    eye_x = {}
    for i, eye in enumerate(sorted(eyes)):
        img = eyes[eye]
        paste(img, i * eye_cell_w + eye_left_in_cell(img.width, eye_cell_w), cell_h)
        eye_x[eye] = i * eye_cell_w

    regions = [
        (label, cell_x[pose] * scale, 0, cell_w * scale, cell_h * scale)
        for label, pose in BODY_REGIONS
    ]
    regions += [
        (f"eye{eye}", x * scale, cell_h * scale, eye_cell_w * scale, eye_cell_h * scale)
        for eye, x in sorted(eye_x.items())
    ]

    meta = {
        "eye_offset_x": (cell_w // 2 - eye_cell_w // 2) * scale,
        "eye_offset_y": [eye_offset_y[pose] * scale for _label, pose in BODY_REGIONS],
        "cell_w": cell_w * scale,
        "cell_h": cell_h * scale,
    }
    return sheet, regions, meta


def append_metadata(path, name, meta, body_count, eye_count):
    """Bolt the attach-offset tables onto the shared asset header."""
    upper = name.upper()
    offsets = ", ".join(str(v) for v in meta["eye_offset_y"])
    extra = f"""
// --- Runtime face compositing -------------------------------------------
// The first {body_count} regions are bodies; the rest are eye/mouth strips.
// Hang region ({upper}_EYE_REGION + variant) on the body at this offset.
#define {upper}_BODY_REGION_COUNT {body_count}
#define {upper}_EYE_REGION {body_count}
#define {upper}_EYE_VARIANTS {eye_count}
// Cells are mirror-symmetric about the eye centre, so this holds when flipped.
#define {upper}_EYE_OFFSET_X {meta['eye_offset_x']}

static const uint8_t {name}EyeOffsetY[{body_count}] PROGMEM = {{
  {offsets}
}};
"""
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    head, sep, tail = text.rpartition("#endif")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(head + extra + "\n" + sep + tail)


def save_preview(path, sheet, regions, meta, body_count):
    """Draw the runtime composite: every body wearing every face."""
    look = {label: (x, y, w, h) for label, x, y, w, h in regions}
    bodies = [label for label, *_ in regions[:body_count]]
    eyes = [label for label, *_ in regions[body_count:]]
    cw, ch = meta["cell_w"], meta["cell_h"]
    out = Image.new("RGBA", (cw * len(bodies), ch * len(eyes)), (86, 132, 86, 255))
    for row, eye in enumerate(eyes):
        ex, ey, ew, eh = look[eye]
        eye_img = sheet.crop((ex, ey, ex + ew, ey + eh))
        for col, body in enumerate(bodies):
            bx, by, bw, bh = look[body]
            cell = sheet.crop((bx, by, bx + bw, by + bh)).copy()
            cell.alpha_composite(eye_img, (meta["eye_offset_x"], meta["eye_offset_y"][col]))
            out.alpha_composite(cell, (col * cw, row * ch))
    out.convert("RGB").save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scale", type=int, default=2,
                        help="Nearest-neighbour upscale of the source art")
    parser.add_argument("--bpp", type=int, default=4, choices=[4, 8, 16])
    parser.add_argument("-o", "--output", default=OUTPUT)
    parser.add_argument("-n", "--name", default=NAME)
    parser.add_argument("--preview-out", default=PREVIEW)
    args = parser.parse_args()

    source = Image.open(SOURCE).convert("RGBA")
    sheet, regions, meta = build_sheet(source, args.scale)
    if sheet.width % 2:
        raise SystemExit("Sheet width must be even: 4-bpp rows are one nibble stream")

    encoded = encode_sheet(sheet, regions, args.bpp, None)
    write_asset_header(args.output, args.name, encoded)
    body_count = len(BODY_REGIONS)
    append_metadata(args.output, args.name, meta, body_count, len(EYE_BOXES))

    print(f"Wrote {args.output}")
    print(f"  {len(POSE_CELLS)} body cells + {len(EYE_BOXES)} eye cells, "
          f"{len(regions)} regions, cell {meta['cell_w']}x{meta['cell_h']}")
    print(preview_stats(encoded))
    if args.preview_out:
        save_preview(args.preview_out, sheet, regions, meta, body_count)
        print(f"Preview PNG: {args.preview_out}")


if __name__ == "__main__":
    main()
