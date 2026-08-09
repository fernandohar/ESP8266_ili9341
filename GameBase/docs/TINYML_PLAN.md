# TinyML Plan — Game Console

Branch: `cursor/tinyml-plan-6e1f`

This document verifies an external TinyML proposal (ChatGPT), explains what to keep or drop, and defines a **simpler plan** aligned with this repo (PlatformIO + Arduino, Pet Totoro, mini-games).

---

## 1. Verdict on the ChatGPT proposal

### What is correct

| Claim | Verdict |
|-------|---------|
| TinyML should be a small behavioral classifier, not an on-device LLM | **Yes** — right goal for ESP32 |
| ESP-DL supports original ESP32 (slower than S3/P4) | **Yes** — operators run in C on ESP32 |
| Start with data collection before training | **Yes** — best first milestone |
| 8–15 numeric features → tiny dense network | **Yes** — appropriate for MCU RAM/flash |
| Synthetic dataset to prove the pipeline first | **Yes** — reduces debugging surface |
| Model predicts; C++ rules act on the prediction | **Yes** — good separation |
| Use confidence threshold + `UNKNOWN` fallback | **Yes** — required for robust demos |
| Manual labels teach supervised learning | **Yes** — but not ideal as the *first* problem (see below) |

### What is wrong or mismatched for this project

| Issue | Detail |
|-------|--------|
| **ESP-PPQ `target="esp32"`** | Espressif docs say use **`target="c"`** when quantizing for original ESP32 ([ESP-DL quantize guide](https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html)). |
| **Repo layout (`components/`, CMake)** | This project uses **PlatformIO + Arduino**, not ESP-IDF components. ESP-DL integration is heavier here than ChatGPT implies. |
| **ESP-DL as step 1** | Official examples focus on **ESP32-S3/P4**; ESP32 support exists but is less documented and slower. Prefer **TFLite Micro** for the first deployed MLP, ESP-DL later if you want the Espressif stack. |
| **`petEnergy` field** | Does not exist — pet stats are **health, hunger, happiness, cleanness** (`PetTotoroState`). |
| **Reaction-time features** | Not instrumented in any mini-game yet — would be new work across 4 scenes. |
| **Engaged / Frustrated / Bored as model 1** | Subjective labels, hard to collect solo, synthetic data will not match real play. Good *learning exercise*, weak *product demo*. |
| **Three models + eight milestones upfront** | Over-scoped for a first milestone; leads to never shipping. |
| **500–1000 hand-labeled emotion samples** | Unrealistic before you know features are useful. |

### Is “AI Game Coach” (emotion classifier) a good use case?

**As ML coursework:** yes — features, labels, train/evaluate/deploy loop.

**As the first TinyML feature for this console:** **no** — prefer an **objective** task that:

- Uses data you already have (pet stats, game outcome, score, difficulty level)
- Can be **auto-labeled** with rules while you collect real logs
- Maps to visible UI (radial menu: eat, play, pet, bathe)

That matches your root `README.md` TinyML example (“what should the player do next?”).

---

## 2. Recommended first model (instead of emotion)

### Care Action Classifier (offline pet coach)

**Job:** Given pet state + recent session context, predict the best **care action**:

| Output | Maps to |
|--------|---------|
| `EAT` | Grocery / feeding |
| `PLAY` | Mini-games radial |
| `PET` | Pet interaction |
| `BATHE` | Bath action |

**Inputs (8 features, all available today or with minimal logging):**

| Feature | Source |
|---------|--------|
| `hunger_norm` | `PetTotoroState::stats().hunger / 100` |
| `happiness_norm` | happiness / 100 |
| `health_norm` | health / 100 |
| `clean_norm` | cleanness / 100 |
| `is_sick` | `PetTotoroState::isSick()` |
| `last_game_id` | scene id from last `GameResult` |
| `last_outcome` | win / loss |
| `session_games` | count this boot (logger) |

**Labels (phase 1 — automatic “oracle”):** rule-based teacher in `CareActionRules.h` (same thresholds you would code by hand). The ML model later **learns to mimic or beat** those rules from logged data.

