#!/usr/bin/env python3
"""Generate and verify GameBase/diagram.json for the Wokwi simulator.

The ILI9341 board carries its whole pin header on the bottom edge while the
screen occupies the top, so wires routed straight at it from the side end up
drawn across the display. This lays the ESP32 out underneath the panel and
routes every wire through a horizontal lane in the gap between the two, then
checks that no wire segment lands on the screen.

Run: python3 tools/gen_wokwi_diagram.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

MM = 96 / 25.4  # Wokwi renders 1mm at 96dpi

OUT = Path(__file__).resolve().parent.parent / "diagram.json"

# ---------------------------------------------------------------- geometry --
# Board pin coordinates are in mm (wokwi-boards board.json); element pin
# coordinates are in px (wokwi-elements pinInfo).

ESP_PINS_MM = {
    "3V3": (1.22, 7.62), "EN": (1.22, 10.16), "VP": (1.22, 12.7),
    "VN": (1.22, 15.24), "34": (1.22, 17.78), "35": (1.22, 20.32),
    "32": (1.22, 22.86), "33": (1.22, 25.40), "25": (1.22, 27.94),
    "26": (1.22, 30.48), "27": (1.22, 33.02), "14": (1.22, 35.56),
    "12": (1.22, 38.10), "GND.1": (1.22, 40.64), "13": (1.22, 43.18),
    "D2": (1.22, 45.72), "D3": (1.22, 48.26), "CMD": (1.22, 50.80),
    "5V": (1.22, 53.34),
    "GND.2": (26.66, 7.62), "23": (26.66, 10.16), "22": (26.66, 12.7),
    "TX": (26.66, 15.24), "RX": (26.66, 17.78), "21": (26.66, 20.32),
    "GND.3": (26.66, 22.86), "19": (26.66, 25.40), "18": (26.66, 27.94),
    "5": (26.66, 30.48), "17": (26.66, 33.02), "16": (26.66, 35.56),
    "4": (26.66, 38.10), "0": (26.66, 40.64), "2": (26.66, 43.18),
    "15": (26.66, 45.72), "D1": (26.66, 48.26), "D0": (26.66, 50.80),
    "CLK": (26.66, 53.34),
}

TFT_PINS_MM = {
    "VCC": (10.26, 76), "GND": (12.8, 76), "CS": (15.34, 76),
    "RST": (17.88, 76), "D/C": (20.42, 76), "MOSI": (22.96, 76),
    "SCK": (25.5, 76), "LED": (28.04, 76), "MISO": (30.58, 76),
    "SCL": (33.12, 76), "SDA": (35.66, 76),
}

BUZZER_PINS_PX = {"1": (27, 84), "2": (37, 84)}
BUTTON_PINS_PX = {"1.l": (0, 13), "2.l": (0, 32), "1.r": (67, 13), "2.r": (67, 32)}

# ------------------------------------------------------------------ layout --
# Screen on top, ESP32 directly below it, buttons to the lower left, buzzer to
# the right. Everything the wires need to cross is below the panel.

TFT_POS = (200, 0)
ESP_POS = (235, 420)
BUZZER_POS = (440, 430)
BTN_POS = {"btnLeft": (50, 560), "btnHome": (50, 630), "btnRight": (50, 700)}

PARTS = {
    "esp": {"type": "board-esp32-devkit-c-v4", "pos": ESP_POS,
            "size_mm": (27.9, 56.628), "pins_mm": ESP_PINS_MM},
    "tft": {"type": "board-ili9341-cap-touch", "pos": TFT_POS,
            "size_mm": (46.5, 77.6), "pins_mm": TFT_PINS_MM,
            "display_mm": (2, 7, 42.5, 56)},
    "speaker": {"type": "wokwi-buzzer", "pos": BUZZER_POS,
                "size_mm": (17, 20), "pins_px": BUZZER_PINS_PX},
}
for _id, _pos in BTN_POS.items():
    PARTS[_id] = {"type": "wokwi-pushbutton", "pos": _pos,
                  "size_mm": (17.802, 12), "pins_px": dict(BUTTON_PINS_PX)}


def pin(ref: str) -> tuple[float, float]:
    part_id, pin_name = ref.split(":", 1)
    part = PARTS[part_id]
    left, top = part["pos"]
    if "pins_mm" in part:
        px, py = part["pins_mm"][pin_name]
        return (left + px * MM, top + py * MM)
    px, py = part["pins_px"][pin_name]
    return (left + px, top + py)


def body(part_id: str) -> tuple[float, float, float, float]:
    part = PARTS[part_id]
    left, top = part["pos"]
    w, h = part["size_mm"]
    return (left, top, left + w * MM, top + h * MM)


def display_rect() -> tuple[float, float, float, float]:
    left, top = PARTS["tft"]["pos"]
    x, y, w, h = PARTS["tft"]["display_mm"]
    return (left + x * MM, top + y * MM, left + (x + w) * MM, top + (y + h) * MM)


def r(v: float) -> float:
    return round(v, 2)


# ------------------------------------------------------------------- wires --
# Each entry becomes a wire that leaves the source pin sideways, runs to a
# horizontal lane, crosses, then rises into the target pin from below.
# (source, target, colour, horizontal step off the source pin, lane y)
LANE_WIRES = [
    ("esp:3V3", "tft:VCC", "red", -12, 305),
    ("esp:3V3", "tft:LED", "red", -20, 315),
    ("esp:GND.3", "tft:GND", "black", 12, 325),
    ("esp:23", "tft:MOSI", "green", 20, 335),
    ("esp:18", "tft:SCK", "green", 28, 345),
    ("esp:19", "tft:MISO", "green", 36, 355),
    ("esp:5", "tft:CS", "blue", 44, 365),
    ("esp:2", "tft:D/C", "purple", 52, 375),
    ("esp:22", "tft:SCL", "gray", 60, 385),
    ("esp:21", "tft:SDA", "gray", 68, 395),
    ("esp:16", "speaker:1", "orange", 76, 600),
    ("esp:GND.2", "speaker:2", "black", 72, 620),
]

# Button signal wires: step sideways, drop to the target pin's row, run in.
# (source, target, colour, step off source, step off target)
ROW_WIRES = [
    ("btnLeft:1.r", "esp:13", "green", 20, -20),
    ("btnHome:1.r", "esp:27", "blue", 28, -28),
    ("btnRight:1.r", "esp:14", "green", 36, -36),
]

# Ground daisy chain down the left of the button stack.
COLUMN_WIRES = [
    ("btnLeft:2.l", "btnHome:2.l", "black", -12, -12),
    ("btnHome:2.l", "btnRight:2.l", "black", -12, -12),
]


def build_connections() -> list:
    conns = [
        ["esp:TX", "$serialMonitor:RX", "", []],
        ["esp:RX", "$serialMonitor:TX", "", []],
    ]

    for src, dst, color, step, lane in LANE_WIRES:
        sx, sy = pin(src)
        tx, ty = pin(dst)
        conns.append([src, dst, color,
                      [f"h{r(step)}", f"v{r(lane - sy)}", "*", f"v{r(lane - ty)}"]])

    for src, dst, color, s_step, t_step in ROW_WIRES:
        sx, sy = pin(src)
        tx, ty = pin(dst)
        conns.append([src, dst, color,
                      [f"h{r(s_step)}", f"v{r(ty - sy)}", "*", f"h{r(t_step)}"]])

    for src, dst, color, s_step, t_step in COLUMN_WIRES:
        conns.append([src, dst, color, [f"h{r(s_step)}", "*", f"h{r(t_step)}"]])

    # Button ground back to the ESP32: out to the right, then up the gap
    # between the button stack and the board.
    sx, sy = pin("btnRight:2.r")
    tx, ty = pin("esp:GND.1")
    conns.append(["btnRight:2.r", "esp:GND.1", "black",
                  ["h20", "*", f"v{r(sy - ty)}", "h-24"]])
    return conns


# ------------------------------------------------------------------ verify --

def trace(src: str, dst: str, path: list[str]) -> list[tuple]:
    """Reproduce Wokwi's wire routing as a list of segments."""
    split = path.index("*") if "*" in path else len(path)
    head, tail = path[:split], path[split + 1:]

    pts = [pin(src)]
    for ins in head:
        x, y = pts[-1]
        pts.append((x + float(ins[1:]), y) if ins[0] == "h" else (x, y + float(ins[1:])))

    end = [pin(dst)]
    for ins in reversed(tail):  # target-side instructions apply in reverse
        x, y = end[-1]
        end.append((x + float(ins[1:]), y) if ins[0] == "h" else (x, y + float(ins[1:])))

    a, b = pts[-1], end[-1]
    if abs(a[0] - b[0]) > 0.01 and abs(a[1] - b[1]) > 0.01:
        raise ValueError(f"{src}->{dst}: auto-join from {a} to {b} is ambiguous")
    pts.extend(reversed(end))

    return [(pts[i], pts[i + 1]) for i in range(len(pts) - 1)
            if pts[i] != pts[i + 1]]


