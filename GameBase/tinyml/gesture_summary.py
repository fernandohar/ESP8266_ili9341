#!/usr/bin/env python3
"""Summarise a labelled touch-gesture capture, and eyeball individual episodes.

The capture CSV holds one row per sampled point:

    MLG,boot,episode,label,stroke,t_ms,x,y

An episode is identified by (boot, episode); boot changes at every reboot so
appending several capture sessions to one file stays unambiguous. An episode with
a DISCARD row was undone on the device and is dropped here.

Usage:
  python gesture_summary.py data/raw/gestures.csv
  python gesture_summary.py data/raw/gestures.csv --show brush --count 3
"""

from __future__ import annotations

import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

LABELS = [
    "poke",
    "double_poke",
    "long_press",
    "brush",
    "swipe",
    "circle",
    "zigzag",
    "unknown",
]

ART_W = 46
ART_H = 18


def load(paths: list[Path]) -> tuple[dict, set]:
    """Return {(boot, episode): {label, points}} plus the discarded episode keys."""
    episodes: dict[tuple[str, str], dict] = {}
    discarded: set[tuple[str, str]] = set()

    for path in paths:
        with path.open(newline="") as handle:
            for row in csv.reader(handle):
                if not row or row[0] != "MLG":
                    continue
                # Appending capture sessions repeats the header row.
                if len(row) < 8 or row[1] == "boot":
                    continue

                key = (row[1], row[2])
                label = row[3]
                if label == "DISCARD":
                    discarded.add(key)
                    continue
                if label not in LABELS:
                    continue

                try:
                    stroke = int(row[4])
                    t_ms = int(row[5])
                    x = int(row[6])
                    y = int(row[7])
                except ValueError:
                    continue

                episode = episodes.setdefault(key, {"label": label, "points": []})
                episode["points"].append((stroke, t_ms, x, y))

    return episodes, discarded


def describe(episode: dict) -> dict:
    points = episode["points"]
    strokes = len({p[0] for p in points})
    duration = max(p[1] for p in points) if points else 0
    path_len = 0.0
    for prev, cur in zip(points, points[1:]):
        if prev[0] != cur[0]:
            continue
        path_len += ((cur[2] - prev[2]) ** 2 + (cur[3] - prev[3]) ** 2) ** 0.5
    return {
        "points": len(points),
        "strokes": strokes,
        "duration": duration,
        "path": path_len,
    }


def median(values: list[float]) -> float:
    return statistics.median(values) if values else 0.0


def ascii_art(episode: dict) -> list[str]:
    points = episode["points"]
    xs = [p[2] for p in points]
    ys = [p[3] for p in points]
    x0, x1 = min(xs), max(xs)
    y0, y1 = min(ys), max(ys)
    span_x = max(x1 - x0, 1)
    span_y = max(y1 - y0, 1)

    grid = [[" "] * ART_W for _ in range(ART_H)]
    marks = "123456789"
    for stroke, _t, x, y in points:
        col = int((x - x0) * (ART_W - 1) / span_x)
        row = int((y - y0) * (ART_H - 1) / span_y)
        grid[row][col] = marks[stroke % len(marks)]

    return ["|" + "".join(row) + "|" for row in grid]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="+", type=Path, help="capture CSV file(s)")
    parser.add_argument("--show", help="render episodes of this label as ASCII")
    parser.add_argument(
        "--count", type=int, default=2, help="how many episodes to render (default 2)"
    )
    args = parser.parse_args()

    missing = [p for p in args.csv if not p.exists()]
    if missing:
        parser.error(f"no such file: {', '.join(str(p) for p in missing)}")

    episodes, discarded = load(args.csv)
    kept = {k: v for k, v in episodes.items() if k not in discarded}

    by_label: dict[str, list[dict]] = defaultdict(list)
    for key, episode in kept.items():
        by_label[episode["label"]].append(describe(episode))

    total = sum(len(v) for v in by_label.values())
    print(f"{total} episodes, {len(discarded)} discarded on device")
    print()
    print(f"{'label':<12}{'n':>5}{'pts':>7}{'ms':>7}{'strokes':>9}{'px':>7}{'Hz':>6}")
    print("-" * 53)

    for label in LABELS:
        stats = by_label.get(label, [])
        if not stats:
            print(f"{label:<12}{0:>5}")
            continue
        pts = median([s["points"] for s in stats])
        dur = median([s["duration"] for s in stats])
        strokes = median([s["strokes"] for s in stats])
        path = median([s["path"] for s in stats])
        hz = (pts * 1000 / dur) if dur else 0
        print(
            f"{label:<12}{len(stats):>5}{pts:>7.0f}{dur:>7.0f}"
            f"{strokes:>9.0f}{path:>7.0f}{hz:>6.0f}"
        )

    if not args.show:
        return

    shown = 0
    print()
    for key, episode in kept.items():
        if episode["label"] != args.show:
            continue
        stats = describe(episode)
        print(
            f"{args.show}  boot {key[0]} episode {key[1]}  "
            f"{stats['points']} pts  {stats['duration']} ms  "
            f"{stats['strokes']} strokes"
        )
        for line in ascii_art(episode):
            print(line)
        print()
        shown += 1
        if shown >= args.count:
            break

    if shown == 0:
        print(f"no episodes labelled {args.show!r}")


if __name__ == "__main__":
    main()
