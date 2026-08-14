#!/usr/bin/env python3
"""Mirror Scene_ConnectFour's CPU and report how strong it actually is.

The scene's difficulty is search depth and nothing else, so the two depth
constants are the whole difficulty design and they deserve a measurement rather
than a guess. This reads them - along with the evaluation weights - straight out
of the header, replays the same negamax against the same evaluation, and reports:

  * the size of the board's line table, which the scene builds at load time;
  * head-to-head results between the shipped easy and hard settings;
  * peak leaf evaluations for one move at each depth, which is what decides
    whether a turn fits inside the scene's 50ms tick.

Usage: check_connect4.py [games]   (default 20; head-to-head runs are slow)
"""

import pathlib
import random
import re
import sys

HEADER = pathlib.Path(__file__).resolve().parent.parent / "src" / "Scene_ConnectFour.h"
TEXT = HEADER.read_text()


def const(name):
    match = re.search(rf"^#define {name} (\d+)", TEXT, re.MULTILINE)
    if not match:
        raise SystemExit(f"{name} not found in {HEADER.name}")
    return int(match.group(1))


COLS = const("CONNECT4_COLS")
ROWS = const("CONNECT4_ROWS")
NEED = const("CONNECT4_NEED")
EMPTY = const("CONNECT4_EMPTY")
P1 = const("CONNECT4_TOTORO")
P2 = const("CONNECT4_SOOT")
WIN_SCORE = const("CONNECT4_WIN_SCORE")
CENTER_WEIGHT = const("CONNECT4_CENTER_WEIGHT")
DEPTH_EASY = const("CONNECT4_DEPTH_EASY")
DEPTH_HARD = const("CONNECT4_DEPTH_HARD")

WEIGHT = [
    int(v) for v in re.search(
        r"CONNECT4_WINDOW_WEIGHT\[4\]\s*=\s*\{([^}]*)\}", TEXT
    ).group(1).split(",")
]

# Centre-out ordering, as in orderedColumn().
ORDER = [int(v) for v in re.search(
    r"ORDER\[CONNECT4_COLS\]\s*=\s*\{([^}]*)\}", TEXT
).group(1).split(",")]

DIRS = ((1, 0), (0, 1), (1, 1), (1, -1))

# Mirror of buildWindows(): every line of four, as flat cell indices.
WINDOWS = []
for row in range(ROWS):
    for col in range(COLS):
        for dc, dr in DIRS:
            ec, er = col + dc * (NEED - 1), row + dr * (NEED - 1)
            if 0 <= ec < COLS and 0 <= er < ROWS:
                WINDOWS.append(
                    [(row + dr * i) * COLS + col + dc * i for i in range(NEED)]
                )

leaf_evals = [0]


def new_board():
    return [EMPTY] * (COLS * ROWS), [0] * COLS


def foe(p):
    return P2 if p == P1 else P1


def landing_row(h, c):
    return ROWS - 1 - h[c]


def drop(b, h, c, p):
    r = landing_row(h, c)
    b[r * COLS + c] = p
    h[c] += 1
    return r


def undo(b, h, c):
    h[c] -= 1
    b[landing_row(h, c) * COLS + c] = EMPTY


def makes_four(b, c, r, p):
    for dc, dr in DIRS:
        run = 1
        for sign in (-1, 1):
            x, y = c + dc * sign, r + dr * sign
            while 0 <= x < COLS and 0 <= y < ROWS and b[y * COLS + x] == p:
                run += 1
                x += dc * sign
                y += dr * sign
        if run >= NEED:
            return True
    return False


def evaluate(b, p):
    leaf_evals[0] += 1
    other = foe(p)
    total = 0
    for cells in WINDOWS:
        mine = them = 0
        for idx in cells:
            v = b[idx]
            if v == p:
                mine += 1
            elif v == other:
                them += 1
        if them == 0:
            total += WEIGHT[min(mine, 3)]
        elif mine == 0:
            total -= WEIGHT[min(them, 3)]
    mid = COLS // 2
    for row in range(ROWS):
        v = b[row * COLS + mid]
        if v == p:
            total += CENTER_WEIGHT
        elif v == other:
            total -= CENTER_WEIGHT
    return total


