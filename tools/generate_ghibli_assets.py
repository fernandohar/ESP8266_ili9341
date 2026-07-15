#!/usr/bin/env python3
"""
Generate Ghibli-inspired 8-bit pixel art for the Totoro Home virtual pet console.
Output: PNG files in art/backgrounds and art/sprites (240x320 background, modular furniture).
"""

from PIL import Image, ImageDraw
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BG_DIR = os.path.join(ROOT, "art", "backgrounds")
SPRITE_DIR = os.path.join(ROOT, "art", "sprites")

# Ghibli-inspired palette (RGB)
PALETTE = {
    "transparent": (0, 0, 0, 0),
    "wall_dark": (58, 72, 46),
    "wall_mid": (90, 105, 68),
    "wall_light": (118, 132, 88),
    "ceiling": (72, 86, 58),
    "beam": (61, 42, 26),
    "beam_light": (92, 62, 38),
    "tatami_dark": (107, 94, 58),
    "tatami_mid": (138, 122, 76),
    "tatami_light": (168, 150, 92),
    "tatami_edge": (82, 72, 44),
    "wood_dark": (45, 30, 18),
    "wood_mid": (72, 48, 28),
    "wood_light": (110, 74, 44),
    "shoji_glow": (245, 228, 176),
    "shoji_mid": (220, 200, 150),
    "shoji_frame": (90, 74, 48),
    "shoji_shadow": (160, 140, 100),
    "lamp_white": (240, 238, 228),
    "lamp_warm": (255, 232, 160),
    "lamp_string": (200, 195, 180),
    "warm_glow": (255, 220, 140),
    "floor_shadow": (55, 48, 32),
    "green_leaf": (72, 120, 58),
    "green_leaf_light": (110, 160, 72),
    "gray_body": (140, 145, 150),
    "gray_dark": (100, 105, 112),
    "gray_light": (175, 180, 188),
    "white_belly": (230, 232, 238),
    "eye_black": (30, 30, 35),
    "eye_white": (250, 250, 252),
    "pink_nose": (180, 140, 140),
    "acorn_brown": (120, 80, 40),
    "acorn_cap": (90, 60, 30),
    "cushion_red": (180, 90, 80),
    "cushion_shadow": (130, 60, 55),
    "plant_pot": (140, 100, 70),
    "plant_green": (60, 110, 50),
    "sky_blue": (120, 175, 210),
    "garden_green": (70, 130, 60),
    "garden_light": (100, 165, 80),
    "curtain_blue": (140, 175, 200),
    "floor_wood": (90, 65, 40),
    "floor_wood_light": (130, 95, 60),
    "sunlight": (255, 245, 200),
}


def rgb(c):
    return PALETTE[c][:3]


