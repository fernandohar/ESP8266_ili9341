#!/usr/bin/env python3
"""Generate synthetic care-action dataset using the same rules as CareActionRules.h."""

from __future__ import annotations

import random
from pathlib import Path

import numpy as np
import pandas as pd

FEATURES = [
    "hunger",
    "happy",
    "excitement",
    "clean",
    "unhappy",
    "game_id_norm",
    "win",
    "session_games",
]
LABELS = ["eat", "play", "pet", "bath"]

# Device thresholds (0..100 on device); features here are normalized 0..1.
# Bands where PetSim steepens the happiness slide, so they outrank the ranking.
CRITICAL_HUNGER = 0.15
CRITICAL_CLEAN = 0.10
# Excitement bar that picks between the two ways to raise happiness.
BORED_EXCITEMENT = 0.40
# A round's hunger/cleanness cost plus CARE_PLAY_COST_MARGIN.
PLAY_HUNGER_MIN = 0.18
PLAY_CLEAN_MIN = 0.16
HAPPY_UNHAPPY = 0.15
# Where "every stat is healthy" starts, for boundary sampling only — the rule
# itself no longer compares against a target.
HEALTHY_STAT = 0.80


def can_afford_play(row: dict) -> bool:
    return row["hunger"] > PLAY_HUNGER_MIN and row["clean"] > PLAY_CLEAN_MIN


def play_or_pet(row: dict) -> str:
    if row["excitement"] < BORED_EXCITEMENT and can_afford_play(row):
        return "play"
    return "pet"


def suggest(row: dict) -> str:
    """Rule oracle in Python — must match CareActionRules.h.

    Serves the lowest of the three visible stats; ties go to hunger, then
    cleanness, in the order they drain.
    """
    if row["hunger"] < CRITICAL_HUNGER:
        return "eat"
    if row["clean"] < CRITICAL_CLEAN:
        return "bath"

    lowest = min(row["hunger"], row["clean"], row["happy"])
    if row["hunger"] == lowest:
        return "eat"
    if row["clean"] == lowest:
        return "bath"
    return play_or_pet(row)


def derive_unhappy(happy: float) -> float:
    return 1.0 if happy < HAPPY_UNHAPPY else 0.0


def random_row(rng: random.Random, np_rng: np.random.Generator) -> dict:
    happy = float(np_rng.uniform(0, 1))
    return {
        "hunger": float(np_rng.uniform(0, 1)),
        "happy": happy,
        "excitement": float(np_rng.uniform(0, 1)),
        "clean": float(np_rng.uniform(0, 1)),
        "unhappy": derive_unhappy(happy),
        "game_id_norm": float(np_rng.uniform(0, 1)),
        "win": float(1 if rng.random() < 0.55 else 0),
        "session_games": float(np_rng.uniform(0, 1)),
    }


def row_near_threshold(np_rng: np.random.Generator, rng: random.Random) -> dict:
    """Sample near a care-rule boundary for better class coverage.

    One case per branch of suggest(): the two critical bands, each of the three
    stats reading lowest (happiness split by the bored bar), all three healthy
    with a clear lowest, and a near-tie between them. The last two are where a
    model trained on uniform rows guesses worst — the gaps there are a couple of
    points, so the decision boundary is thin.
    """
    case = rng.randint(0, 7)
    row = random_row(rng, np_rng)

    if case == 0:  # critical hunger
        row["hunger"] = float(np_rng.uniform(0.02, CRITICAL_HUNGER))
        row["clean"] = float(np_rng.uniform(0.30, 1.0))
    elif case == 1:  # critical cleanness
        row["clean"] = float(np_rng.uniform(0.01, CRITICAL_CLEAN))
        row["hunger"] = float(np_rng.uniform(0.20, 1.0))
    elif case == 2:  # hunger is the neediest stat
        row["hunger"] = float(np_rng.uniform(CRITICAL_HUNGER, 0.55))
        row["clean"] = float(np_rng.uniform(0.60, 1.0))
        row["happy"] = float(np_rng.uniform(0.60, 1.0))
    elif case == 3:  # cleanness is the neediest stat
        row["clean"] = float(np_rng.uniform(CRITICAL_CLEAN, 0.55))
        row["hunger"] = float(np_rng.uniform(0.60, 1.0))
        row["happy"] = float(np_rng.uniform(0.60, 1.0))
    elif case in (4, 5):  # happiness is the neediest stat: bored, then not
        row["happy"] = float(np_rng.uniform(0.05, 0.55))
        row["hunger"] = float(np_rng.uniform(0.60, 1.0))
        row["clean"] = float(np_rng.uniform(0.60, 1.0))
        if case == 4:
            row["excitement"] = float(np_rng.uniform(0.0, BORED_EXCITEMENT))
        else:
            row["excitement"] = float(np_rng.uniform(BORED_EXCITEMENT, 1.0))
    elif case == 6:  # every stat healthy: the lowest one still decides
        row["hunger"] = float(np_rng.uniform(HEALTHY_STAT, 1.0))
        row["clean"] = float(np_rng.uniform(HEALTHY_STAT, 1.0))
        row["happy"] = float(np_rng.uniform(HEALTHY_STAT, 1.0))
    else:  # near-tie: all three within a few points of each other
        base = float(np_rng.uniform(0.18, 0.95))
        for key in ("hunger", "clean", "happy"):
            row[key] = float(np.clip(base + np_rng.uniform(-0.04, 0.04), 0.02, 1.0))

    row["unhappy"] = derive_unhappy(row["happy"])
    return row


def main() -> None:
    rng = random.Random(42)
    np_rng = np.random.default_rng(42)
    rows = []
    for _ in range(1500):
        row = random_row(rng, np_rng)
        row["label"] = suggest(row)
        rows.append(row)
    for _ in range(500):
        row = row_near_threshold(np_rng, rng)
        row["label"] = suggest(row)
        rows.append(row)

    out = Path(__file__).resolve().parent / "data" / "processed" / "synthetic.csv"
    out.parent.mkdir(parents=True, exist_ok=True)
    pd.DataFrame(rows).to_csv(out, index=False)
    print(f"Wrote {len(rows)} rows to {out}")
    print(pd.Series([r["label"] for r in rows]).value_counts())


if __name__ == "__main__":
    main()
