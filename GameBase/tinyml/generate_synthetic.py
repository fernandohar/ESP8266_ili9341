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
    "health",
    "clean",
    "sick",
    "game_id_norm",
    "win",
    "session_games",
]
LABELS = ["eat", "play", "pet", "bath"]


def suggest(row: dict) -> str:
    if row["sick"] >= 0.5 or row["health"] < 0.25:
        return "bath"
    if row["hunger"] < 0.30:
        return "eat"
    if row["clean"] < 0.25:
        return "bath"
    if row["happy"] < 0.35:
        return "pet"
    if row["win"] < 0.5 and row["session_games"] > 0.3:
        return "pet"
    return "play"


def main() -> None:
    random.seed(42)
    np.random.seed(42)
    rows = []
    for _ in range(2000):
        row = {
            "hunger": float(np.random.uniform(0, 1)),
            "happy": float(np.random.uniform(0, 1)),
            "health": float(np.random.uniform(0, 1)),
            "clean": float(np.random.uniform(0, 1)),
            "sick": float(1 if random.random() < 0.08 else 0),
            "game_id_norm": float(np.random.uniform(0, 1)),
            "win": float(1 if random.random() < 0.55 else 0),
            "session_games": float(np.random.uniform(0, 1)),
        }
        row["label"] = suggest(row)
        rows.append(row)

    out = Path(__file__).resolve().parent / "data" / "processed" / "synthetic.csv"
    out.parent.mkdir(parents=True, exist_ok=True)
    pd.DataFrame(rows).to_csv(out, index=False)
    print(f"Wrote {len(rows)} rows to {out}")
    print(pd.Series([r["label"] for r in rows]).value_counts())


if __name__ == "__main__":
    main()
