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

# Device thresholds (0..100 scale); features here are normalized 0..1.
CLEAN_BATH = 0.25
HUNGER_EAT = 0.30
HAPPY_LOW = 0.35
EXCITE_PLAY = 0.10
HAPPY_UNHAPPY = 0.15


def suggest(row: dict) -> str:
    if row["clean"] < CLEAN_BATH:
        return "bath"
    if row["hunger"] < HUNGER_EAT:
        return "eat"
    if row["happy"] < HAPPY_LOW:
        if row["excitement"] < EXCITE_PLAY:
            return "play"
        return "pet"
    return "play"


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
    """Sample near a care-rule boundary for better class coverage."""
    case = rng.randint(0, 4)
    happy = float(np_rng.uniform(0, 1))
    row = random_row(rng, np_rng)
    row["happy"] = happy
    row["unhappy"] = derive_unhappy(happy)

    if case == 0:
        row["clean"] = float(np_rng.uniform(0.05, 0.30))
        row["hunger"] = float(np_rng.uniform(0.35, 1.0))
    elif case == 1:
        row["hunger"] = float(np_rng.uniform(0.05, 0.32))
        row["clean"] = float(np_rng.uniform(0.30, 1.0))
    elif case == 2:
        row["happy"] = float(np_rng.uniform(0.05, 0.34))
        row["unhappy"] = derive_unhappy(row["happy"])
        row["excitement"] = float(np_rng.uniform(0.0, 0.09))
        row["hunger"] = float(np_rng.uniform(0.35, 1.0))
        row["clean"] = float(np_rng.uniform(0.30, 1.0))
    elif case == 3:
        row["happy"] = float(np_rng.uniform(0.05, 0.34))
        row["unhappy"] = derive_unhappy(row["happy"])
        row["excitement"] = float(np_rng.uniform(0.10, 1.0))
        row["hunger"] = float(np_rng.uniform(0.35, 1.0))
        row["clean"] = float(np_rng.uniform(0.30, 1.0))
    else:
        row["happy"] = float(np_rng.uniform(0.40, 1.0))
        row["unhappy"] = derive_unhappy(row["happy"])
        row["hunger"] = float(np_rng.uniform(0.35, 1.0))
        row["clean"] = float(np_rng.uniform(0.30, 1.0))

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
