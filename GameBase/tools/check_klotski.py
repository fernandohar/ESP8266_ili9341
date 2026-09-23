#!/usr/bin/env python3
"""Solve the Klotski layouts in src/Scene_Klotski.h and report their difficulty.

Klotski has no shuffle to tune: each layout is a fixed puzzle whose difficulty is
whatever its geometry dictates, and the spread between layouts is enormous (a few
steps to nearly a hundred). This exhaustively BFSes the reachable state space so
the scene's difficulty ladder and its par step counts come from measurement.

Pieces of the same shape are interchangeable, so states are canonicalised by
sorting pieces by (shape, x, y). That collapses the classic layout to ~26k
positions instead of ~66k and makes plain BFS instant.

Two conventions are reported because both are in circulation:
  steps - a piece moving one cell. This is what the scene counts, because a drag
          that crosses two cell boundaries is two separate slides on screen.
  moves - a piece sliding any distance in one direction, counted once.
"""

import pathlib
import re
import sys
from collections import deque

COLS, ROWS = 4, 5
# The 2x2 block wins by reaching this cell (bottom centre, under the exit).
GOAL = (1, 3)

HEADER = pathlib.Path(__file__).resolve().parent.parent / "src" / "Scene_Klotski.h"

# Shapes, keyed by the letter used in the layout art below.
BIG, VERT, HORZ, ONE = "B", "V", "H", "S"
SHAPE_SIZE = {BIG: (2, 2), VERT: (1, 2), HORZ: (2, 1), ONE: (1, 1)}


def parse_layout(rows):
    """Turn 5 rows of 4 letters into a canonical piece tuple.

    Every distinct letter is one piece; '.' is empty. The piece's shape is
    inferred from the bounding box of its letter, and checked against the classic
    piece set so a typo in the art cannot quietly become a different puzzle.
    """
    cells = {}
    for y, row in enumerate(rows):
        if len(row) != COLS:
            raise SystemExit(f"row {y!r} is not {COLS} wide")
        for x, ch in enumerate(row):
            if ch != ".":
                cells.setdefault(ch, []).append((x, y))

    pieces = []
    for ch, pts in cells.items():
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        x, y = min(xs), min(ys)
        w, h = max(xs) - x + 1, max(ys) - y + 1
        if w * h != len(pts):
            raise SystemExit(f"piece {ch!r} is not a rectangle")
        shape = {(2, 2): BIG, (1, 2): VERT, (2, 1): HORZ, (1, 1): ONE}.get((w, h))
        if shape is None:
            raise SystemExit(f"piece {ch!r} has unsupported size {w}x{h}")
        pieces.append((shape, x, y))
    return tuple(sorted(pieces))


# A cell is one bit of a 20-bit board mask, so an occupancy test is an AND rather
# than a nested loop. Without this the BFS is slow enough that screening layouts
# for a difficulty ladder is impractical.
def cell_mask(x, y, w, h):
    mask = 0
    for dy in range(h):
        for dx in range(w):
            mask |= 1 << ((y + dy) * COLS + x + dx)
    return mask


PLACEMENT = {}
for _shape, (_w, _h) in SHAPE_SIZE.items():
    for _y in range(ROWS - _h + 1):
        for _x in range(COLS - _w + 1):
            PLACEMENT[(_shape, _x, _y)] = cell_mask(_x, _y, _w, _h)


def occupancy(state):
    mask = 0
    for piece in state:
        mask |= PLACEMENT[piece]
    return mask


def solved(state):
    return (BIG, GOAL[0], GOAL[1]) in state


DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def search(start, slide_to_end):
    """BFS to the nearest solved position.

    slide_to_end=False expands one cell at a time and so counts steps;
    slide_to_end=True expands every stop along a run and so counts moves. Either
    way each edge costs 1, so plain BFS is optimal.
    """
    if solved(start):
        return 0, 1
    seen = {start: 0}
    queue = deque([start])
    while queue:
        state = queue.popleft()
        cost = seen[state] + 1
        for index in range(len(state)):
            shape, x, y = state[index]
            rest = state[:index] + state[index + 1:]
            # Occupancy without the moving piece, so it can slide along its run.
            rest_occ = occupancy(rest)
            for dx, dy in DIRS:
                cx, cy = x, y
                while True:
                    place = (shape, cx + dx, cy + dy)
                    mask = PLACEMENT.get(place)
                    if mask is None or (mask & rest_occ):
                        break
                    cx, cy = place[1], place[2]
                    nxt = tuple(sorted(rest + (place,)))
                    if nxt not in seen:
                        seen[nxt] = cost
                        if solved(nxt):
                            return cost, len(seen)
                        queue.append(nxt)
                    if not slide_to_end:
                        break
    return None, len(seen)


# Candidate layouts, kept for re-tuning. B = the 2x2 block.
#
# Worth knowing before hunting for a board of some target difficulty: difficulty
# tracks the *number* of pieces far more than their arrangement. Every classic-set
# arrangement tried came out between 98 and 116 steps, and a random search over 700
# arrangements of a mid-weight set (1 big, 3 vertical, 1 horizontal, 3 singles)
# found nothing at all between 40 and 75 steps - they are all much easier. That is
# why the shipped tiers vary the soldier count rather than the layout.
CANDIDATES = {
    # The standard puzzle, "Hengdao Lima" - the arrangement in the reference art.
    "classic": ["ABBC", "ABBC", "DEEF", "DGHF", "I..J"],
    # Horizontal general parked at the foot of the board.
    "foot": ["ABBC", "ABBC", "DGHF", "D..F", "IEEJ"],
    # Singles split to the flanks, gap column open down the middle.
    "flanks": ["ABBC", "ABBC", "GEEH", "D..F", "DIJF"],
    # Block already halfway down.
    "halfway": ["AGHC", "AEEC", "DBBF", "DBBF", "I..J"],
    # Block low, singles above it.
    "low": ["AEEC", "AGHC", "DBBF", "DBBF", "I..J"],
    # Verticals inboard, block boxed by singles.
    "boxed": ["GBBH", "IBBJ", "ADDC", "AEEC", "F..F"],
}


def layout_from_header(name):
    """Read a layout out of the scene header, so the tool checks what ships."""
    if not HEADER.exists():
        return None
    text = HEADER.read_text()
    block = re.search(
        rf'KLOTSKI_LAYOUT_{name.upper()}\[[^\]]*\]\s*=\s*\{{(.*?)\}};', text, re.S
    )
    if not block:
        return None
    return re.findall(r'"([^"]{4})"', block.group(1))


def par_in_header(name):
    """The par constant the firmware ships, so a drift is caught rather than told."""
    if not HEADER.exists():
        return None
    match = re.search(
        rf"^#define KLOTSKI_PAR_{name.upper()} (\d+)", HEADER.read_text(), re.MULTILINE
    )
    return int(match.group(1)) if match else None


def main():
    names = sys.argv[1:] or ["easy", "medium", "classic"]
    bad = 0
    for name in names:
        shipped = layout_from_header(name)
        rows = shipped or CANDIDATES.get(name)
        if not rows:
            raise SystemExit(f"unknown layout: {name}")
        start = parse_layout(rows)
        steps, reach = search(start, slide_to_end=False)
        moves, _ = search(start, slide_to_end=True)
        source = "header" if shipped else "candidate"
        note = ""
        par = par_in_header(name) if shipped else None
        if par is not None:
            if par == steps:
                note = f"  par matches ({par})"
            else:
                note = f"  PAR MISMATCH: header says {par}, solver says {steps}"
                bad += 1
        print(
            f"{name:<9} ({source:<9}) min steps={str(steps):<5} min moves={str(moves):<5} "
            f"states seen={reach}{note}"
        )
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
