#!/usr/bin/env python3
"""Score the exported .tflite the way the firmware will run it.

Runs the same flatbuffer that got compiled into the header, standardizes inputs
with the same scaler, and applies the same confidence threshold as
CareActionPredictor — so the agreement number here is what the device does, not
what Keras did during training.

Usage:
  python eval_tflite.py --data data/processed/pooled.csv
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf

from prepare_dataset import FEATURES, LABELS

HERE = Path(__file__).resolve().parent
# Keep in step with CARE_PREDICT_MIN_CONFIDENCE in src/ml/CareActionPredictor.h.
MIN_CONFIDENCE = 0.65


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=HERE / "data" / "processed" / "pooled.csv")
    parser.add_argument("--model", type=Path, default=HERE / "models" / "care_action.tflite")
    parser.add_argument("--threshold", type=float, default=MIN_CONFIDENCE)
    args = parser.parse_args()

    frame = pd.read_csv(args.data)
    features = frame[FEATURES].to_numpy(dtype=np.float32)
    truth = frame["label"].map({name: i for i, name in enumerate(LABELS)}).to_numpy()

    mean = np.load(HERE / "models" / "scaler_mean.npy").astype(np.float32)
    scale = np.load(HERE / "models" / "scaler_scale.npy").astype(np.float32)
    scaled = (features - mean) / scale

    interpreter = tf.lite.Interpreter(model_path=str(args.model))
    interpreter.allocate_tensors()
    in_detail = interpreter.get_input_details()[0]
    out_detail = interpreter.get_output_details()[0]

    scores = np.zeros((len(scaled), len(LABELS)), dtype=np.float32)
    for i, row in enumerate(scaled):
        interpreter.set_tensor(in_detail["index"], row.reshape(1, -1))
        interpreter.invoke()
        scores[i] = interpreter.get_tensor(out_detail["index"])[0]

    predicted = scores.argmax(axis=1)
    confidence = scores.max(axis=1)
    confident = confidence >= args.threshold

    # What the device shows: the model when it is sure, the oracle otherwise.
    # `truth` is the oracle label, so a fallback row is right by construction.
    deployed = np.where(confident, predicted, truth)

    total = len(truth)
    print(f"Rows: {total}   threshold: {args.threshold}")
    print(f"Model agrees with oracle:      {(predicted == truth).mean():6.1%}")
    print(f"Confident enough to drive UI:  {confident.mean():6.1%}")
    print(f"Agreement when confident:      {(predicted == truth)[confident].mean():6.1%}")
    print(f"Deployed behaviour vs oracle:  {(deployed == truth).mean():6.1%}")
    print(f"Median confidence:             {np.median(confidence):6.3f}")

    print("\nDisagreements the player would see, by oracle label:")
    wrong = confident & (predicted != truth)
    if not wrong.any():
        print("  none")
    else:
        for i, name in enumerate(LABELS):
            rows = wrong & (truth == i)
            if rows.any():
                got = pd.Series(predicted[rows]).map(dict(enumerate(LABELS)))
                print(f"  {name:5s} -> {got.value_counts().to_dict()}")


if __name__ == "__main__":
    main()
