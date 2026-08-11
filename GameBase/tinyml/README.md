# TinyML — PC-side training pipeline

Care-action classifier for Pet Totoro. See [`../docs/TINYML_PLAN.md`](../docs/TINYML_PLAN.md).

After the pet-care redesign, features and labels follow `CareActionRules.h`:

| Priority | Condition (0..100 stats) | Label |
|----------|--------------------------|-------|
| 1 | `cleanness < 25` | `bath` |
| 2 | `hunger < 30` | `eat` |
| 3 | `happiness < 35` and `excitement < 10` | `play` |
| 3 | `happiness < 35` and `excitement ≥ 10` | `pet` |
| else | — | `play` |

**Excitement is hidden from the player UI** but is logged on Serial for training.

---

## Setup (once per PC)

```bash
cd GameBase/tinyml
python3 -m venv .venv
source .venv/bin/activate          # Windows: .venv\Scripts\activate
pip install -r requirements.txt
```

---

## Step 1 — Prove the pipeline (synthetic data)

Generates 2000 rows using the same rules as the device oracle:

```bash
python generate_synthetic.py
# → data/processed/synthetic.csv
```

Train and export TFLite:

```bash
python train_care_action.py --data data/processed/synthetic.csv
# → models/care_action.keras
# → models/care_action.tflite
# → models/scaler_mean.npy, models/scaler_scale.npy
```

Expect ~90%+ accuracy on synthetic data. This only validates the toolchain.

---

## Step 2 — Collect real data from the console

Flash the logging firmware:

```bash
cd GameBase
pio run -e esp32-tinyml-log -t upload
pio device monitor -b 115200
```

Play several sessions: let hunger/clean/happiness/excitement drift, finish games with wins and losses, return home between rounds.

**Capture Serial to a file:**

```bash
mkdir -p data/raw
pio device monitor -b 115200 | python download_serial.py > data/raw/sessions.csv
```

(Ctrl+C when done.)

### Which rows to use

| `event` | When logged | Use for training? |
|---------|-------------|-------------------|
| `0` | Hub visit (after `applyGameReward`) | **Yes — preferred** |
| `1` | Game end (pre-reward stats) | Optional |

Hub rows have correct post-game hunger, cleanness, and excitement.

---

## Step 3 — Prepare real CSV

```bash
python prepare_dataset.py data/raw/sessions.csv
# → data/processed/sessions.csv (hub rows only)
```

Options:

```bash
python prepare_dataset.py data/raw/sessions.csv -o data/processed/my_run.csv
python prepare_dataset.py data/raw/sessions.csv --all-events   # include game-end rows
```

The script checks that device `label` column matches the Python rule oracle.

---

## Step 4 — Train on real data

```bash
python train_care_action.py --data data/processed/sessions.csv
```

Or train directly from raw capture (hub rows only):

```bash
python train_care_action.py --data data/raw/sessions.csv
```

Aim for balanced labels (`eat`, `bath`, `play`, `pet`). If one class is missing, play until that situation occurs (e.g. skip feeding to get `eat`, skip bathing for `bath`).

---

## Feature columns (8)

| Column | Source |
|--------|--------|
| `hunger` | hunger / 100 |
| `happy` | happiness / 100 |
| `excitement` | excitement / 100 (hidden stat) |
| `clean` | cleanness / 100 |
| `unhappy` | 1 if happy < 0.15 else 0 |
| `game_id_norm` | last game scene / 7 |
| `win` | 1 if last game win else 0 |
| `session_games` | games this boot / 10 (capped) |

## Labels (4)

`eat`, `play`, `pet`, `bath` — auto-labeled on-device by `CareActionRules.h`.

---

## Serial CSV header

```
ML,ms,event,game_id,outcome,score,difficulty,session_sec,hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label
```

Legacy captures with `health,sick` columns must be re-collected after the pet-care redesign.

---

## Quick checklist

1. [ ] `pip install -r requirements.txt`
2. [ ] `python generate_synthetic.py && python train_care_action.py --data data/processed/synthetic.csv`
3. [ ] Flash `esp32-tinyml-log`, play 10+ varied rounds
4. [ ] `pio device monitor | python download_serial.py > data/raw/sessions.csv`
5. [ ] `python prepare_dataset.py data/raw/sessions.csv`
6. [ ] `python train_care_action.py --data data/processed/sessions.csv`
7. [ ] Inspect classification report; re-collect if one label is weak
