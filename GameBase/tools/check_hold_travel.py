#!/usr/bin/env python3
"""Verify the pet's hold thresholds still separate a hold from a stroke.

A hold on Totoro opens the radial menu, and it fires mid-contact, so a false
positive interrupts whatever the finger was really doing. The scene tells a hold
from a slow brush over the same spot by *path length* at the instant contact
reaches PET_GRAB_HOLD_MS, and PET_GRAB_MAX_TRAVEL_PX has to sit in the gap
between the two. This measures that gap from the captures and fails if the
shipped constant has drifted out of it.

Both constants are read out of src/Scene_PetTotoro.h, so editing the header and
rerunning this is the whole re-tuning workflow.

The captures under tinyml/data/ are gitignored; without them this skips.
"""
import csv
import math
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "src" / "Scene_PetTotoro.h"
CAPTURES = ROOT / "tinyml" / "data" / "raw" / "gestures.csv"

# The scene reads the panel from its own update(), which runs on this period, and
# accumulates travel once per read. Measuring on a finer grid would report a
# longer path than the firmware can actually see.
TICK_MS = 50

HOLD_LABEL = "long_press"


def define_in_header(name):
    """The value of a #define in the scene header, as a float."""
    pattern = re.compile(r"^#define\s+%s\s+([0-9.]+)f?\s*$" % re.escape(name),
                         re.MULTILINE)
    match = pattern.search(HEADER.read_text())
    if match is None:
        raise SystemExit("could not find #define %s in %s" % (name, HEADER))
    return float(match.group(1))


def load_first_strokes():
    """First stroke of every captured episode, as (t_ms, x, y) lists by label.

    Only the first stroke can become a hold: a second stroke means the finger
    left the glass, which ends the press the scene was timing.
    """
    episodes = defaultdict(list)
    with open(CAPTURES) as fh:
        for row in csv.DictReader(fh):
            # Every capture session re-emits the header, so headers recur mid-file.
            if not row["stroke"] or not row["stroke"].isdigit():
                continue
            if int(row["stroke"]) != 0:
                continue
            key = (row["boot"], row["episode"], row["label"])
            episodes[key].append((int(row["t_ms"]), int(row["x"]), int(row["y"])))
    for points in episodes.values():
        points.sort()
    return episodes


def travel_at(points, hold_ms):
    """Path length accumulated when contact age first reaches hold_ms.

    Returns None when the finger lifted first, since no hold could fire.
    """
    if not points or points[-1][0] < hold_ms:
        return None
    travel = 0.0
    last_x, last_y = points[0][1], points[0][2]
    index = 0
    tick = TICK_MS
    while True:
        # A read at this tick returns the most recent position the panel reported.
        while index + 1 < len(points) and points[index + 1][0] <= tick:
            index += 1
        x, y = points[index][1], points[index][2]
        travel += math.hypot(x - last_x, y - last_y)
        last_x, last_y = x, y
        if tick >= hold_ms:
            return travel
        tick += TICK_MS


def measure(hold_ms):
    """Travel figures per label for every episode still down at hold_ms."""
    per_label = defaultdict(list)
    for (_, _, label), points in load_first_strokes().items():
        travel = travel_at(points, hold_ms)
        if travel is not None:
            per_label[label].append(travel)
    return {label: sorted(values) for label, values in per_label.items()}


def report(hold_ms, budget_px):
    per_label = measure(hold_ms)
    holds = per_label.get(HOLD_LABEL, [])
    strokes = sorted(v for label, values in per_label.items()
                     if label != HOLD_LABEL for v in values)

    print("contact age %d ms, travel as path length on the %d ms tick:"
          % (hold_ms, TICK_MS))
    for label in sorted(per_label):
        values = per_label[label]
        print("  %-12s n=%-4d min=%6.1f  median=%6.1f  max=%6.1f"
              % (label, len(values), values[0],
                 values[len(values) // 2], values[-1]))

    if not holds:
        print("\nno %s capture stays down that long: nothing to separate" % HOLD_LABEL)
        return False
    if not strokes:
        print("\nno other gesture stays down that long: any budget works")
        return True

    print("\na hold reaches at most %.1f px; the nearest stroke covers %.1f px"
          % (holds[-1], strokes[0]))
    if holds[-1] >= strokes[0]:
        print("FAIL: the ranges overlap, so no travel budget can separate them.")
        print("      Raise PET_GRAB_HOLD_MS until they part.")
        return False
    if not holds[-1] < budget_px < strokes[0]:
        print("FAIL: PET_GRAB_MAX_TRAVEL_PX is %.0f, outside that gap." % budget_px)
        return False
    print("PET_GRAB_MAX_TRAVEL_PX is %.0f, inside the gap "
          "(%.0f px clear of a real hold, %.0f px clear of a stroke)."
          % (budget_px, budget_px - holds[-1], strokes[0] - budget_px))
    return True


def main():
    if not CAPTURES.exists():
        print("no captures at %s (gitignored); skipping" % CAPTURES)
        return 0

    hold_ms = int(define_in_header("PET_GRAB_HOLD_MS"))
    budget_px = define_in_header("PET_GRAB_MAX_TRAVEL_PX")
    ok = report(hold_ms, budget_px)

    # A shorter hold is what the interaction wants, so show where that stops
    # working rather than leaving the floor to be rediscovered by hand.
    shorter = hold_ms - TICK_MS
    if shorter >= TICK_MS:
        print("\n--- one tick shorter, %d ms ---" % shorter)
        report(shorter, budget_px)

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
