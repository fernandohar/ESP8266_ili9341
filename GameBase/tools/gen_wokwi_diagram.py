#!/usr/bin/env python3
"""Generate and verify GameBase/diagram.json for the Wokwi simulator.

This file is the source of truth for the diagram. If you drag something around
in the Wokwi editor and like the result, copy the new top/left/rotate back into
PARTS below rather than leaving the change only in diagram.json - rerunning this
script overwrites whatever is on disk.

Wires deliberately carry no explicit path, so Wokwi auto-routes them. An earlier
version pinned every segment by hand to keep wires off the display, which meant
recomputing the whole routing table whenever a part moved.

That leaves the netlist as the part worth checking, and it is the part that can
actually break the build: verify() reads the pin numbers back out of the
firmware and fails if the diagram wires a peripheral somewhere the code does not
drive it.

Run: python3 tools/gen_wokwi_diagram.py          (rewrite diagram.json)
     python3 tools/gen_wokwi_diagram.py --check  (fail if it would change)
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "diagram.json"

# ------------------------------------------------------------------ layout --
# Panel top right, ESP32 to its left, buttons rotated upright in a row beneath
# it, buzzer off to the side. (id, type, left, top, rotate, attrs)

PARTS = [
    ("tft", "board-ili9341-cap-touch", 400, 0, 0, {}),
    ("esp", "board-esp32-devkit-c-v4", 150, 300, 0, {}),
    ("speaker", "wokwi-buzzer", 10, 190, 0, {"volume": "0.2"}),
    ("btnLeft", "wokwi-pushbutton", 520, 355, 90, {"color": "green", "label": "Left"}),
    ("btnHome", "wokwi-pushbutton", 600, 355, 90, {"color": "blue", "label": "Home"}),
    ("btnRight", "wokwi-pushbutton", 680, 355, 90, {"color": "red", "label": "Right"}),
]

# ----------------------------------------------------------------- netlist --
# (source, target, colour). Buttons pull their GPIO to ground, so each one has a
# signal wire plus a link in the ground daisy chain back to esp:GND.1.

CONNECTIONS = [
    ("esp:TX", "$serialMonitor:RX", ""),
    ("esp:RX", "$serialMonitor:TX", ""),
    ("esp:3V3", "tft:VCC", "red"),
    ("esp:3V3", "tft:LED", "red"),
    ("esp:GND.3", "tft:GND", "black"),
    ("esp:23", "tft:MOSI", "green"),
    ("esp:18", "tft:SCK", "green"),
    ("esp:19", "tft:MISO", "green"),
    ("esp:5", "tft:CS", "blue"),
    ("esp:2", "tft:D/C", "purple"),
    ("esp:22", "tft:SCL", "gray"),
    ("esp:21", "tft:SDA", "gray"),
    ("esp:16", "speaker:1", "orange"),
    ("esp:GND.2", "speaker:2", "black"),
    ("btnLeft:1.r", "esp:13", "green"),
    ("btnHome:1.r", "esp:27", "blue"),
    ("btnRight:1.r", "esp:14", "green"),
    ("btnLeft:2.l", "btnHome:2.l", "black"),
    ("btnHome:2.l", "btnRight:2.l", "black"),
    ("btnRight:2.r", "esp:GND.1", "black"),
]

# ------------------------------------------------------------------ verify --
# Peripheral pin -> the firmware symbol that has to name the same GPIO.

EXPECTED_PINS = {
    "tft:MOSI": ("platformio.ini", "TFT_MOSI"),
    "tft:SCK": ("platformio.ini", "TFT_SCLK"),
    "tft:MISO": ("platformio.ini", "TFT_MISO"),
    "tft:CS": ("platformio.ini", "TFT_CS"),
    "tft:D/C": ("platformio.ini", "TFT_DC"),
    "speaker:1": ("src/GameScene.h", "SPEAKER_PIN"),
    "btnLeft:1.r": ("src/Input.h", "BTN_LEFT_PIN"),
    "btnHome:1.r": ("src/Input.h", "BTN_HOME_PIN"),
    "btnRight:1.r": ("src/Input.h", "BTN_RIGHT_PIN"),
}

# The cap-touch controller rides the ESP32's default I2C pins, which nothing in
# the firmware spells out, so they are asserted here instead.
EXPECTED_I2C = {"tft:SDA": 21, "tft:SCL": 22}


def firmware_pin(source: str, symbol: str) -> int:
    """Read a GPIO number out of the firmware, erroring on any disagreement."""
    text = (ROOT / source).read_text()
    if source.endswith(".ini"):
        # Every env repeats the TFT pins; they all have to agree.
        found = re.findall(rf"-D\s+{re.escape(symbol)}=(\d+)", text)
    else:
        found = re.findall(rf"^#define\s+{re.escape(symbol)}\s+(\d+)", text, re.M)
    if not found:
        raise LookupError(f"{symbol} not defined in {source}")
    if len(set(found)) > 1:
        raise ValueError(f"{symbol} has conflicting values in {source}: {sorted(set(found))}")
    return int(found[0])


def esp_pin_for(peripheral: str) -> str | None:
    """The esp:<pin> a peripheral pin is wired to, or None if it is unwired."""
    for src, dst, _color in CONNECTIONS:
        for a, b in ((src, dst), (dst, src)):
            if a == peripheral and b.startswith("esp:"):
                return b.split(":", 1)[1]
    return None


def verify() -> int:
    errors = []
    part_ids = {pid for pid, *_ in PARTS}

    for src, dst, _color in CONNECTIONS:
        for ref in (src, dst):
            if ref.startswith("$"):
                continue
            part_id = ref.split(":", 1)[0]
            if part_id not in part_ids:
                errors.append(f"{src}->{dst}: no part called '{part_id}'")

    for peripheral, (source, symbol) in EXPECTED_PINS.items():
        wired = esp_pin_for(peripheral)
        if wired is None:
            errors.append(f"{peripheral} is not wired to the ESP32")
            continue
        try:
            expected = firmware_pin(source, symbol)
        except (LookupError, ValueError) as exc:
            errors.append(str(exc))
            continue
        if wired != str(expected):
            errors.append(f"{peripheral} is on esp:{wired} but {symbol} "
                          f"in {source} says GPIO {expected}")
        else:
            print(f"  ok  {peripheral:<14} esp:{wired:<4} ({symbol})")

    for peripheral, expected in EXPECTED_I2C.items():
        wired = esp_pin_for(peripheral)
        if wired != str(expected):
            errors.append(f"{peripheral} is on esp:{wired}, expected the default GPIO {expected}")
        else:
            print(f"  ok  {peripheral:<14} esp:{wired:<4} (ESP32 default I2C)")

    # A signal pin driven by two peripherals is a wiring mistake; the rails are
    # shared on purpose and each ESP32 ground is a physically separate pin.
    seen = {}
    for peripheral in list(EXPECTED_PINS) + list(EXPECTED_I2C):
        wired = esp_pin_for(peripheral)
        if wired in seen:
            errors.append(f"esp:{wired} drives both {seen[wired]} and {peripheral}")
        seen[wired] = peripheral

    for e in errors:
        print(f"  ERROR: {e}")
    print(f"\n{len(CONNECTIONS)} wires, {len(errors)} errors.")
    return len(errors)


# ------------------------------------------------------------------ render --

def render() -> str:
    lines = ['{', '  "version": 1,', '  "author": "Fernando",',
             '  "editor": "wokwi",', '  "parts": [']
    for i, (pid, ptype, left, top, rotate, attrs) in enumerate(PARTS):
        comma = "," if i < len(PARTS) - 1 else ""
        rot = f'"rotate": {rotate}, ' if rotate else ""
        lines.append(f'    {{ "type": "{ptype}", "id": "{pid}", "top": {top}, '
                     f'"left": {left}, {rot}"attrs": {json.dumps(attrs)} }}{comma}')
    lines.append('  ],')
    lines.append('  "connections": [')
    for i, (src, dst, color) in enumerate(CONNECTIONS):
        comma = "," if i < len(CONNECTIONS) - 1 else ""
        lines.append(f'    [ "{src}", "{dst}", "{color}", [] ]{comma}')
    lines.append('  ],')
    lines.append('  "dependencies": {}')
    lines.append('}')
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    if verify():
        sys.exit(1)

    text = render()
    current = OUT.read_text() if OUT.exists() else None

    if "--check" in sys.argv:
        if text != current:
            print(f"{OUT} is out of date; rerun without --check")
            sys.exit(1)
        print(f"{OUT} is up to date")
    elif text == current:
        print(f"{OUT} already up to date")
    else:
        OUT.write_text(text)
        print(f"wrote {OUT}")
