#!/usr/bin/env python3
"""Filter ML CSV rows from a serial capture or live monitor stream.

Usage:
  pio device monitor -b 115200 | python download_serial.py > data/raw/sessions.csv
  python download_serial.py < capture.txt > data/raw/sessions.csv

  # Touch-gesture capture (esp32-gesture-log) emits MLG rows instead:
  pio device monitor -b 115200 | python download_serial.py --gestures \
      >> data/raw/gestures.csv

Rows go to stdout, which is normally redirected into a file. So that a live
capture is not silent, each row is also mirrored to stderr with a running count
whenever stdout is redirected; -q turns the mirror off (use it with `tee`, which
already echoes). Every row is flushed as it arrives, so the file stays current
and nothing is lost if the monitor is interrupted.
"""

from __future__ import annotations

import argparse
import sys

HEADER = (
    "ML,ms,event,game_id,outcome,score,difficulty,session_sec,"
    "hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label"
)

GESTURE_HEADER = "MLG,boot,episode,label,stroke,t_ms,x,y"

# prefix -> (header row, how to recognise a header the firmware already printed)
FORMATS = {
    "ML": (HEADER, "ML,ms,event"),
    "MLG": (GESTURE_HEADER, "MLG,boot,episode"),
}

FLASH_HINTS = {
    "ML": "pio run -e esp32-tinyml-log -t upload",
    "MLG": "pio run -e esp32-gesture-log -t upload",
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="Do not mirror captured rows to stderr",
    )
    parser.add_argument(
        "-g",
        "--gestures",
        action="store_true",
        help="Capture touch-gesture rows (MLG) instead of gameplay rows (ML)",
    )
    args = parser.parse_args()

    prefix = "MLG" if args.gestures else "ML"
    header, header_marker = FORMATS[prefix]
    row_marker = prefix + ","

    mirror = not args.quiet and not sys.stdout.isatty()
    wrote_header = False
    rows = 0

    # Gesture capture emits one row per sampled point, so a single gesture is 30+
    # rows. Mirroring every one of them buries everything else; count episodes
    # instead and only show the first row of each.
    last_episode = None
    episodes = 0

    try:
        for line in sys.stdin:
            line = line.strip()
            if not line.startswith(row_marker):
                continue
            if line.startswith(header_marker):
                if not wrote_header:
                    print(line, flush=True)
                    wrote_header = True
                continue
            if not wrote_header:
                print(header, flush=True)
                wrote_header = True
            print(line, flush=True)
            rows += 1

            if not mirror:
                continue

            if prefix == "MLG":
                fields = line.split(",")
                episode = tuple(fields[1:4]) if len(fields) >= 4 else None
                if episode != last_episode:
                    last_episode = episode
                    episodes += 1
                    label = fields[3] if len(fields) >= 4 else "?"
                    print(
                        f"[{episodes:4d}] {label}",
                        file=sys.stderr,
                        flush=True,
                    )
            else:
                print(f"[{rows:4d}] {line}", file=sys.stderr, flush=True)
    except KeyboardInterrupt:
        pass

    if mirror:
        if prefix == "MLG":
            print(
                f"Captured {episodes} gestures ({rows} point rows).",
                file=sys.stderr,
                flush=True,
            )
        else:
            print(f"Captured {rows} ML rows.", file=sys.stderr, flush=True)
        if rows == 0:
            print(
                f"No {row_marker} rows seen. Flash the logging build first: "
                f"{FLASH_HINTS[prefix]}",
                file=sys.stderr,
                flush=True,
            )


if __name__ == "__main__":
    main()
