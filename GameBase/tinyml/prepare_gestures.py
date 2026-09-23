#!/usr/bin/env python3
"""Turn a raw gesture capture into a feature table for training.

The features are computed by the *firmware's* extractor
(`src/ml/GestureFeatures.h`), compiled here as a small host binary. Nothing in
this script re-implements it: the care-action model keeps the same logic in C++
and in Python and needs a parity harness to stop the two drifting, and this
avoids that class of bug entirely.

Usage:
  python prepare_gestures.py data/raw/gestures.csv
  python prepare_gestures.py data/raw/*.csv -o data/processed/gestures.csv
"""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "src"
HOST_DIR = HERE / "host"
HOST_SRC = HOST_DIR / "gesture_features_main.cpp"
HOST_BIN = HOST_DIR / "gesture_features"
EXTRACTOR_HEADERS = [
    SRC / "ml" / "GestureFeatures.h",
    SRC / "ml" / "GestureEpisode.h",
]


def build_host_binary(force: bool = False) -> Path:
    """Compile the extractor driver, if the sources are newer than the binary."""
    sources = [HOST_SRC, *EXTRACTOR_HEADERS]
    if not force and HOST_BIN.exists():
        newest = max(path.stat().st_mtime for path in sources)
        if HOST_BIN.stat().st_mtime >= newest:
            return HOST_BIN

    command = [
        "c++",
        "-std=c++11",
        "-O2",
        "-Wall",
        f"-I{SRC}",
        str(HOST_SRC),
        "-o",
        str(HOST_BIN),
    ]
    print(f"compiling {HOST_SRC.name} ...", file=sys.stderr)
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise SystemExit("failed to build the feature extractor")
    if result.stderr.strip():
        print(result.stderr, file=sys.stderr)
    return HOST_BIN


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="+", type=Path, help="raw capture CSV file(s)")
    parser.add_argument(
        "-o",
        "--out",
        type=Path,
        default=HERE / "data" / "processed" / "gestures.csv",
        help="where to write the feature table",
    )
    parser.add_argument(
        "--rebuild", action="store_true", help="force a recompile of the extractor"
    )
    args = parser.parse_args()

    missing = [path for path in args.csv if not path.exists()]
    if missing:
        parser.error(f"no such file: {', '.join(str(p) for p in missing)}")

    binary = build_host_binary(force=args.rebuild)

    result = subprocess.run(
        [str(binary), *[str(path) for path in args.csv]],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        raise SystemExit("feature extraction failed")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(result.stdout)
    print(result.stderr.strip(), file=sys.stderr)

    rows = list(csv.DictReader(result.stdout.splitlines()))
    if not rows:
        raise SystemExit("no episodes survived; is the capture file empty?")

    counts = Counter(row["label"] for row in rows)
    sessions = Counter(row["boot"] for row in rows)
    feature_count = len(rows[0]) - 3  # boot, episode, label are not features
    print(
        f"\n{args.out}  ({len(rows)} episodes, {feature_count} features, "
        f"{len(sessions)} capture sessions)"
    )
    print()

    # Flagged rather than silently trained on: a class with a handful of examples
    # produces a confident-looking model that has simply memorised them.
    weakest = min(counts.values())
    for label, count in counts.most_common():
        flag = "  <- thin" if count < 20 else ""
        print(f"  {label:<12}{count:>5}{flag}")
    for label in ("poke", "double_poke", "long_press", "brush", "swipe",
                  "circle", "zigzag", "unknown"):
        if label not in counts:
            print(f"  {label:<12}{0:>5}  <- MISSING")

    if weakest < 20:
        print(
            "\nSome classes are thin. Capture more of them before reading much "
            "into a training run."
        )


if __name__ == "__main__":
    main()
