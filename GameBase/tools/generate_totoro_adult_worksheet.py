#!/usr/bin/env python3
"""Build the hand-editable adult Totoro worksheet from the original render.

`assets/totoro_adult_parts_source.png` is a soft, dithered AI render at ~8x the
size the firmware needs: adjacent pixels almost never repeat, so it cannot be
edited as pixel art. This script bakes it down to
`assets/totoro_adult_worksheet.png` — clean flat pixels at native resolution, on
the fixed grid that `generate_totoro_adult_sheet.py` reads.

Run it once to seed the worksheet, then edit the PNG by hand and convert with
`generate_totoro_adult_sheet.py`. Rerunning without `--refresh` overwrites the
worksheet and throws hand edits away.

    python3 tools/generate_totoro_adult_worksheet.py            # seed from the render
    python3 tools/generate_totoro_adult_worksheet.py --refresh  # redraw band 2 only

`--refresh` keeps your edited poses and eyes and only redraws the pose+eye
reference matrix, so the composite you see matches what the firmware will draw.
"""

import argparse
import os
import sys
from collections import deque

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Pillow is required: pip install pillow", file=sys.stderr)
    sys.exit(1)

sys.path.insert(0, os.path.dirname(__file__))
import generate_totoro_adult_sheet as spec  # noqa: E402  (owns the grid geometry)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RENDER = os.path.join(ROOT, "assets", "totoro_adult_parts_source.png")
WORKSHEET = spec.WORKSHEET

# Room to the right of the grid for the band captions.
MARGIN = 210
GUIDE_COLOR = (188, 188, 188, 255)
LABEL_COLOR = (64, 64, 64, 255)

# Anything this dark that the background can reach is keyed out. Interior dark
# pixels (pupils, the belly nose dot) survive because the key floods inwards
# from the crop border instead of thresholding every pixel.
DARK_THRESHOLD = 40

# Tight bounding boxes (x0, y0, x1, y1) of each piece in the render.
POSE_BOXES = {
    1: (40, 89, 137, 214),
    2: (180, 89, 282, 212),
    3: (330, 88, 419, 212),
    4: (462, 100, 556, 215),
    5: (604, 89, 696, 211),
    6: (747, 89, 845, 212),
    7: (879, 93, 974, 211),
}

# The full eye pair, and the left eye on its own for the side-facing poses.
EYE_PAIR_BOXES = {
    1: (65, 300, 112, 317),   # open, round pupils (normal)
    2: (196, 301, 249, 313),  # shut, happy arches
    3: (325, 305, 379, 317),  # half lidded (content)
    4: (452, 305, 507, 317),  # small half lids (sad)
    5: (579, 300, 631, 317),  # sparkling diamonds (excited)
}

EYE_SINGLE_BOXES = {
    1: (65, 300, 81, 317),
    2: (196, 301, 216, 313),
    3: (325, 305, 343, 317),
    4: (452, 305, 470, 317),
    5: (579, 300, 597, 317),
}

# Centre of the eyes on each body, as (x, y) in pose-local render pixels.
# Recovered from the render's own 7x5 reference matrix: for all 35 cells the eye
# whites were located relative to the body box and scaled back up to pose size.
# `spec.EYE_OFFSETS` is the native-resolution descendant of this table and is
# what the firmware ships; these numbers only seed the initial layout.
EYE_ANCHORS = {
    1: (48, 37),
    2: (49, 41),
    3: (43, 38),
    4: (46, 42),
    5: (26, 35),
    6: (28, 35),
    7: (30, 36),
}


def crop(source, box):
    x0, y0, x1, y1 = box
    return source.crop((x0, y0, x1 + 1, y1 + 1))


def key_background(img):
    """Return an RGBA copy with the reachable dark border flooded to alpha 0."""
    img = img.convert("RGB")
    w, h = img.size
    px = img.load()
    dark = [[max(px[x, y]) <= DARK_THRESHOLD for x in range(w)] for y in range(h)]

    outside = [[False] * w for _ in range(h)]
    queue = deque()
    for x in range(w):
        for y in (0, h - 1):
            if dark[y][x] and not outside[y][x]:
                outside[y][x] = True
                queue.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if dark[y][x] and not outside[y][x]:
                outside[y][x] = True
                queue.append((x, y))
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < w and 0 <= ny < h and dark[ny][nx] and not outside[ny][nx]:
                outside[ny][nx] = True
                queue.append((nx, ny))

    out = Image.new("RGBA", (w, h))
    dst = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            dst[x, y] = (r, g, b, 0 if outside[y][x] else 255)
    return out