def seg_hits_rect(seg, rect, pad=0.0) -> bool:
    (x1, y1), (x2, y2) = seg
    rx1, ry1, rx2, ry2 = rect
    rx1, ry1, rx2, ry2 = rx1 + pad, ry1 + pad, rx2 - pad, ry2 - pad
    return (min(x1, x2) < rx2 and max(x1, x2) > rx1
            and min(y1, y2) < ry2 and max(y1, y2) > ry1)


def verify(conns) -> int:
    errors, warnings = [], []
    screen = display_rect()
    traced = []

    for src, dst, color, path in conns:
        if not path or "$serialMonitor" in dst:
            continue
        segs = trace(src, dst, path)
        traced.append((f"{src}->{dst}", segs))

        for seg in segs:
            if seg_hits_rect(seg, screen):
                errors.append(f"{src}->{dst}: segment {seg} crosses the display {screen}")

        for part_id in PARTS:
            if part_id in (src.split(":")[0], dst.split(":")[0]):
                continue  # a wire may sit on its own part's pin header
            if any(seg_hits_rect(seg, body(part_id), pad=1.0) for seg in segs):
                errors.append(f"{src}->{dst}: runs over the body of '{part_id}'")

    # Collinear overlap between wires on different nets is visually confusing.
    for i, (name_a, segs_a) in enumerate(traced):
        for name_b, segs_b in traced[i + 1:]:
            for sa in segs_a:
                for sb in segs_b:
                    ov = overlap(sa, sb)
                    if ov > 2.0:
                        warnings.append(f"{name_a} and {name_b} overlap for {ov:.0f}px")

    for w in sorted(set(warnings)):
        print(f"  warning: {w}")
    for e in errors:
        print(f"  ERROR: {e}")
    print(f"\n{len(traced)} wires checked, {len(errors)} errors, "
          f"{len(set(warnings))} overlap warnings.")
    print(f"display face: x {screen[0]:.0f}..{screen[2]:.0f}, y {screen[1]:.0f}..{screen[3]:.0f}")
    lo = min(min(s[0][1], s[1][1]) for _, segs in traced for s in segs)
    print(f"topmost wire pixel: y {lo:.0f} (must be >= {screen[3]:.0f} to clear the screen)")
    return len(errors)


