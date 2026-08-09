#!/usr/bin/env python3
"""Filter ML CSV rows from a serial capture or live monitor stream.

Usage:
  pio device monitor -b 115200 | python download_serial.py > data/raw/sessions.csv
  python download_serial.py < capture.txt > data/raw/sessions.csv
"""

from __future__ import annotations

import sys

HEADER = (
    "ML,ms,event,game_id,outcome,score,difficulty,session_sec,"
    "hunger,happy,health,clean,sick,game_id_norm,win,session_games,label"
)


def main() -> None:
    wrote_header = False
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("ML,"):
            continue
        if line.startswith("ML,ms,event"):
            if not wrote_header:
                print(line)
                wrote_header = True
            continue
        if not wrote_header:
            print(HEADER)
            wrote_header = True
        print(line)


if __name__ == "__main__":
    main()
