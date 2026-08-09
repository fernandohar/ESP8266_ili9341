# TinyML — PC-side training pipeline

Phase 1–2 tooling for the care-action classifier. See [`../docs/TINYML_PLAN.md`](../docs/TINYML_PLAN.md).

## Setup

```bash
cd GameBase/tinyml
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## 1. Synthetic data (prove pipeline)

```bash
python generate_synthetic.py
# writes data/processed/synthetic.csv
```

## 2. Train

```bash
python train_care_action.py --data data/processed/synthetic.csv
# writes models/care_action.keras and models/care_action.tflite
```

## 3. Collect real data from the console

Build firmware with logging enabled:

```bash
cd GameBase
pio run -e esp32-tinyml-log -t upload
pio device monitor -b 115200
```

Play games; lines prefixed with `ML,` are CSV rows. Capture to file:

```bash
pio device monitor -b 115200 | python download_serial.py > data/raw/sessions.csv
```

Retrain on real data:

```bash
python train_care_action.py --data data/raw/sessions.csv
```

## Feature columns

`hunger`, `happy`, `health`, `clean`, `sick`, `game_id_norm`, `win`, `session_games`

## Labels

`eat`, `play`, `pet`, `bath` — from `CareActionRules` oracle until you add manual overrides.