def downsample(img, target_w, target_h):
    """Box filter that averages colour over opaque pixels only.

    Plain RGBA resizing would drag the black background into every edge pixel;
    weighting by coverage keeps the outline its own colour. A destination pixel
    is opaque when at least half of the source box was.
    """
    w, h = img.size
    src = img.load()
    out = Image.new("RGBA", (target_w, target_h))
    dst = out.load()
    for ty in range(target_h):
        y0 = ty * h // target_h
        y1 = max(y0 + 1, (ty + 1) * h // target_h)
        for tx in range(target_w):
            x0 = tx * w // target_w
            x1 = max(x0 + 1, (tx + 1) * w // target_w)
            total = 0
            opaque = 0
            acc = [0, 0, 0]
            for y in range(y0, y1):
                for x in range(x0, x1):
                    r, g, b, a = src[x, y]
                    total += 1
                    if a:
                        opaque += 1
                        acc[0] += r
                        acc[1] += g
                        acc[2] += b
            if opaque * 2 >= total and opaque:
                dst[tx, ty] = (acc[0] // opaque, acc[1] // opaque, acc[2] // opaque, 255)
            else:
                dst[tx, ty] = (0, 0, 0, 0)
    return out


def flatten(images, colors):
    """Median-cut a group of pieces against one shared palette of flat colours.

    Groups are quantized apart because the eye whites are only ~3% of the art
    and sit close to the belly cream in RGB: pooled with the bodies they get
    merged into it and the eyes turn yellow.
    """
    opaque = [img.convert("RGB").getpixel((x, y))
              for img in images
              for y in range(img.height) for x in range(img.width)
              if img.getpixel((x, y))[3]]
    sample = Image.new("RGB", (len(opaque), 1))
    sample.putdata(opaque)
    palette = sample.quantize(colors=colors, method=Image.MEDIANCUT, dither=Image.NONE)

    out = []
    for img in images:
        alpha = img.getchannel("A")
        flat = img.convert("RGB").quantize(palette=palette, dither=Image.NONE).convert("RGBA")
        flat.putalpha(alpha)
        out.append(flat)
    return out


def extract(render, body_colors, eye_colors):
    """Bake the render down to native-resolution pose and eye cells."""
    poses = {i: key_background(crop(render, POSE_BOXES[i])) for i in spec.POSE_CELLS}
    pairs = {i: key_background(crop(render, EYE_PAIR_BOXES[i])) for i in sorted(EYE_PAIR_BOXES)}
    singles = {i: key_background(crop(render, EYE_SINGLE_BOXES[i]))
               for i in sorted(EYE_SINGLE_BOXES)}

    # One factor for every piece so bodies and faces stay in proportion.
    factor = spec.CELL_H / max(img.height for img in poses.values())

    def resize(img):
        return downsample(img, max(1, round(img.width * factor)),
                          max(1, round(img.height * factor)))

    order = list(spec.POSE_CELLS)
    bodies = dict(zip(order, flatten([resize(poses[i]) for i in order], body_colors)))
    eyes = flatten([resize(pairs[i]) for i in sorted(pairs)]
                   + [resize(singles[i]) for i in sorted(singles)], eye_colors)

    # Report where the anchors land on the grid, so spec.EYE_OFFSETS can be
    # checked after the render or the cell size changes.
    offsets = {}
    for pose in order:
        img = bodies[pose]
        left = (spec.CELL_W - img.width) // 2
        top = spec.CELL_H - img.height
        ax, ay = EYE_ANCHORS[pose]
        offsets[pose] = (left + round(ax * factor) - spec.EYE_W // 2,
                         top + round(ay * factor) - spec.EYE_H // 2)
    return bodies, eyes, offsets


def body_cell(img):
    """Pad a pose to its cell: centred horizontally, sitting on the floor."""
    cell = Image.new("RGBA", (spec.CELL_W, spec.CELL_H), (0, 0, 0, 0))
    cell.alpha_composite(img, ((spec.CELL_W - img.width) // 2, spec.CELL_H - img.height))
    return cell


def eye_cell(img):
    """Pad an eye piece to its cell, centred on both axes."""
    cell = Image.new("RGBA", (spec.EYE_W, spec.EYE_H), (0, 0, 0, 0))
    cell.alpha_composite(img, ((spec.EYE_W - img.width) // 2,
                               (spec.EYE_H - img.height) // 2))
    return cell


def draw_matrix(sheet, bodies, eyes):
    """Replace band 2. Cells are cleared first so --refresh cannot leave a
    ghost of the previous composite showing through the new one."""
    blank = Image.new("RGBA", (spec.CELL_W, spec.CELL_H), (0, 0, 0, 0))
    for variant in range(spec.EYE_VARIANTS):
        for col, pose in enumerate(spec.POSE_CELLS):
            box = spec.matrix_cell_box(col, variant)
            sheet.paste(blank, box[:2])
            sheet.alpha_composite(spec.compose(bodies, eyes, pose, variant), box[:2])


def draw_guides(sheet):
    """Gutter lines and captions. Both live outside every cell, so the
    converter never sees them and they are safe to repaint or ignore."""
    draw = ImageDraw.Draw(sheet)
    grid_right = spec.matrix_cell_box(spec.POSE_COUNT - 1, 0)[2]
    # Wipe the caption margin: the text is anti-aliased, so drawing it over an
    # older copy of itself would darken it a little on every --refresh.
    sheet.paste(Image.new("RGBA", (sheet.width - grid_right, sheet.height), (0, 0, 0, 0)),
                (grid_right, 0))

    def gutter_v(x, y0, y1):
        draw.line([(x, y0), (x, y1)], fill=GUIDE_COLOR)

    def gutter_h(y, x0, x1):
        draw.line([(x0, y), (x1, y)], fill=GUIDE_COLOR)

    pose_band = spec.pose_cell_box(0)
    eye_band = spec.eye_cell_box(0)
    matrix_bottom = spec.matrix_cell_box(0, spec.EYE_VARIANTS - 1)[3]
    for i in range(spec.POSE_COUNT):
        x = spec.pose_cell_box(i)[2]
        gutter_v(x, pose_band[1], pose_band[3])
        gutter_v(x, eye_band[3] + 1, matrix_bottom)
    for i in range(2 * spec.EYE_VARIANTS):
        gutter_v(spec.eye_cell_box(i)[2], eye_band[1], eye_band[3])
    gutter_h(pose_band[3], 0, grid_right)
    gutter_h(eye_band[3], 0, grid_right)
    for row in range(spec.EYE_VARIANTS - 1):
        gutter_h(spec.matrix_cell_box(0, row)[3], 0, grid_right)

    font = ImageFont.load_default()
    captions = [
        (pose_band[1] + spec.CELL_H // 2 - 4, "POSE 1 - 7"),
        (eye_band[1] + 2, "EYES 1-5 PAIR, THEN 1-5 SIDE"),
        (spec.matrix_cell_box(0, 0)[1] + 4, "POSE + EYE REFERENCE"),
    ]
    for y, text in captions:
        draw.text((grid_right + 8, y), text, fill=LABEL_COLOR, font=font)
    draw.text((grid_right + 8, spec.matrix_cell_box(0, 0)[1] + 18),
              f"cell {spec.CELL_W}x{spec.CELL_H}, eye {spec.EYE_W}x{spec.EYE_H}, 1px gutter",
              fill=LABEL_COLOR, font=font)
    draw.text((grid_right + 8, spec.matrix_cell_box(0, 0)[1] + 30),
              "edit bands 1-2; this band is generated", fill=LABEL_COLOR, font=font)


def build(bodies, eyes):
    width, height = spec.worksheet_size()
    sheet = Image.new("RGBA", (width + MARGIN, height), (0, 0, 0, 0))
    # Pad to whole cells first, so band 2 here matches what --refresh draws
    # after reading those same cells back.
    body_cells = {pose: body_cell(img) for pose, img in bodies.items()}
    eye_cells = [eye_cell(img) for img in eyes]
    for i, pose in enumerate(spec.POSE_CELLS):
        sheet.alpha_composite(body_cells[pose], spec.pose_cell_box(i)[:2])
    for i, cell in enumerate(eye_cells):
        sheet.alpha_composite(cell, spec.eye_cell_box(i)[:2])
    draw_matrix(sheet, body_cells, eye_cells)
    draw_guides(sheet)
    return sheet


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--refresh", action="store_true",
                        help="Keep the edited cells; only redraw the reference matrix")
    parser.add_argument("--render", default=RENDER)
    parser.add_argument("-o", "--output", default=WORKSHEET)
    parser.add_argument("--body-colors", type=int, default=10,
                        help="Palette size for the pose cells")
    parser.add_argument("--eye-colors", type=int, default=4,
                        help="Palette size for the eye cells; below 4 the pupil greys out")
    args = parser.parse_args()
    if args.body_colors + args.eye_colors > 16:
        raise SystemExit("The two palettes share one 16-entry 4-bpp table")

    if args.refresh:
        bodies, eyes = spec.read_worksheet(args.output)
        sheet = Image.open(args.output).convert("RGBA")
        draw_matrix(sheet, bodies, eyes)
        draw_guides(sheet)
        sheet.save(args.output)
        print(f"Refreshed the reference matrix in {args.output}")
        return

    render = Image.open(args.render).convert("RGB")
    bodies, eyes, offsets = extract(render, args.body_colors, args.eye_colors)
    build(bodies, eyes).save(args.output)
    print(f"Wrote {args.output}")
    print(f"  cell {spec.CELL_W}x{spec.CELL_H}, eye {spec.EYE_W}x{spec.EYE_H}, "
          f"{spec.POSE_COUNT} poses + {2 * spec.EYE_VARIANTS} eye cells")
    drift = {p: o for p, o in offsets.items() if o != spec.EYE_OFFSETS[p]}
    if drift:
        print("  anchors moved; update EYE_OFFSETS in generate_totoro_adult_sheet.py:")
        for pose, offset in sorted(drift.items()):
            print(f"    {pose}: {offset}  (currently {spec.EYE_OFFSETS[pose]})")
    else:
        print("  eye offsets match EYE_OFFSETS in generate_totoro_adult_sheet.py")


if __name__ == "__main__":
    main()
