#!/usr/bin/env python3
"""Sanity-check the slide puzzle's scramble depth against its 20 s target.

Mirrors Scene_SlidePuzzle's random walk exactly (including the refusal to undo
the previous slide) and solves each scramble optimally with IDA*, so the two
shuffle depths can be judged in taps rather than by eye. Run after changing
PUZZLE_*_SHUFFLE_MOVES.
"""

import pathlib
import random
import re
import sys

HEADER = pathlib.Path(__file__).resolve().parent.parent / "src" / "Scene_SlidePuzzle.h"

COLS, ROWS = 3, 4
CELLS = COLS * ROWS
HOLE = CELLS - 1
SOLVED = tuple(range(CELLS))


def neighbours(cell):
    col, row = cell % COLS, cell // COLS
    out = []
    if col > 0:
        out.append(cell - 1)
    if col < COLS - 1:
        out.append(cell + 1)
    if row > 0:
        out.append(cell - COLS)
    if row < ROWS - 1:
        out.append(cell + COLS)
    return out


NEIGHBOURS = [neighbours(c) for c in range(CELLS)]


def scramble(steps, rng):
    """The C++ randomWalk(), tile-for-tile."""
    tiles = list(range(CELLS))
    hole = CELLS - 1
    came_from = -1
    for _ in range(steps):
        options = NEIGHBOURS[hole]
        pick = rng.choice(options)
        tries = 0
        while tries < 6 and pick == came_from and len(options) > 1:
            pick = rng.choice(options)
            tries += 1
        came_from = hole
        tiles[hole] = tiles[pick]
        tiles[pick] = HOLE
        hole = pick
    return tuple(tiles), hole


def manhattan(tiles):
    """Distance ignoring the hole, which is not a piece the player places."""
    total = 0
    for cell, tile in enumerate(tiles):
        if tile == HOLE:
            continue
        total += abs(cell % COLS - tile % COLS) + abs(cell // COLS - tile // COLS)
    return total


def solve_length(tiles, hole):
    """Optimal slide count via IDA* on Manhattan distance."""
    bound = manhattan(tiles)
    tiles = list(tiles)

    def search(hole, prev, g, bound):
        h = manhattan(tiles)
        if h == 0:
            return g, True
        f = g + h
        if f > bound:
            return f, False
        best = None
        for nxt in NEIGHBOURS[hole]:
            if nxt == prev:
                continue
            tiles[hole], tiles[nxt] = tiles[nxt], tiles[hole]
            got, done = search(nxt, hole, g + 1, bound)
            if done:
                return got, True
            tiles[hole], tiles[nxt] = tiles[nxt], tiles[hole]
            if best is None or got < best:
                best = got
        return (bound + 1 if best is None else best), False

    while True:
        got, done = search(hole, -1, 0, bound)
        if done:
            return got
        bound = got


def shuffle_depth(macro):
    """Read a scramble depth out of the scene header so this can't drift."""
    text = HEADER.read_text()
    match = re.search(rf"^#define {macro} (\d+)", text, re.MULTILINE)
    if not match:
        raise SystemExit(f"{macro} not found in {HEADER}")
    return int(match.group(1))


def main():
    samples = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    rng = random.Random(20260814)
    modes = (
        ("TIME ATTACK", shuffle_depth("PUZZLE_ATTACK_SHUFFLE_MOVES")),
        ("NORMAL", shuffle_depth("PUZZLE_NORMAL_SHUFFLE_MOVES")),
    )
    for name, steps in modes:
        lengths = []
        for _ in range(samples):
            tiles, hole = scramble(steps, rng)
            assert tiles != SOLVED, "shuffle produced a solved board"
            lengths.append(solve_length(tiles, hole))
        lengths.sort()
        mid = lengths[len(lengths) // 2]
        print(
            f"{name:<12} walk={steps:<3} optimal slides: "
            f"min={lengths[0]} median={mid} max={lengths[-1]}  "
            f"({mid / 20:.1f} slides/s needed to beat 20 s at optimal play)"
        )


if __name__ == "__main__":
    main()
