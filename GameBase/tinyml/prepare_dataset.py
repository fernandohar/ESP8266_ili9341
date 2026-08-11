#!/usr/bin/env python3
"""Convert ML serial CSV captures into a narrow training table.

The device logs wide rows prefixed with ML,:
  ML,ms,event,game_id,outcome,score,difficulty,session_sec,
  hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label

Hub rows (event=0) are logged when returning to the pet home *after* game rewards
(hunger/clean/excitement applied). Prefer those for training.

Usage:
  python prepare_dataset.py data/raw/sessions.csv
  python prepare_dataset.py data/raw/sessions.csv -o data/processed/real.csv
  python prepare_dataset.py data/raw/sessions.csv --all-events
"""

from __future__ import annotations

import argparse
from pathlib import Path

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

# Thresholds mirrored from CareActionRules.h / PetSim.h (0..100 on device).
CLEAN_BATH = 25
HUNGER_EAT = 30
HAPPY_LOW = 35
EXCITE_PLAY = 10
HAPPY_UNHAPPY = 15


def suggest_from_features(row: pd.Series) -> str:
    """Rule oracle in Python — must match CareActionRules.h."""
    clean = row["clean"] * 100.0 if row["clean"] <= 1.0 else row["clean"]
    hunger = row["hunger"] * 100.0 if row["hunger"] <= 1.0 else row["hunger"]
    happy = row["happy"] * 100.0 if row["happy"] <= 1.0 else row["happy"]
    excitement = row["excitement"] * 100.0 if row["excitement"] <= 1.0 else row["excitement"]

    if clean < CLEAN_BATH:
        return "bath"
    if hunger < HUNGER_EAT:
        return "eat"
    if happy < HAPPY_LOW:
        return "play" if excitement < EXCITE_PLAY else "pet"
    return "play"


def load_serial(path: Path) -> pd.DataFrame:
    text = path.read_text().strip()
    first_line = text.split("\n", 1)[0]
    # Captures pasted without a header row start with "ML,<ms>,..."
    if first_line.startswith("ML,") and not first_line.startswith("ML,ms,"):
        parts = first_line.split(",")
        if len(parts) >= 2 and parts[1].isdigit():
            header = (
                "ML,ms,event,game_id,outcome,score,difficulty,session_sec,"
                "hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label"
            )
            text = header + "\n" + text
    from io import StringIO

    return pd.read_csv(StringIO(text))


def normalize(df: pd.DataFrame, hub_only: bool) -> pd.DataFrame:
    if hub_only and "event" in df.columns:
        df = df[df["event"] == 0].copy()

    rename = {
        "hungerNorm": "hunger",
        "happinessNorm": "happy",
        "excitementNorm": "excitement",
        "healthNorm": "excitement",  # legacy captures
        "cleanNorm": "clean",
        "isUnhappy": "unhappy",
        "isSick": "unhappy",
        "lastGameIdNorm": "game_id_norm",
        "lastOutcomeWin": "win",
        "sessionGamesNorm": "session_games",
    }
    df = df.rename(columns=rename)

    missing = [c for c in FEATURES if c not in df.columns]
    if missing:
        raise SystemExit(f"Missing feature columns {missing}. Re-flash esp32-tinyml-log firmware.")

    if "label" not in df.columns:
        df["label"] = df.apply(suggest_from_features, axis=1)

    df["label"] = df["label"].astype(str).str.strip().str.lower()
    bad = ~df["label"].isin(LABELS)
    if bad.any():
        raise SystemExit(f"Unknown labels: {df.loc[bad, 'label'].unique().tolist()}")

    # Recompute unhappy from happy when missing or inconsistent (happiness < 15).
    df["unhappy"] = (df["happy"] < (HAPPY_UNHAPPY / 100.0)).astype(float)

    out = df[FEATURES + ["label"]].copy()
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Raw or processed CSV")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output path (default: data/processed/<input stem>.csv)",
    )
    parser.add_argument(
        "--all-events",
        action="store_true",
        help="Keep game-end rows (event=1) as well as hub rows",
    )
    args = parser.parse_args()

    df = load_serial(args.input)
    out_df = normalize(df, hub_only=not args.all_events)

    out_path = args.output
    if out_path is None:
        out_path = Path(__file__).resolve().parent / "data" / "processed" / f"{args.input.stem}.csv"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_df.to_csv(out_path, index=False)

    print(f"Wrote {len(out_df)} rows to {out_path}")
    print(out_df["label"].value_counts())
    oracle = out_df.apply(suggest_from_features, axis=1)
    agree = (oracle == out_df["label"]).mean()
    print(f"Label agrees with rule oracle on {agree:.1%} of rows")


if __name__ == "__main__":
    main()
