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

from prepare_dataset import FEATURES, LABELS, normalize, load_serial

FEATURE_COUNT = len(FEATURES)


def load_csv(path: Path, hub_only: bool = True) -> pd.DataFrame:
    df = load_serial(path)

    # Wide ML logger format (has event/ms columns).
    if "event" in df.columns or "hunger" in df.columns:
        if "label" not in df.columns and "hunger" in df.columns:
            pass
        return normalize(df, hub_only=hub_only)

    # Already narrow (e.g. synthetic.csv from generate_synthetic.py).
    if "label" not in df.columns:
        raise SystemExit(f"Missing label column in {path}")
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
    return df


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=Path, required=True, help="Synthetic, prepared, or raw ML CSV")
    parser.add_argument(
        "--all-events",
        action="store_true",
        help="Include game-end rows (event=1) when loading raw ML captures",
    )
    parser.add_argument("--epochs", type=int, default=80)
    args = parser.parse_args()

    df = load_csv(args.data, hub_only=not args.all_events)
    if len(df) < 40:
        raise SystemExit(f"Need at least ~40 rows; got {len(df)}. Collect more serial data.")

    X = df[FEATURES].astype(np.float32).values
    y = df["label"].map({name: i for i, name in enumerate(LABELS)}).values

    scaler = StandardScaler()
    X = scaler.fit_transform(X)

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
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
    print(classification_report(y_test, preds, target_names=LABELS))

    models_dir = Path(__file__).resolve().parent / "models"
    models_dir.mkdir(parents=True, exist_ok=True)
    keras_path = models_dir / "care_action.keras"
    model.save(keras_path)

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_path = models_dir / "care_action.tflite"
    tflite_path.write_bytes(converter.convert())

    np.save(models_dir / "scaler_mean.npy", scaler.mean_)
    np.save(models_dir / "scaler_scale.npy", scaler.scale_)

    print(f"Saved {keras_path} and {tflite_path}")
    print(f"TFLite size: {tflite_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