def make_bg_washitsu():
    """Traditional Japanese room — evening lamp glow, tatami, shoji doors."""
    w, h = 240, 320
    img = Image.new("RGB", (w, h), rgb("wall_dark"))
    px = img.load()

    # Ceiling gradient
    for y in range(0, 55):
        t = y / 55
        for x in range(w):
            r = int(58 + t * 20)
            g = int(72 + t * 18)
            b = int(46 + t * 14)
            px[x, y] = (r, g, b)

    # Ceiling beam
    for y in range(38, 46):
        for x in range(w):
            px[x, y] = rgb("beam") if y < 42 else rgb("beam_light")

    # Walls with subtle gradient
    for y in range(55, 200):
        t = (y - 55) / 145
        for x in range(w):
            r = int(90 - t * 15)
            g = int(105 - t * 12)
            b = int(68 - t * 10)
            px[x, y] = (r, g, b)

    # Shoji doors (center back wall) — warm evening glow
    shoji_x0, shoji_x1 = 50, 190
    shoji_y0, shoji_y1 = 70, 195
    for y in range(shoji_y0, shoji_y1):
        for x in range(shoji_x0, shoji_x1):
            # Frame edges
            if x < shoji_x0 + 6 or x > shoji_x1 - 7 or y < shoji_y0 + 4 or y > shoji_y1 - 5:
                px[x, y] = rgb("shoji_frame")
            elif (x - shoji_x0) % 24 < 3 or (y - shoji_y0) % 28 < 3:
                px[x, y] = rgb("shoji_frame")
            else:
                # Warm glow from lamp — brighter near center
                cx, cy = (shoji_x0 + shoji_x1) // 2, (shoji_y0 + shoji_y1) // 2
                dist = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
                glow = max(0, 1 - dist / 90)
                base = rgb("shoji_mid")
                warm = rgb("shoji_glow")
                px[x, y] = tuple(
                    int(base[i] + glow * (warm[i] - base[i])) for i in range(3)
                )

    # Small window cutouts in shoji (darker beyond)
    for wx, wy in [(80, 110), (140, 110), (110, 150)]:
        for dy in range(12):
            for dx in range(16):
                px[wx + dx, wy + dy] = rgb("shoji_shadow")

    # Pendant lamp
    lamp_cx = 120
    for y in range(46, 68):
        for x in range(lamp_cx - 2, lamp_cx + 3):
            px[x, y] = rgb("lamp_string")
    for y in range(68, 82):
        for x in range(lamp_cx - 14, lamp_cx + 15):
            if (x - lamp_cx) ** 2 + (y - 74) ** 2 < 196:
                px[x, y] = rgb("lamp_white")
    # Warm light cone on wall/shoji
    for y in range(82, 200):
        for x in range(60, 180):
            dist = abs(x - lamp_cx) + (y - 82) * 0.3
            if dist < 80:
                glow = max(0, 1 - dist / 80) * 0.15
                old = px[x, y]
                warm = rgb("warm_glow")
                px[x, y] = tuple(
                    min(255, int(old[i] + glow * warm[i])) for i in range(3)
                )

    # Wall clock (pixel detail)
    for y in range(58, 72):
        for x in range(28, 42):
            if (x - 35) ** 2 + (y - 65) ** 2 < 49:
                px[x, y] = rgb("wood_mid")

    # Calendar scroll on left wall
    for y in range(95, 130):
        for x in range(12, 22):
            px[x, y] = rgb("shoji_glow")

    # Tatami floor
    floor_y = 200
    for y in range(floor_y, h):
        row = (y - floor_y) // 20
        for x in range(w):
            col = x // 40
            base = "tatami_mid" if (row + col) % 2 == 0 else "tatami_light"
            edge = (y - floor_y) % 20 < 2 or x % 40 < 2
            px[x, y] = rgb("tatami_edge") if edge else rgb(base)

    # Floor shadow at wall base
    for y in range(floor_y, floor_y + 8):
        for x in range(w):
            old = px[x, y]
            px[x, y] = tuple(int(c * 0.85) for c in old)

    # Built-in cabinet silhouette (right wall, non-interactive bg)
    for y in range(120, 200):
        for x in range(195, 235):
            if x < 200 or y > 195:
                px[x, y] = rgb("wood_dark")
            else:
                px[x, y] = rgb("wood_mid")

    # Tansu silhouette (left wall, non-interactive bg)
    for y in range(130, 200):
        for x in range(5, 45):
            px[x, y] = rgb("wood_mid")
            if (y - 130) % 22 < 3:
                px[x, y] = rgb("wood_dark")
    # Drawer handles
    for dy in [8, 30, 52]:
        for x in range(20, 30):
            px[x, 130 + dy] = rgb("wood_light")

    # Walkable floor zone marker (subtle — center clear area)
    for y in range(230, 310):
        for x in range(50, 190):
            pass  # kept clear for character movement

    return img


