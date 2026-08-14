#!/usr/bin/env python3
"""Train the touch-gesture classifier and export TFLite for the ESP32.

Input is the feature table from prepare_gestures.py, which runs the firmware's own
extractor, so the columns here are exactly the vector the device will build.

No feature scaler, unlike the care-action model. GestureFeatures.h normalises every
feature into a comparable range by construction (that is what the caps in it are
for), so there is nothing to fit and nothing extra to ship - which also removes a
place where training and inference could disagree.

Two accuracy numbers are reported on purpose:

  * a stratified random split, which is optimistic: episodes captured back to back
    in one sitting are near-duplicates, so some of the test set is effectively in
    the training set, and
  * leave-one-session-out, which holds out a whole capture session at a time and
    is the number that predicts how the device will behave.

A large gap between them means the model has learned something about *sessions*
rather than about gestures.

Usage:
  python train_gesture.py
  python train_gesture.py --data data/processed/gestures.csv --epochs 120
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.utils.class_weight import compute_class_weight

HERE = Path(__file__).resolve().parent

LABELS = [
    "poke",
    "double_poke",
    "long_press",
    "brush",
    "swipe",
    "circle",
    "zigzag",
    "unknown",
]

META_COLUMNS = ["boot", "episode", "label"]


def build_model(feature_count: int, seed: int = 42) -> tf.keras.Model:
    tf.keras.utils.set_random_seed(seed)
    model = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(feature_count,)),
            tf.keras.layers.Dense(32, activation="relu"),
            # A few hundred episodes against ~2k parameters overfits readily, so
            # dropout and early stopping do the regularising rather than a smaller
            # net that cannot express the circle/zigzag boundary.
            tf.keras.layers.Dropout(0.2),
            tf.keras.layers.Dense(16, activation="relu"),
            tf.keras.layers.Dense(len(LABELS), activation="softmax"),
        ]
    )
    model.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def fit(model, X_train, y_train, epochs: int, verbose: int = 0, seed: int = 42):
    weights = compute_class_weight("balanced", classes=np.unique(y_train), y=y_train)
    class_weight = {int(c): float(w) for c, w in zip(np.unique(y_train), weights)}

    # The validation split has to be built here rather than left to Keras'
    # validation_split, which takes the *last* fraction of the array without
    # shuffling. Captures arrive grouped by class, so that hands over a
    # single-class validation set, early stopping then tracks a meaningless loss
    # and restore_best_weights returns a barely-trained model. It looks exactly
    # like a data problem: accuracy collapses and no prediction clears 0.5.
    stratify = y_train if np.bincount(y_train).min() >= 2 else None
    X_fit, X_val, y_fit, y_val = train_test_split(
        X_train, y_train, test_size=0.15, random_state=seed, stratify=stratify
    )

    model.fit(
        X_fit,
        y_fit,
        epochs=epochs,
        batch_size=32,
        validation_data=(X_val, y_val),
        class_weight=class_weight,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(
                monitor="val_loss", patience=20, restore_best_weights=True
            )
        ],
        verbose=verbose,
    )
    return model


def print_confusion(y_true, y_pred, present: list[int]) -> None:
    matrix = confusion_matrix(y_true, y_pred, labels=range(len(LABELS)))
    width = 12
    print(" " * width + "".join(f"{LABELS[i][:8]:>9}" for i in present))
    for i in present:
        row = "".join(f"{matrix[i][j]:>9}" for j in present)
        total = matrix[i].sum()
        correct = matrix[i][i]
        share = 100 * correct / total if total else 0
        print(f"{LABELS[i]:<{width}}{row}   {share:>5.0f}% correct")


def confidence_table(y_true, probabilities) -> None:
    predicted = np.argmax(probabilities, axis=1)
    confidence = probabilities.max(axis=1)
    correct = predicted == y_true

    print(f"{'threshold':>10}{'acted on':>11}{'accuracy':>11}{'wrong acts':>12}")
    print("-" * 44)
    for threshold in (0.0, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95):
        accepted = confidence >= threshold
        if accepted.sum() == 0:
            continue
        accuracy = 100 * correct[accepted].mean()
        # What the player actually feels: the share of all gestures that produced
        # the wrong action. Rejections are silent, so they are not errors.
        wrong = 100 * (accepted & ~correct).sum() / len(y_true)
        print(
            f"{threshold:>10.2f}{100 * accepted.mean():>10.0f}%"
            f"{accuracy:>10.1f}%{wrong:>11.1f}%"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data", type=Path, default=HERE / "data" / "processed" / "gestures.csv"
    )
    parser.add_argument("--epochs", type=int, default=200)
    parser.add_argument(
        "--skip-loso",
        action="store_true",
        help="Skip leave-one-session-out cross validation (faster, less honest)",
    )
    parser.add_argument(
        "--keep-continuous-brush",
        action="store_true",
        help="Keep the early single-stroke brushes (see below); they are dropped "
        "by default as a superseded style",
    )
    args = parser.parse_args()

    if not args.data.exists():
        raise SystemExit(f"{args.data} not found; run prepare_gestures.py first")

    frame = pd.read_csv(args.data)

    # Brushing was first captured as one continuous scrub with the finger never
    # leaving the pad, and later as repeated strokes with a lift between them. Those
    # are different gestures, not variations of one: contact 100% of the time versus
    # 45%, one stroke versus four. Held out, 12 of the 14 continuous ones were
    # misread, while every other class in that fold scored 88% or better. Only the
    # lifting style is intended, so the handful of old ones are dropped rather than
    # left to contradict it.
    if not args.keep_continuous_brush:
        continuous = (
            (frame["label"] == "brush")
            & (frame["strokes"] <= 0.25)
            & (frame["contact_frac"] > 0.9)
        )
        if continuous.any():
            print(
                f"dropping {continuous.sum()} single-stroke brushes "
                "(superseded style; pass --keep-continuous-brush to train on them)"
            )
            frame = frame[~continuous].reset_index(drop=True)

    features = [c for c in frame.columns if c not in META_COLUMNS]
    X = frame[features].astype(np.float32).values
    y = frame["label"].map({name: i for i, name in enumerate(LABELS)}).values

    if np.isnan(y.astype(float)).any():
        unknown = sorted(set(frame["label"]) - set(LABELS))
        raise SystemExit(f"unrecognised labels in the data: {unknown}")
    y = y.astype(np.int32)

    counts = np.bincount(y, minlength=len(LABELS))
    present = [i for i in range(len(LABELS)) if counts[i] > 0]
    print(f"{len(frame)} episodes, {len(features)} features, "
          f"{frame['boot'].nunique()} capture sessions")
    print("  " + ", ".join(f"{LABELS[i]}={counts[i]}" for i in present))

    missing = [LABELS[i] for i in range(len(LABELS)) if counts[i] == 0]
    if missing:
        print(f"  no examples of: {', '.join(missing)} - the model cannot predict them")

    # A class living in only one session cannot be validated honestly: hold that
    # session out and the class vanishes from training entirely.
    per_session = frame.groupby("label")["boot"].nunique()
    single = per_session[per_session < 2]
    if len(single):
        print(
            "  only one capture session contains: "
            + ", ".join(single.index)
            + " - mix classes within a session so this is measurable"
        )

    print("\n=== stratified random split (optimistic) ===")
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )
    model = fit(build_model(len(features)), X_train, y_train, args.epochs)
    probabilities = model.predict(X_test, verbose=0)
    predicted = np.argmax(probabilities, axis=1)
    print(f"accuracy {100 * (predicted == y_test).mean():.1f}%\n")
    print(
        classification_report(
            y_test,
            predicted,
            labels=present,
            target_names=[LABELS[i] for i in present],
            zero_division=0,
        )
    )

    if not args.skip_loso:
        print("=== leave-one-session-out (honest) ===")
        sessions = frame["boot"].values
        true_all: list[int] = []
        prob_all: list[np.ndarray] = []
        for session in sorted(set(sessions)):
            test_mask = sessions == session
            if test_mask.sum() == 0 or (~test_mask).sum() < 40:
                continue
            fold = fit(
                build_model(len(features)),
                X[~test_mask],
                y[~test_mask],
                args.epochs,
            )
            fold_prob = fold.predict(X[test_mask], verbose=0)
            fold_pred = np.argmax(fold_prob, axis=1)
            accuracy = 100 * (fold_pred == y[test_mask]).mean()
            classes = ", ".join(sorted({LABELS[i] for i in y[test_mask]}))
            print(
                f"  held out {session:>6}: {test_mask.sum():>4} episodes, "
                f"{accuracy:>5.1f}% correct   ({classes})"
            )
            true_all.extend(y[test_mask].tolist())
            prob_all.append(fold_prob)

        if prob_all:
            y_true = np.array(true_all)
            probabilities = np.vstack(prob_all)
            predicted = np.argmax(probabilities, axis=1)
            print(f"\npooled accuracy {100 * (predicted == y_true).mean():.1f}%\n")
            print(
                classification_report(
                    y_true,
                    predicted,
                    labels=present,
                    target_names=[LABELS[i] for i in present],
                    zero_division=0,
                )
            )
            print("confusion, rows are the true gesture:\n")
            print_confusion(y_true, predicted, present)
            print("\nwhat a confidence threshold would buy, on the honest split:\n")
            confidence_table(y_true, probabilities)

    print("\n=== final model, trained on everything ===")
    final = fit(build_model(len(features)), X, y, args.epochs)

    models_dir = HERE / "models"
    models_dir.mkdir(parents=True, exist_ok=True)
    keras_path = models_dir / "gesture.keras"
    final.save(keras_path)

    # No Optimize.DEFAULT here, and it must stay that way. It applies dynamic-range
    # quantization to weight tensors above a size threshold, which for this network
    # caught exactly one - the first layer's 32x40 matrix - and left the rest
    # float32. The result is a *hybrid* model needing TFLite Micro's int8-weight,
    # float-activation FullyConnected path, which the TensorFlowLite_ESP32 snapshot
    # this project uses does not handle: every inference returned NaN.
    #
    # NaN then defeated the argmax (no comparison is ever true, so it reported index
    # 0) and the confidence floor (NaN < 0.8 is false, so it passed), turning a dead
    # model into a confident wrong answer. The predictor now rejects non-finite
    # outputs, and export_gesture_header.py refuses to ship quantized weights.
    #
    # Full int8 quantization would be a legitimate alternative, but it needs a
    # representative dataset and this model is 10 KB. There is nothing to save.
    converter = tf.lite.TFLiteConverter.from_keras_model(final)
    tflite_path = models_dir / "gesture.tflite"
    tflite_path.write_bytes(converter.convert())

    print(f"saved {keras_path.name} and {tflite_path.name}")
    print(f"tflite size: {tflite_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