**Demo line:** *“The console runs a neural network on the ESP32 that recommends what to do for Totoro — no Wi‑Fi.”*

### Second model (later): Difficulty adjuster

For **Whack-a-Mole level** or **Acorn Catch spawn aggression** — labels from outcomes:

- success rate high → increase difficulty  
- repeated failures → decrease difficulty  

Objective, easy to auto-label, pairs with adaptive gameplay you already discussed.

### Third model (optional, much later): Player engagement

Only after you instrument **input rate, retries, session length** and add a quick post-game label UI. Do not start here.

---

## 3. Simplified roadmap (4 phases, not 8)

### Phase 1 — Data plumbing (this branch)

- [x] `GameplaySample` struct  
- [x] `MLDataLogger` — CSV lines on Serial when `TINYML_DATA_LOG=1`  
- [x] `CareActionRules` — rule oracle + label name  
- [ ] Hook logger from mini-game end + pet hub (next PR)  
- [ ] Settings → “Export ML CSV” (optional)

**No neural network yet.**

### Phase 2 — Train on PC

- `tinyml/generate_synthetic.py` — prove pipeline  
- `tinyml/train_care_action.py` — Keras MLP → **TFLite** (`int8`)  
- Target: >90% on synthetic; then retrain on real Serial exports  

### Phase 3 — On-device inference (Arduino-friendly)

- Add **`esp32-tinyml`** PlatformIO env with `TINYML_INFERENCE=1`  
- Integrate **TensorFlow Lite Micro** (or EloquentTinyML) — ~3 KB model  
- `CareActionPredictor::predict()` → `CareAction` + confidence  
- `Scene_PetTotoro`: highlight radial menu item when confidence ≥ 0.65  

**Skip ESP-DL until this works.**

### Phase 4 — Optional ESP-DL path

- Export same MLP → ONNX → ESP-PPQ with **`target="c"`** → `.espdl`  
- Compare inference ms / flash vs TFLite  
- Good for “Espressif stack” write-ups, not required for v1  

---

## 4. Architecture

```
Mini-game / Pet hub
        │
        ▼
  GameplaySample  ──► MLDataLogger (Serial CSV)     [Phase 1]
        │
        ▼
  CareActionPredictor                              [Phase 3]
   ├── TFLite model (primary)
   └── fallback: CareActionRules                  [always]
        │
        ▼
  GameDirector (pure C++)
   └── highlight menu / suggest game / adjust difficulty
```

**Rule:** the network never touches GPIO or scene transitions directly — only suggests an enum your existing code already understands.

---

## 5. Build flags

| Environment | Flags | Purpose |
|-------------|-------|---------|
| `esp32-hw` | (default) | Normal firmware, no ML overhead |
| `esp32-tinyml-log` | `TINYML_DATA_LOG=1` | Emit training CSV on Serial |
| `esp32-tinyml` | `TINYML_INFERENCE=1` | Run TFLite care-action model |

---

## 6. Demo script (interviewer / video)

1. Show Status screen — hunger low.  
2. Return home — Totoro bubble: **“Maybe visit the market?”** + `[EAT]` highlighted.  
3. Turn off Wi‑Fi (not used for this model).  
4. Optional dev overlay: `EAT 0.88 · 4.2 ms · 2.1 KB model`.  

---

## 7. What we deliberately defer

- Engaged / Frustrated / Bored classifier  
- ESP-DL as first deployment path  
- On-device retraining  
- Post-game emoji label UI  
- Third model (game recommendation)  

---

## 8. Next implementation tasks

1. Call `MLDataLogger::logGameEnd(...)` from each mini-game via a thin wrapper.  
2. Call `MLDataLogger::logHubVisit(...)` when entering `Scene_PetTotoro`.  
3. Play 20 sessions → `python tinyml/download_serial.py > data/raw/sessions.csv`  
4. Train → drop `care_action.tflite` into `src/ml/models/`  
5. Wire `CareActionPredictor` into pet hub UI.

See `GameBase/tinyml/README.md` for commands.