def make_bg_garden_room():
    """Sunlit room opening to garden — second room variant."""
    w, h = 240, 320
    img = Image.new("RGB", (w, h), rgb("wall_mid"))
    px = img.load()

    # Sky / garden through open doors (top half)
    for y in range(0, 180):
        for x in range(40, 200):
            t = y / 180
            if t < 0.35:
                px[x, y] = rgb("sky_blue")
            elif t < 0.55:
                px[x, y] = rgb("garden_light")
            else:
                px[x, y] = rgb("garden_green")

    # Palm tree silhouette
    for y in range(40, 160):
        for x in range(155, 175):
            if (x + y) % 4 < 2:
                px[x, y] = rgb("plant_green")

    # Door frames
    for y in range(0, 200):
        for x in range(35, 45):
            px[x, y] = rgb("curtain_blue")
        for x in range(195, 205):
            px[x, y] = rgb("curtain_blue")
    for y in range(0, 8):
        for x in range(35, 205):
            px[x, y] = rgb("wood_mid")

    # Interior walls (sides)
    for y in range(0, h):
        for x in range(0, 35):
            px[x, y] = rgb("wall_light")
        for x in range(205, w):
            px[x, y] = rgb("wall_light")

    # Wooden floor with sunlight patches
    for y in range(180, h):
        for x in range(w):
            base = "floor_wood" if (x // 16 + y // 12) % 2 == 0 else "floor_wood_light"
            px[x, y] = rgb(base)
    # Sunlight rectangles
    for y in range(190, 310):
        for x in range(60, 180):
            if 60 < x < 120 and y < 260:
                old = px[x, y]
                sun = rgb("sunlight")
                px[x, y] = tuple(
                    min(255, int(old[i] * 0.6 + sun[i] * 0.4)) for i in range(3)
                )

    return img


def make_sprite(name, w, h, draw_fn):
    img = Image.new("RGBA", (w, h), PALETTE["transparent"])
    draw = ImageDraw.Draw(img)
    draw_fn(draw, w, h)
    path = os.path.join(SPRITE_DIR, f"{name}.png")
    img.save(path)
    print(f"  {path}")
    return img


def draw_forest_spirit(draw, w, h):
    """Ghibli-inspired forest spirit (original design, not trademarked character)."""
    # Body
    draw.ellipse([6, 14, 34, 44], fill=rgb("gray_body"), outline=rgb("gray_dark"))
    draw.ellipse([10, 28, 30, 42], fill=rgb("white_belly"))
    # Ears
    draw.polygon([(10, 16), (6, 4), (16, 14)], fill=rgb("gray_body"))
    draw.polygon([(30, 16), (34, 4), (24, 14)], fill=rgb("gray_body"))
    # Eyes
    draw.ellipse([12, 20, 18, 28], fill=rgb("eye_white"))
    draw.ellipse([22, 20, 28, 28], fill=rgb("eye_white"))
    draw.ellipse([14, 22, 16, 26], fill=rgb("eye_black"))
    draw.ellipse([24, 22, 26, 26], fill=rgb("eye_black"))
    # Nose
    draw.ellipse([18, 26, 22, 30], fill=rgb("pink_nose"))
    # Whiskers
    draw.line([(8, 28), (2, 26)], fill=rgb("gray_dark"), width=1)
    draw.line([(8, 30), (2, 32)], fill=rgb("gray_dark"), width=1)
    draw.line([(32, 28), (38, 26)], fill=rgb("gray_dark"), width=1)
    draw.line([(32, 30), (38, 32)], fill=rgb("gray_dark"), width=1)
    # Leaf on head
    draw.polygon([(18, 8), (14, 2), (22, 2)], fill=rgb("green_leaf"))
    draw.polygon([(20, 6), (26, 0), (28, 8)], fill=rgb("green_leaf_light"))
    # Feet
    draw.ellipse([10, 40, 18, 46], fill=rgb("gray_dark"))
    draw.ellipse([22, 40, 30, 46], fill=rgb("gray_dark"))


def draw_forest_spirit_sleep(draw, w, h):
    draw.ellipse([4, 20, 36, 40], fill=rgb("gray_body"), outline=rgb("gray_dark"))
    draw.arc([10, 24, 30, 34], 0, 180, fill=rgb("gray_dark"), width=2)
    draw.ellipse([8, 34, 16, 40], fill=rgb("gray_dark"))
    draw.ellipse([24, 34, 32, 40], fill=rgb("gray_dark"))
    # Zzz
    draw.text((28, 8), "z", fill=rgb("gray_light"))


def draw_chabudai(draw, w, h):
    # Low table
    draw.rectangle([4, 10, 44, 16], fill=rgb("wood_mid"), outline=rgb("wood_dark"))
    for lx in [8, 36]:
        draw.rectangle([lx, 16, lx + 4, 22], fill=rgb("wood_dark"))
    # Tea cups
    draw.ellipse([12, 4, 18, 10], fill=rgb("shoji_glow"), outline=rgb("wood_light"))
    draw.ellipse([28, 4, 34, 10], fill=rgb("shoji_glow"), outline=rgb("wood_light"))
    # Kettle
    draw.ellipse([20, 2, 28, 10], fill=rgb("eye_black"))


def draw_tansu(draw, w, h):
    draw.rectangle([2, 0, 30, 46], fill=rgb("wood_mid"), outline=rgb("wood_dark"))
    for dy in [0, 15, 30]:
        draw.line([(2, dy), (30, dy)], fill=rgb("wood_dark"), width=2)
        draw.ellipse([12, dy + 5, 20, dy + 11], fill=rgb("wood_light"))


def draw_cushion(draw, w, h):
    draw.ellipse([2, 4, 22, 14], fill=rgb("cushion_red"), outline=rgb("cushion_shadow"))
    draw.arc([4, 2, 20, 10], 180, 360, fill=rgb("cushion_shadow"))


def draw_plant(draw, w, h):
    draw.rectangle([6, 18, 14, 28], fill=rgb("plant_pot"))
    draw.ellipse([4, 14, 16, 22], fill=rgb("plant_green"))
    draw.ellipse([2, 6, 10, 16], fill=rgb("green_leaf"))
    draw.ellipse([10, 4, 18, 14], fill=rgb("green_leaf_light"))


def draw_acorn(draw, w, h):
    draw.polygon([(4, 2), (2, 5), (6, 5)], fill=rgb("acorn_cap"))
    draw.ellipse([2, 4, 6, 8], fill=rgb("acorn_brown"))


def draw_lamp_small(draw, w, h):
    draw.line([8, 0, 8, 8], fill=rgb("lamp_string"), width=1)
    draw.ellipse([2, 8, 14, 20], fill=rgb("lamp_white"), outline=rgb("wood_light"))


def make_tatami_tile():
    img = Image.new("RGB", (40, 20), rgb("tatami_mid"))
    px = img.load()
    for x in range(40):
        px[x, 0] = rgb("tatami_edge")
        px[x, 19] = rgb("tatami_edge")
    for y in range(20):
        px[0, y] = rgb("tatami_edge")
        px[39, y] = rgb("tatami_edge")
    path = os.path.join(SPRITE_DIR, "tatami_tile.png")
    img.save(path)
    print(f"  {path}")


def main():
    os.makedirs(BG_DIR, exist_ok=True)
    os.makedirs(SPRITE_DIR, exist_ok=True)

    print("Generating backgrounds...")
    make_bg_washitsu().save(os.path.join(BG_DIR, "washitsu_evening.png"))
    print(f"  {BG_DIR}/washitsu_evening.png")
    make_bg_garden_room().save(os.path.join(BG_DIR, "garden_room_day.png"))
    print(f"  {BG_DIR}/garden_room_day.png")

    print("Generating sprites...")
    make_sprite("forest_spirit", 40, 48, draw_forest_spirit)
    make_sprite("forest_spirit_sleep", 40, 44, draw_forest_spirit_sleep)
    make_sprite("chabudai", 48, 24, draw_chabudai)
    make_sprite("tansu", 32, 48, draw_tansu)
    make_sprite("cushion", 24, 16, draw_cushion)
    make_sprite("plant", 20, 28, draw_plant)
    make_sprite("acorn", 8, 8, draw_acorn)
    make_sprite("lamp", 16, 22, draw_lamp_small)
    make_tatami_tile()

    print("Done.")


if __name__ == "__main__":
    main()