def negamax(b, h, depth, alpha, beta, p):
    any_move = False
    best = -WIN_SCORE * 2
    for c in ORDER:
        if h[c] >= ROWS:
            continue
        any_move = True
        r = drop(b, h, c, p)
        if makes_four(b, c, r, p):
            undo(b, h, c)
            return WIN_SCORE + depth
        value = evaluate(b, p) if depth <= 1 else -negamax(
            b, h, depth - 1, -beta, -alpha, foe(p)
        )
        undo(b, h, c)
        best = max(best, value)
        alpha = max(alpha, best)
        if alpha >= beta:
            break
    return best if any_move else 0


def choose_column(b, h, p, depth, rng):
    leaf_evals[0] = 0
    scores = {}
    for c in ORDER:
        if h[c] >= ROWS:
            continue
        r = drop(b, h, c, p)
        if makes_four(b, c, r, p):
            value = WIN_SCORE + depth
        elif depth <= 1:
            value = evaluate(b, p)
        else:
            value = -negamax(b, h, depth - 1, -WIN_SCORE * 2, WIN_SCORE * 2, foe(p))
        undo(b, h, c)
        scores[c] = value
    if not scores:
        return -1, 0
    top = max(scores.values())
    return rng.choice([c for c, v in scores.items() if v == top]), leaf_evals[0]


def play(first, second, rng, peak=None):
    """One game. first/second are depths; returns the winning player or 0."""
    b, h = new_board()
    depth_of = {P1: first, P2: second}
    p = P1
    for _ in range(COLS * ROWS):
        c, evals = choose_column(b, h, p, depth_of[p], rng)
        if peak is not None:
            peak[depth_of[p]] = max(peak.get(depth_of[p], 0), evals)
        if c < 0:
            return 0
        r = drop(b, h, c, p)
        if makes_four(b, c, r, p):
            return p
        p = foe(p)
    return 0


def match(depth_a, depth_b, games, seed, peak=None):
    """Alternates who moves first, and reports from depth_a's point of view."""
    rng = random.Random(seed)
    tally = {"a": 0, "b": 0, "draw": 0}
    for g in range(games):
        a_first = (g % 2 == 0)
        winner = play(depth_a, depth_b, rng, peak) if a_first else play(
            depth_b, depth_a, rng, peak
        )
        if winner == 0:
            tally["draw"] += 1
        elif (winner == P1) == a_first:
            tally["a"] += 1
        else:
            tally["b"] += 1
    return tally


def main():
    games = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    expected = 69  # lines of four on a 7x6 grid
    ok = len(WINDOWS) == expected
    print(f"board {COLS}x{ROWS}, lines of {NEED}: {len(WINDOWS)} "
          f"({'as expected' if ok else f'EXPECTED {expected}'})")
    print(f"shipped depths: easy={DEPTH_EASY} hard={DEPTH_HARD}, "
          f"window weights={WEIGHT}, centre={CENTER_WEIGHT}")

    peak = {}
    print(f"\nhard vs easy, {games} games with alternating first move:")
    tally = match(DEPTH_HARD, DEPTH_EASY, games, seed=11, peak=peak)
    print(f"  hard {tally['a']} - easy {tally['b']}  (draws {tally['draw']})")

    print(f"\neasy vs a first-column-only opponent, {games} games "
          f"(easy must still punish that):")
    tally = match(DEPTH_EASY, 1, games, seed=12, peak=peak)
    print(f"  easy {tally['a']} - depth-1 {tally['b']}  (draws {tally['draw']})")

    print("\npeak leaf evaluations for a single move:")
    for depth in sorted(peak):
        n = peak[depth]
        print(f"  depth {depth}: {n:>6}  (~{n * 276 / 1e6:.2f}M cell reads)")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