def overlap(sa, sb) -> float:
    """Length of collinear overlap between two axis-aligned segments."""
    (ax1, ay1), (ax2, ay2) = sa
    (bx1, by1), (bx2, by2) = sb
    if abs(ax1 - ax2) < 0.01 and abs(bx1 - bx2) < 0.01 and abs(ax1 - bx1) < 0.01:
        return max(0.0, min(max(ay1, ay2), max(by1, by2)) - max(min(ay1, ay2), min(by1, by2)))
    if abs(ay1 - ay2) < 0.01 and abs(by1 - by2) < 0.01 and abs(ay1 - by1) < 0.01:
        return max(0.0, min(max(ax1, ax2), max(bx1, bx2)) - max(min(ax1, ax2), min(bx1, bx2)))
    return 0.0


def render(conns) -> str:
    parts = [
        ("esp", "board-esp32-devkit-c-v4", ESP_POS, {}),
        ("tft", "board-ili9341-cap-touch", TFT_POS, {}),
        ("speaker", "wokwi-buzzer", BUZZER_POS, {"volume": "0.2"}),
        ("btnLeft", "wokwi-pushbutton", BTN_POS["btnLeft"], {"color": "green", "label": "Left"}),
        ("btnHome", "wokwi-pushbutton", BTN_POS["btnHome"], {"color": "blue", "label": "Home"}),
        ("btnRight", "wokwi-pushbutton", BTN_POS["btnRight"], {"color": "red", "label": "Right"}),
    ]
    lines = ['{', '  "version": 1,', '  "author": "Fernando",',
             '  "editor": "wokwi",', '  "parts": [']
    for i, (pid, ptype, (left, top), attrs) in enumerate(parts):
        comma = "," if i < len(parts) - 1 else ""
        lines.append(f'    {{ "type": "{ptype}", "id": "{pid}", "top": {top}, '
                     f'"left": {left}, "attrs": {json.dumps(attrs)} }}{comma}')
    lines.append('  ],')
    lines.append('  "connections": [')
    for i, (src, dst, color, path) in enumerate(conns):
        comma = "," if i < len(conns) - 1 else ""
        lines.append(f'    [ "{src}", "{dst}", "{color}", {json.dumps(path)} ]{comma}')
    lines.append('  ],')
    lines.append('  "dependencies": {}')
    lines.append('}')
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    connections = build_connections()
    if verify(connections):
        sys.exit(1)
    OUT.write_text(render(connections))
    print(f"wrote {OUT}")
