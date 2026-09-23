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
- [x] Hook logger from mini-game end + pet hub  
- [ ] Settings → “Export ML CSV” (optional)

**No neural network yet.**

### Phase 2 — Train on PC

- `tinyml/generate_synthetic.py` — prove pipeline  
- `tinyml/train_care_action.py` — Keras MLP → **TFLite** (`int8`)  
- Target: >90% on synthetic; then retrain on real Serial exports  

### Phase 3 — On-device inference (Arduino-friendly)

- [x] **`esp32-tinyml`** PlatformIO env with `TINYML_INFERENCE=1`
- [x] **TensorFlow Lite Micro** via `tanakamasayuki/TensorFlowLite_ESP32` — the
      Arduino-framework port; upstream TFLM and ESP-IDF ports do not build under
      `framework = arduino`
- [x] `tinyml/export_model_header.py` — 3.4 KB flatbuffer **plus the scaler
      constants** into `src/ml/models/care_action_model.h`
- [x] `CareActionPredictor::predict()` → `CareAction` + confidence, falling back
      to `CareActionRules` on any failure or when confidence < 0.65
- [x] `Scene_PetTotoro`: yellow ring on the suggested radial menu item
- [x] Verified on hardware — `ML: care model ready, arena 916/4096 bytes`, ring
      tracks the neediest stat
- [ ] Optional dev overlay showing confidence and inference time

Keep the 4 KB tensor arena: `arena_used_bytes()` reports 916, but the memory
planner's own bookkeeping comes out of the same arena, and trimming to 1280 broke
`AllocateTensors` with "Too many buffers (max is 4)".

Measured cost: **+54 KB flash** (71.2% → 75.3%) and **+4.5 KB RAM**, with only
`FullyConnected` and `Softmax` registered — `AllOpsResolver` costs ~237 KB.
`tinyml/eval_tflite.py` scores the exported model through the same scaler and
threshold the firmware uses: 99.0% agreement with the oracle, and the model is
confident enough to drive the UI on 95.7% of rows.

Hardware testing drove one rule change. The oracle originally ranked each stat by
its shortfall against a care target of 80, so a thriving pet (hunger 94,
cleanness 88, happiness 100) had no deficit to serve and got a distraction —
`pet` — instead of the bath its lowest stat called for. It now ranks the raw
values, which is what "suggest how to raise the lowest stat" actually means.

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

1. ~~Call `MLDataLogger::onGameEnd(...)` from each mini-game via a thin wrapper.~~  
2. ~~Call `MLDataLogger::onHubVisit(...)` when entering `Scene_PetTotoro`.~~  
3. ~~Play 20 sessions → `python tinyml/download_serial.py > data/raw/sessions.csv`~~  
4. ~~Train → drop `care_action.tflite` into `src/ml/models/`~~  
5. ~~Wire `CareActionPredictor` into pet hub UI.~~  
6. Flash `esp32-tinyml` and confirm on hardware: the boot line
   `ML: care model ready, arena N/4096 bytes` and a ring on the suggested action.
7. Collect more real rows (especially `pet`) and re-pool — synthetic rows still
   outnumber real ones roughly 19:1.

See `GameBase/tinyml/README.md` for commands.
