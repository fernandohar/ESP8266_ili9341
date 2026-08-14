#!/usr/bin/env python3
"""Train tiny care-action MLP and export TFLite for ESP32 (Phase 2)."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler

from prepare_dataset import (
    FEATURES,
    LABELS,
    load_serial,
    normalize,
    suggest_from_features,
)

FEATURE_COUNT = len(FEATURES)


def load_csv(path: Path, home_only: bool = True, relabel: bool = False) -> pd.DataFrame:
    df = load_serial(path)

    # Wide ML logger format: the device always writes an event column.
    if "event" in df.columns:
        return normalize(df, home_only=home_only, relabel=relabel)

    # Already narrow (e.g. synthetic.csv from generate_synthetic.py).
    rename = {
        "hungerNorm": "hunger",
        "happinessNorm": "happy",
        "excitementNorm": "excitement",
        "healthNorm": "excitement",
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
        raise SystemExit(f"Missing features {missing} in {path}")
    if relabel or "label" not in df.columns:
        df["label"] = df.apply(suggest_from_features, axis=1)
    return df


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=Path, required=True, help="Synthetic, prepared, or raw ML CSV")
    parser.add_argument(
        "--all-events",
        action="store_true",
        help="Include game-end rows (event=1) when loading raw ML captures",
    )
    parser.add_argument(
        "--relabel",
        action="store_true",
        help="Relabel with the current rule oracle instead of trusting the "
        "labels in the CSV (use for data collected before a rule change)",
    )
    parser.add_argument("--epochs", type=int, default=80)
    args = parser.parse_args()

    df = load_csv(args.data, home_only=not args.all_events, relabel=args.relabel)
    if len(df) < 40:
        raise SystemExit(f"Need at least ~40 rows; got {len(df)}. Collect more serial data.")

    X = df[FEATURES].astype(np.float32).values
    y = df["label"].map({name: i for i, name in enumerate(LABELS)}).values

    # A real capture often misses a label entirely (e.g. no bath if the pet never
    # got dirty). Training still works, but the model can never answer with a
    # class it has not seen, so say so up front.
    counts = np.bincount(y, minlength=len(LABELS))
    print("Label counts: " + ", ".join(f"{LABELS[i]}={counts[i]}" for i in range(len(LABELS))))
    missing = [LABELS[i] for i in range(len(LABELS)) if counts[i] == 0]
    if missing:
        print(
            f"Warning: no rows labeled {', '.join(missing)}. The model cannot predict "
            "those actions — collect sessions that reach those states."
        )

    scaler = StandardScaler()
    X = scaler.fit_transform(X)

    # Stratifying needs two rows of every present class.
    stratify = y if counts[counts > 0].min() >= 2 else None
    if stratify is None:
        print("Warning: a label has a single row; splitting without stratification.")

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=stratify
    )

    model = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(FEATURE_COUNT,)),
            tf.keras.layers.Dense(16, activation="relu"),
            tf.keras.layers.Dense(8, activation="relu"),
            tf.keras.layers.Dense(len(LABELS), activation="softmax"),
        ]
    )
    model.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    model.fit(
        X_train,
        y_train,
        epochs=args.epochs,
        batch_size=32,
        validation_split=0.15,
        verbose=1,
    )

    preds = np.argmax(model.predict(X_test, verbose=0), axis=1)
    # labels= pins the report to all four classes so an absent one reports zero
    # support instead of failing the size check against target_names.
    print(
        classification_report(
            y_test,
            preds,
            labels=range(len(LABELS)),
            target_names=LABELS,
            zero_division=0,
        )
    )

    models_dir = Path(__file__).resolve().parent / "models"
    models_dir.mkdir(parents=True, exist_ok=True)
    keras_path = models_dir / "care_action.keras"
    model.save(keras_path)

    # Deliberately not Optimize.DEFAULT. It dynamic-range quantizes weight tensors
    # over a size threshold and leaves the rest float32, producing a hybrid model
    # that needs an int8-weight FullyConnected kernel TensorFlowLite_ESP32 lacks -
    # and it fails by returning NaN, not by refusing to run. This model's largest
    # matrix (8x16) has always been under the threshold, so nothing here was ever
    # quantized and removing the flag changes nothing; the gesture model's 32x40
    # matrix crossed it and shipped broken. Not worth leaving armed for whenever
    # this network next grows.
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_path = models_dir / "care_action.tflite"
    tflite_path.write_bytes(converter.convert())

    np.save(models_dir / "scaler_mean.npy", scaler.mean_)
    np.save(models_dir / "scaler_scale.npy", scaler.scale_)

    print(f"Saved {keras_path} and {tflite_path}")
    print(f"TFLite size: {tflite_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
