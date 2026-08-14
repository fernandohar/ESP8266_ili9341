#!/usr/bin/env python3
"""Convert ML serial CSV captures into a narrow training table.

The device logs wide rows prefixed with ML,:
  ML,ms,event,game_id,outcome,score,difficulty,session_sec,
  hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label

Hub rows (event=0) are logged when returning to the pet home *after* game rewards
(hunger/clean/excitement applied). Care rows (event=2) are logged just after a
feed / pet / bath took effect. Both describe the pet while it is at home, so both
are kept by default; game-end rows (event=1) need --all-events.

The device writes its own `label` column, so a capture taken before a change to
CareActionRules.h carries labels from the old rule. --relabel replaces them with
the current oracle instead of re-collecting the session.

Usage:
  python prepare_dataset.py data/raw/sessions.csv
  python prepare_dataset.py data/raw/sessions.csv -o data/processed/real.csv
  python prepare_dataset.py data/raw/sessions.csv --all-events
  python prepare_dataset.py data/raw/sessions.csv --relabel
"""

from __future__ import annotations

import argparse
from io import StringIO
from pathlib import Path

import pandas as pd

ML_HEADER = (
    "ML,ms,event,game_id,outcome,score,difficulty,session_sec,"
    "hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label"
)

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

# Event kinds from GameplaySample.h that describe the pet at home.
HOME_EVENTS = (0, 2)

# Thresholds mirrored from CareActionRules.h / PetSim.h (0..100 on device).
CRITICAL_HUNGER = 15
CRITICAL_CLEAN = 10
BORED_EXCITEMENT = 40
PLAY_HUNGER_MIN = 18  # PET_GAME_PLAY_HUNGER_COST + CARE_PLAY_COST_MARGIN
PLAY_CLEAN_MIN = 16   # PET_GAME_PLAY_CLEAN_COST + CARE_PLAY_COST_MARGIN
HAPPY_UNHAPPY = 15


def suggest_from_features(row: pd.Series) -> str:
    """Rule oracle in Python — must match CareActionRules.h.

    Serves the lowest of the three visible stats; ties go to hunger, then
    cleanness, in the order they drain.
    """
    clean = row["clean"] * 100.0 if row["clean"] <= 1.0 else row["clean"]
    hunger = row["hunger"] * 100.0 if row["hunger"] <= 1.0 else row["hunger"]
    happy = row["happy"] * 100.0 if row["happy"] <= 1.0 else row["happy"]
    excitement = row["excitement"] * 100.0 if row["excitement"] <= 1.0 else row["excitement"]

    if hunger < CRITICAL_HUNGER:
        return "eat"
    if clean < CRITICAL_CLEAN:
        return "bath"

    lowest = min(hunger, clean, happy)
    if hunger == lowest:
        return "eat"
    if clean == lowest:
        return "bath"

    can_play = hunger > PLAY_HUNGER_MIN and clean > PLAY_CLEAN_MIN
    return "play" if excitement < BORED_EXCITEMENT and can_play else "pet"


def load_serial(path: Path) -> pd.DataFrame:
    lines = [ln.strip() for ln in path.read_text().splitlines() if ln.strip()]
    if not lines:
        raise SystemExit(f"{path} is empty")

    # Narrow CSV: synthetic.csv, or a file already through this script.
    if not lines[0].startswith("ML,"):
        return pd.read_csv(path)

    # Wide capture. Sessions appended with `>>` repeat the header row, and a
    # capture pasted out of the monitor may carry none, so rebuild with one.
    header = next((ln for ln in lines if ln.startswith("ML,ms,")), ML_HEADER)
    body = [ln for ln in lines if not ln.startswith("ML,ms,")]
    return pd.read_csv(StringIO("\n".join([header] + body)))


def normalize(df: pd.DataFrame, home_only: bool, relabel: bool = False) -> pd.DataFrame:
    if home_only and "event" in df.columns:
        df = df[df["event"].isin(HOME_EVENTS)].copy()

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

    if relabel or "label" not in df.columns:
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
        help="Keep game-end rows (event=1) as well as the at-home rows",
    )
    parser.add_argument(
        "--relabel",
        action="store_true",
        help="Replace the device labels with the current rule oracle (use for "
        "captures taken before CareActionRules.h changed)",
    )
    args = parser.parse_args()

    df = load_serial(args.input)
    device_labels = None
    if "label" in df.columns:
        device_labels = df["label"].astype(str).str.strip().str.lower()

    out_df = normalize(df, home_only=not args.all_events, relabel=args.relabel)
    if out_df.empty:
        raise SystemExit("No rows left after filtering; check the capture, or pass --all-events")

    out_path = args.output
    if out_path is None:
        out_path = Path(__file__).resolve().parent / "data" / "processed" / f"{args.input.stem}.csv"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_df.to_csv(out_path, index=False)

    print(f"Wrote {len(out_df)} rows to {out_path}")
    print(out_df["label"].value_counts())

    if args.relabel:
        if device_labels is None:
            print("Capture had no label column; every row was labeled by the rule oracle.")
            return
        changed = int((device_labels.reindex(out_df.index) != out_df["label"]).sum())
        print(
            f"Relabeled {changed} of {len(out_df)} rows ({changed / len(out_df):.1%}) "
            "where the device disagreed with the current rule"
        )
        return

    oracle = out_df.apply(suggest_from_features, axis=1)
    stale = int((oracle != out_df["label"]).sum())
    print(f"Label agrees with rule oracle on {1 - stale / len(out_df):.1%} of rows")
    if stale:
        print(
            f"{stale} rows were labeled under a different rule — rerun with --relabel "
            "to replace them"
        )


if __name__ == "__main__":
    main()
