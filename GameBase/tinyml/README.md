# TinyML — PC-side training pipeline

Two models live here, both trained and running on hardware. The **care-action
classifier** is everything up to the checklist; the **touch-gesture classifier**
is [further down](#touch-gesture-model). See
[`../docs/TINYML_PLAN.md`](../docs/TINYML_PLAN.md).

## Care-action classifier

Care-action classifier for Pet Totoro.

Features and labels follow `CareActionRules.h`, which serves whichever of the
three visible stats reads **lowest** on the shared 0..100 scale:

| Priority | Condition (0..100 stats) | Label |
|----------|--------------------------|-------|
| 1 | `hunger < 15` — starving, and it steepens the happiness decay | `eat` |
| 2 | `cleanness < 10` — filthy, same penalty | `bath` |
| 3 | hunger is the lowest stat | `eat` |
| 3 | cleanness is the lowest stat | `bath` |
| 3 | happiness is the lowest stat, and `excitement < 40` | `play` |
| 3 | happiness is the lowest stat, and `excitement ≥ 40` | `pet` |

Raw values are compared rather than shortfalls against a care target, so the
suggestion keeps tracking the neediest stat even when the pet is thriving: at
hunger 94 / cleanness 88 / happiness 100 the answer is a bath. Ties go to the
stat that will fall first — hunger drains faster than cleanness at either growth
stage, and both outrun the happiness drift (see the decay rates in `PetSim.h`).

`play` is only ever suggested when the pet can pay a round's hunger/cleanness
cost (`hunger > 18` and `cleanness > 16`); otherwise it falls back to `pet`.

One rule sits outside that ranking. A meal has to be bought, so when the answer
is `eat` but the purse holds less than the cheapest item on the shelves (5 coins,
broccoli — read from `GroceryFoods.h` so re-pricing cannot leave the rule
behind), the suggestion becomes `play`, which is how coins are earned.

That one is a **constraint on the final answer** — `affordableAlternative()`, not
part of `suggest()`. The coin balance is not one of the 8 features, so folding it
into the labels would teach the model to answer `play` on hungry rows for a reason
it cannot observe. `CareActionPredictor` instead applies it to whichever action
won, model or rules, so it is enforced rather than learned and **changing it
needs no retraining**. `MLDataLogger` keeps labeling without it, on purpose.

**Excitement is hidden from the player UI** but is logged on Serial for training.
It is not ranked as a need of its own: it decays ~1.5%/min, so it sits near zero
between games and would otherwise make `play` the answer almost every time.

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
pio device monitor -b 115200 | python download_serial.py >> data/raw/sessions.csv
```

(Ctrl+C when done.)

`>>` appends, so several sittings accumulate in one file — `prepare_dataset.py`
skips the repeated header rows. Use `>` only to start over; it truncates the file
the moment you press Enter, before the device has sent anything.

The rows themselves go into the file, so the terminal stays quiet apart from the
monitor's own boot output. `download_serial.py` therefore mirrors each captured
row to stderr with a running count, which the redirect does not swallow:

```
[   1] ML,74442,0,3,2,8,1,2,0.220,0.780,0.000,0.640,0,0.429,0,0.100,eat
[   2] ML,81953,0,3,2,8,1,2,0.220,0.780,0.000,0.640,0,0.429,0,0.100,eat
Captured 2 ML rows.
```

If the count never moves while you enter the pet home or leave a game, the
running firmware is not the logging build — reflash with
`pio run -e esp32-tinyml-log -t upload`. Prefer `tee`? Add `-q` to drop the
mirror and avoid double echo:

```bash
pio device monitor -b 115200 | python download_serial.py -q | tee -a data/raw/sessions.csv
```

### Which rows to use

| `event` | When logged | Use for training? |
|---------|-------------|-------------------|
| `0` | Hub visit (after `applyGameReward`) | **Yes — kept by default** |
| `2` | Care state (after a feed / pet / bath landed) | **Yes — kept by default** |
| `1` | Game end (pre-reward stats) | Optional (`--all-events`) |

Hub rows have correct post-game hunger, cleanness, and excitement, but they are
written moments after the reward tops happiness up, so on their own they cluster
at the high end of every stat. Care rows sample the pet right after a stat moved
the other way (a bath, or one of the vegetables that costs happiness), which is
what stops the `happy` column from being a constant.

---

## Step 3 — Prepare real CSV

```bash
python prepare_dataset.py data/raw/sessions.csv
# → data/processed/sessions.csv (at-home rows: events 0 and 2)
```

Options:

```bash
python prepare_dataset.py data/raw/sessions.csv -o data/processed/my_run.csv
python prepare_dataset.py data/raw/sessions.csv --all-events   # include game-end rows
python prepare_dataset.py data/raw/sessions.csv --relabel      # re-derive labels
```

The script reports how far the device `label` column agrees with the Python rule
oracle. Anything below 100% means the capture predates the current
`CareActionRules.h` — the features are still valid, only the labels went stale,
so `--relabel` recovers the session instead of re-collecting it:

```
Label agrees with rule oracle on 47.1% of rows
9 rows were labeled under a different rule — rerun with --relabel to replace them
```

### After changing `CareActionRules.h`

The rule lives in three places — `src/ml/CareActionRules.h` and the mirrors in
`generate_synthetic.py` and `prepare_dataset.py` — and every downstream artifact
is derived from it, so the whole chain has to be refreshed together:

```bash
python generate_synthetic.py                               # relabel synthetic rows
python prepare_dataset.py data/raw/sessions.csv --relabel   # relabel real captures
{ cat data/processed/synthetic.csv; tail -n +2 data/processed/sessions.csv; } \
  > data/processed/pooled.csv
python train_care_action.py --data data/processed/pooled.csv
python export_model_header.py                              # regenerate the C header
python eval_tflite.py --data data/processed/pooled.csv      # confirm agreement
```

Then reflash: a rule change that skips the model leaves the two disagreeing, and
the model wins wherever it is confident.

Worth checking that the three copies really do agree, since nothing enforces it
at build time. Compile `CareActionRules.h` on the host against a stub `Arduino.h`
with `PetTotoroState::stats()` returning a settable struct, sweep hunger,
cleanness, happiness and excitement across their range, and diff the answers
against both Python mirrors. The current rule matches on all 64,827 grid points.

---

## Step 4 — Train on real data

```bash
python train_care_action.py --data data/processed/sessions.csv
```

Or train directly from a raw capture (at-home rows only). `--relabel` works here
too, so a stale capture can go straight into training without a prepare step:

```bash
python train_care_action.py --data data/raw/sessions.csv
python train_care_action.py --data data/raw/sessions.csv --relabel
```

Training needs at least ~40 rows and stops with a count if the capture is short.
It opens by printing the spread so imbalance is visible before the epochs scroll:

```
Label counts: eat=44, play=14, pet=0, bath=5
Warning: no rows labeled pet. The model cannot predict those actions — collect
sessions that reach those states.
```

### Missing a label

A short session rarely covers all four. `pet` is the hardest to provoke: it needs
happiness to be the *neediest* stat while excitement is still above 40, and
happiness takes roughly two days to drift down, so a fresh save almost never gets
there. Rather than farming for it, pool the capture with the synthetic set, which
covers every branch of the rule by construction:

```bash
python generate_synthetic.py
python prepare_dataset.py data/raw/sessions.csv
{ cat data/processed/synthetic.csv; tail -n +2 data/processed/sessions.csv; } \
  > data/processed/pooled.csv
python train_care_action.py --data data/processed/pooled.csv
```

Both files share the narrow schema, so dropping the second header is all it takes.
The synthetic rows dominate at that ratio — they are there for coverage, not to
replace real data — so re-pool as real captures grow.

---

## Step 5 — Deploy to the console

Training leaves three artifacts in `models/`: `care_action.tflite` and the
scaler's `scaler_mean.npy` / `scaler_scale.npy`. All three matter. The network was
fitted on standardized features, so the firmware must apply `(x - mean) / scale`
before every inference; ship the weights without the scaler and the predictions
are quietly wrong rather than obviously broken. The exporter bundles them into
one generated header:

```bash
python export_model_header.py          # -> src/ml/models/care_action_model.h
```

Check what the device will actually do before flashing. This runs the same
flatbuffer through the same scaler and the same 0.65 confidence gate as
`CareActionPredictor`, which the Keras accuracy from training does not:

```bash
python eval_tflite.py --data data/processed/pooled.csv
```

```
Model agrees with oracle:       95.9%
Confident enough to drive UI:   94.3%
Agreement when confident:       97.8%
Deployed behaviour vs oracle:   98.0%
```

Then flash the inference build:

```bash
pio run -e esp32-tinyml -t upload            # inference only
pio run -e esp32-tinyml-both -t upload       # inference + CSV logging
```

Open the pet's radial menu and the suggested action gets a yellow ring. On boot
the predictor prints its arena usage, which is the line to check if suggestions
look like the plain rules:

```
ML: care model ready, arena 916/4096 bytes
```

Any failure — schema mismatch, allocation failure, unexpected tensor shape, low
confidence — falls back to `CareActionRules` rather than showing nothing, so a
missing ring means the model declined, not that the UI broke.

Costs, measured on `esp32dev`: **+54 KB flash** (75.3% total, up from 71.2%) and
**+4.5 KB RAM** for the interpreter, the two kernels and the 3.4 KB model.
`AllOpsResolver` would cost ~237 KB instead — `CareActionPredictor` registers only
`FullyConnected` and `Softmax`, so adding a layer *type* to the model means
bumping the resolver count there.

> Rerun `export_model_header.py` after every training run. The header is generated
> and dated; a stale one silently deploys the previous model.

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
4. [ ] `pio device monitor | python download_serial.py >> data/raw/sessions.csv`
5. [ ] `python prepare_dataset.py data/raw/sessions.csv` (add `--relabel` if the
       capture predates the current `CareActionRules.h`)
6. [ ] `python train_care_action.py --data data/processed/sessions.csv`
7. [ ] Inspect classification report; re-collect if one label is weak
8. [ ] `python export_model_header.py && python eval_tflite.py`
9. [ ] `pio run -e esp32-tinyml -t upload`, then open the radial menu

---

# Touch-gesture model

Recognises *how* the screen was touched, so the pet can tell a poke from a stroke
from a scribble. Unlike the care model there is **no oracle**: the mapping from
finger motion to intent exists only in the player's hand, so real captured data
is the only ground truth and synthetic strokes are useless as training data. They
are still worth generating to smoke-test the plumbing, and augmenting the real set
(rotation, scale, timing jitter) is standard once it exists.

Current state, from 642 captured episodes across 8 sessions:

| | |
|---|---|
| accuracy, whole session held out | **91.1%** |
| accuracy, stratified random split | 96.9% |
| model size | 9.9 KB TFLite, float32 |
| cost on device | +66 KB flash, +9.7 KB RAM over `esp32-hw` |

Per-class recall on the honest split: `long_press` 95%, `double_poke` 97%,
`poke` 96%, `brush` 94%, `swipe` 93%, `zigzag` 92%, `circle` 88%, `unknown` 62%.

The gap between the two accuracies is the point of reporting both: episodes captured
back to back are near-duplicates, so a random split mostly measures how well the model
memorised the session it was trained on.

## What one sample is

A **stroke** is one continuous contact. An **episode** is one or more strokes
separated by gaps shorter than the episode window, closing once the screen has been
idle that long. A poke is one short stroke, a double poke is two strokes inside the
window, a brush is several strokes with a lift between them. The classifier runs
once per episode.

The window is **550 ms**, and it has to be, because it covers the slowest pause a
player leaves *inside* one gesture: the lift between the two taps of a double poke,
and the lift between two strokes of a brush.

### Segmentation latency, and why the window is no longer it

The obvious problem with a patient window is that it used to *be* the input latency —
nothing could be classified until the episode closed — so a window generous enough for
a slow double poke made every single tap feel late. Two rules take that cost away, and
they are the difference between the gestures feeling immediate and feeling broken:

- A **stationary episode is offered for classification as soon as the finger lifts**
  (`TouchSampler::previewReady()`), about 100 ms after a tap, without waiting for the
  window. The reading is provisional: a second tap withdraws it and the episode goes on
  to close as a double poke, so the player gets the poke reaction and then the double
  poke's. That is what a desktop click-then-double-click does too. Only stationary
  episodes are offered, because there the possibilities are benign — one stroke standing
  still is a poke or a long press, and the only thing it can grow into is a double poke.
  A travelled first stroke would preview as a swipe and could grow into a brush, which
  is a different action, so those still wait out the window.
- A **stationary episode closes as soon as its second stroke ends**, keeping the double
  poke at ~100 ms rather than ~650 ms. Nothing stationary has a third stroke, so the
  wait bought nothing but delay. The price is that a stationary three-tap can never be
  one episode; no class needs one.

Both stay segmentation rules rather than timing rules in disguise: poke, double poke and
long press all still reach the model intact, so neither can mask a distinction the model
is there to make. Previewing the first tap of all 93 captured double pokes through the
shipped model returns `poke` **93 times out of 93**, minimum confidence 0.94, so the
provisional reaction is never an incoherent one.

Because the preview absorbs the latency, there is now **one window instead of a short
stationary one and a long travelled one**. Earlier revisions had 300 ms / 550 ms split on
`GESTURE_TRAVEL_MOVING_PX`, purely to keep the poke snappy.

### How the double poke was actually fixed

Worth reading before touching these numbers, because the first two diagnoses were wrong.

On hardware the double poke needed an unreasonably brisk rhythm. The stationary window
went 300 → 500 ms, which produced **no improvement and a visibly sluggish single poke**.
That looked like proof the window was innocent, and suspicion moved to the touch
thresholds merging or dropping the second tap. It was neither. Two things were true at
once:

1. A **dispatch bug** in `Scene_PetTotoro` gated the gesture on `petPressed` still being
   clear. A stationary episode is ready ~48 ms after lift, often within the same tick as
   the release handling, so a large share of double pokes were classified and then thrown
   away. This is why widening the window appeared to change nothing.
2. A capture session of 58 deliberate double pokes put the real inter-tap gap at
   **184–327 ms**, with a third of them past 280 ms and nine landing at 300–327 ms.
   Against a 300 ms window that is textbook censoring: half of normal play was split into
   two single pokes, and *which* half depended on where the 16 ms poll happened to fall,
   so it failed at random. (The nine that recorded successfully above 300 ms are
   themselves the tell — `SAMPLER_GAP` only checks for expiry on a poll that sees no
   touch, so a late tap sneaks in whenever the previous poll was inside the window.)

The lesson is that the earlier capture had been performed briskly, at 95–187 ms gaps,
which made the boundary look safe with plenty of margin. **Capture at the rhythm you
actually play at**, or the dataset will hide exactly the timing the runtime gets wrong.

What was never the problem is the model. Time-shifting the second tap of every captured
double poke and re-running the shipped `gesture.tflite`:

| extra gap | recognised as `double_poke` | mean confidence | acted on (≥0.80) |
|---|---|---|---|
| +0 ms | 100% | 0.93 | 99% |
| +200 ms | 100% | 0.98 | 100% |
| +500 ms | 100% | 0.98 | 100% |

Confidence *rises* with the pause, because a longer gap makes a double poke less like
anything else, and `max_gap` saturates at `GESTURE_NORM_GAP_MS` (500 ms) rather than
running off into unseen territory. So a 2-stroke episode is recognised essentially
always, and a failing double poke is a **segmentation or dispatch** problem, never a
training one.

If one fails again, the capture screen answers it in one test: flash `esp32-gesture-both`,
select DOUBLE POKE and tap at the rhythm that fails. A rejection reports what it saw —
`needs 2 str, got 1 in 210 ms  drop z4`. `got 1` with a one-tap duration means the second
press was never seen (lower `TOUCH_FAST_Z_ENGAGE`); `got 1` with a duration spanning both
taps means they merged into one stroke (raise `TOUCH_FAST_Z_SUSTAIN`, or shorten the
`TOUCH_FAST_ENGAGE_HOLD_MS` grace). `got 2` and a rejection means the problem is
downstream of segmentation.

Gestures needing a response *while the finger is still down*, dragging above all,
cannot go through the classifier at all; they belong to a live tier that claims the
contact via `TouchSampler::abortEpisode()`.

Constants live in [`../src/ml/GestureEpisode.h`](../src/ml/GestureEpisode.h).

## Why the sampler bypasses `getTouch()`

`TFT_eSPI::getTouch()` averages five `validTouch()` passes, each with its own
`delay()` calls, so one poll costs roughly 10–30 ms. A poke can be over in 100 ms,
so that path cannot describe one. `sampleTouchFast()` in `main.cpp` instead reads
pressure once, takes two back-to-back raw positions, averages them, and rejects the
pair only if they disagree by more than `TOUCH_FAST_DEADBAND` raw counts. Pressure
uses hysteresis (`TOUCH_FAST_Z_ENGAGE` to start a stroke, `TOUCH_FAST_Z_SUSTAIN` to
continue) so a dip mid-brush does not chop one stroke into several.

`TouchSampler::poll()` is called from the scene-manager loop rather than the fixed
tick, and rate-limits itself to `GESTURE_SAMPLE_INTERVAL_MS` (16 ms, ~60 Hz).

Both guards started out far too strict and cost half the samples, holding the real
rate at 33 Hz. Worth knowing if they are ever retuned:

- The deadband is in **raw ADC counts**, and the panel spans ~3300 counts over
  240 px — about 14 counts per pixel. The original 40 counts was therefore ~3 px of
  tolerated disagreement, which back-to-back reads cannot meet, because `TFT_eSPI`
  lets the panel settle between its samples and this path cannot afford to.
- Losing pressure used to clear the engaged flag immediately, so resuming demanded
  the full firm-press threshold again and a momentary light spot mid-brush needed a
  real re-press. The flag now survives for the same window the segmenter uses to end
  a stroke.

The capture screen reports rejections per episode as `drop z<n> j<n>`, split by
guard, so a future regression here is visible on the device instead of needing a
capture-and-analyse round trip.

## Classes

| Label | Intent | Notes |
|-------|--------|-------|
| `poke` | get Totoro's attention | one short stroke, no travel |
| `double_poke` | open the menu | two strokes; **must** have 2, see below |
| `long_press` | grab, then drag | separable from `poke` by duration alone |
| `brush` | petting | repeated strokes, lifting between them |
| `swipe` | send Totoro away | fast and dead straight |
| `circle` | make Totoro dance | consistent winding in one direction |
| `zigzag` | make Totoro dance | turns as sharp as a circle's, but cancelling |
| `unknown` | ignore | stray contact, palm, mis-taps |

`unknown` matters more than it looks: without a reject class and a confidence
floor, every stray palm contact becomes a command. Below threshold the touch
falls through to the existing plain-tap handling.

It is also the only weak class (62% recall), which is expected — a catch-all has no
centre, only spread, and it overlaps `circle`, `zigzag` and `brush` in both
directions. Capture it as **deliberately varied**: palm edges, accidental grazes,
half-taps that slide, aimless drifts, knuckles, taps far apart. Forty similar blobs
teach the model that unknown means "blob", and every real stray touch still fires a
command.

### One structural rule

`gestureLabelMinStrokes()` refuses to log a `double_poke` with fewer than two
strokes. That is not a borderline example but a mislabel — it is indistinguishable
from a `poke` — and 35 of the first 96 double pokes captured were exactly that,
poisoning the very distinction the class exists to make. The capture screen shows a
red notice instead of confirming, and `prepare_gestures.py` drops any that predate
the guard, with a count.

Only *structural impossibilities* belong there. Rejecting a 200 ms long press or a
slow swipe would be hand-writing the classifier's decision boundary, which is the
model's job to learn.

## Collecting

The label is chosen **before** the gesture is performed, so every episode arrives
already labelled and nothing has to be reconstructed afterwards.

```bash
pio run -e esp32-gesture-log -t upload
cd GameBase/tinyml
mkdir -p data/raw
pio device monitor -b 115200 | python download_serial.py --gestures \
    >> data/raw/gestures.csv
```

On the device: **Settings → Gesture Capture**. Left/Right cycle the class, the box
records, UNDO discards the last episode, Home returns. The status line under the
box reports points, duration, stroke count and effective sample rate for the last
capture, so a bad threshold or a mis-segmented stroke shows up immediately rather
than after the fact.

Aim for **50–100 episodes per class**, spread over several sittings and different
days, with a deliberately sloppy hand some of the time. Forty immaculate circles
drawn in one focused minute will not recognise the circle you draw while
distracted. Roughly 20–30 minutes of tapping covers the whole set.

`--gestures` uses `>>` above on purpose: appending several sessions to one file is
expected, and the `boot` column (a fresh random tag per capture session) keeps
episode numbers unambiguous across reboots.

### Interleave the classes within a session

Capture a few of every class, cycle round, repeat — do **not** do forty of one class
and then move on. Six of the first seven sessions held a single class each, which
confounds session with label: any day-to-day artefact becomes a class cue, and
holding a session out to measure honestly also removes most of a class from
training. It is the difference between a number you can trust and a number you have
to argue about.

## Raw points, not features

The firmware emits one row per sampled point rather than a feature vector:

```
MLG,boot,episode,label,stroke,t_ms,x,y
```

The feature extractor changed several times while the model was tuned, and
re-recording hundreds of gestures each time is not a reasonable way to spend an
evening. UNDO emits a `DISCARD` row for the episode, since its point rows are
already on the wire; the parser drops the whole episode.

## Features

[`../src/ml/GestureFeatures.h`](../src/ml/GestureFeatures.h) is the **only**
implementation. It has no Arduino dependency, so `prepare_gestures.py` compiles it
into a host binary and pipes the raw capture through it — the training features are
the on-device features by construction, rather than by a parity harness like the one
`CareActionRules.h` and `prepare_dataset.py` need.

40 features: 12 points resampled by **arc length** (shape), plus 16 scalars
(dynamics and scale). Resampling by path length rather than sample index is what
lets 33 Hz and 60 Hz captures share one dataset — worth preserving, since the set
contains both. For the same reason, **no feature may depend on the sample count**.

There is deliberately no scaler: every feature is normalised into a comparable range
by the caps in that header, so nothing fitted has to be shipped or kept in step.

Two design points that came out of real data:

- **A gesture under `GESTURE_MIN_SPAN_PX` (20 px) of extent has no trajectory**, and
  every path-derived feature reads zero for it. A real long press covers an 11×11 px
  box while a held finger's jitter accumulated 66 px of "path" and saturated the
  turning features, making it identical to a brush on everything but path length.
- **Winding and absolute turning are separate features.** A circle accumulates about
  a full turn in one direction; a zigzag's turns are just as sharp but cancel. Either
  alone confuses the two, and the pair separates them cleanly (circle winding −0.55,
  zigzag 0.01).

## Inspecting a capture

```bash
python gesture_summary.py data/raw/gestures.csv
python gesture_summary.py data/raw/gestures.csv --show circle --count 3
```

Prints per-class episode counts with median points, duration, stroke count, path
length and sample rate, and can render an episode as ASCII to confirm the shape
survived. Check that the median rate sits near 60 Hz and that `double_poke` really
reports 2 strokes before collecting in bulk.

## Training

```bash
python prepare_gestures.py data/raw/gestures.csv   # raw points -> features
python train_gesture.py                            # both accuracy estimates
python export_gesture_header.py                    # -> src/ml/models/gesture_model.h
```

`train_gesture.py` reports two numbers, and the gap between them is the point:

- a **stratified random split**, which is optimistic because episodes captured back
  to back are near-duplicates, so part of the test set is effectively in training;
- **leave-one-session-out**, holding out a whole capture session, which is what
  predicts the device's behaviour.

If they diverge widely, the model has learned about sessions rather than gestures.
They currently sit 1.7 points apart.

> One trap worth knowing, because it cost a full debugging cycle and looks exactly
> like a data problem. Keras `validation_split` takes the **last** fraction of the
> array without shuffling, and captures arrive grouped by class, so it hands over a
> single-class validation set. Early stopping then tracks a meaningless loss and
> `restore_best_weights` returns a barely-trained model: honest accuracy read 57.7%
> instead of 87.9%, the worst fold 10.9%, and nothing cleared 0.5 confidence. `fit()`
> builds a stratified split itself for this reason — do not replace it with
> `validation_split`.

### Never quantize for this TFLM build

`tf.lite.Optimize.DEFAULT` is **not** used, and must not be added back. It
dynamic-range quantizes weight tensors above a size threshold and leaves smaller
ones float32, so a model can come out part int8. That needs the int8-weight,
float-activation `FullyConnected` kernel, which the `TensorFlowLite_ESP32` snapshot
does not implement — and it fails by returning **NaN**, not by refusing to run.

NaN is uniquely nasty here because it defeats the checks meant to catch a bad model.
No comparison against NaN is true, so the argmax never moves off index 0; and
`NaN < 0.8` is false, so the confidence floor passes it. A completely dead model
therefore presented as a *confident* `poke` — the first label in the enum — with its
confidence rendering as `4294967295%`, since casting a non-finite float to unsigned
is undefined.

Three defences now, because the failure was silent:

- `export_gesture_header.py` refuses to emit a header for a model containing
  non-float weights, naming the offending tensor.
- `GesturePredictor` checks the output for finiteness *before* the argmax, and warns
  once on serial.
- The capture screen clamps the percentage instead of casting through.

The care-action model has always been small enough to sit under the quantization
threshold, so it was never affected — the flag is removed there too rather than left
armed for whenever that network grows.

Brushing was first captured as one continuous scrub and later by lifting between
strokes. Those are different gestures, not variants: contact 100% of the time versus
45%, one stroke versus four. Held out, 12 of the 14 continuous ones were misread
while every other class in that fold scored 88% or better, so they are dropped by
default (`--keep-continuous-brush` to keep them).

## On the device

```bash
python export_gesture_header.py
pio run -e esp32-gesture-both -t upload
```

`esp32-gesture-both` runs the model **and** keeps the capture screen, which shows the
model's answer next to the gesture you meant: green when it agrees, red when it does
not, grey when it declined to act, plus a running agreement tally and the best guess
even when it was ignored. That is the fastest way to find the classes that still need
data, and much better evidence than an offline number.

`esp32-gesture` is the playable build: the same model, without the capture screen.

`GesturePredictor` has **no rule fallback**, unlike `CareActionPredictor`. A rule
good enough to tell a circle from a zigzag from a stray palm is precisely what the
model exists to avoid writing, so when the model is missing or unsure the answer is
"no gesture" and nothing happens — which is also what a genuinely unrecognisable
scribble produces, so the two look identical from the outside.

`GESTURE_MIN_CONFIDENCE` is 0.80. Measured on the honest split:

| threshold | acted on | correct | wrong actions |
|-----------|----------|---------|---------------|
| 0.70 | 95% | 93.2% | 6.5% |
| 0.80 | 92% | 94.3% | 5.2% |
| 0.90 | 85% | 95.4% | 4.0% |
| 0.95 | 74% | 96.4% | 2.7% |

A rejected gesture costs the player a repeat; an accepted wrong one makes the pet act
unasked, which reads as the toy being broken. Those are not equally bad, which is why
the threshold sits above the accuracy-maximising point.

## What the gestures do

`Scene_PetTotoro` binds the classes, in `handleGesture()`. Everything here is
compiled only under `TINYML_GESTURE_INFERENCE`, so `esp32-hw` behaves exactly as it
always did.

| Gesture | Where | Totoro |
|---------|-------|--------|
| poke | on the pet | stops, stands, turns to face you (~100 ms after the tap) |
| poke | empty floor | walks over and stands on that spot |
| double poke | on the pet | says a greeting in a speech bubble |
| hold, then let go | on the pet | opens the radial menu — **live tier**, see below |
| hold, then move | on the pet | picks it up to carry — **live tier** |
| brush | on the pet | petting: `doPet()`, love note, a happy wiggle |
| swipe | on the pet | trots to the far wall and sulks facing it |
| circle / zigzag | anywhere | dances for 4.5 s |
| unknown, or below 0.80 | — | nothing |

"On the pet" is the existing `tapOnPet` box (the sprite plus 12 px) tested against
both the episode centroid **and** its first point, because a brush wanders off the
body and a swipe is meant to end away from where it started.

A double poke shows **both** reactions in sequence — Totoro looks up on the first tap,
then greets you — because the first tap is classified on its own before the second
arrives. That is intentional, and `Scene_PetTotoro` keeps the start time of the tap it
reacted to so the same single tap is not acted on twice when its window later expires
with no follow-up.

Nothing here moves a stat except brush, which calls the same `doPet()` the menu does
and so inherits its three-per-minute cap. Poke deliberately gives nothing: a free tap
that raised happiness would make feeding, bathing and the games pointless. Swipe
deliberately costs nothing either — stats already fall on their own when the pet is
ignored, so charging for the gesture would bill the player twice for one thing.

### Why the menu is not on a gesture

It was on the double poke, and that was the wrong place for it. Not because the double
poke is badly recognised — a two-stroke episode is classified correctly ~97% of the time
— but because **the menu is the one interaction with no workaround**. Every other
gesture can be repeated for free: a missed poke means Totoro did not look up, and a
missed brush means one fewer pet. A missed menu means the player cannot feed, wash or
play, and the toy reads as broken. So it belongs on the most deterministic input
available, not on the most expressive one.

That is a hold, decided in the live tier without the model, and it is why
`GESTURE_LONG_PRESS` still does nothing in `handleGesture()`. The class stays in the
training set regardless: without it the model would be forced to file every hold under
one of the other seven.

### The live tier

Dragging cannot go through the model, not even through the early preview: both fire on
*lift*, and a pet that only starts following your finger after you let go is broken.

One hold serves both the menu and carrying, because the two are told apart by what the
finger does *next*, the way long-press-then-drag works on a phone:

1. `TouchSampler::contactHoldMs()` reaches `PET_GRAB_HOLD_MS` (400 ms) with the episode
   still stationary → the hold **arms**: a short blip acknowledges it and
   `TouchSampler::abortEpisode()` takes the contact away from the classifier, whichever
   way it goes from here.
2. The finger moves `PET_CARRY_BREAK_PX` (10 px) from where it armed → `startDrag()`,
   and Totoro is in hand.
3. The finger lifts instead → `openMenu()`.

The blip matters: without it a hold looks ignored until release, since nothing visible
can happen at step 1 while both outcomes are still open. The anchor for step 2 is where
the finger was when it *armed*, not where it first landed, so a hold that drifted slowly
under the 80 px travel budget does not instantly count as having moved.

**Travel alone never starts a drag in gesture builds.** The old rule — a press that
moves 6 px becomes a drag — had to go, because it claimed every stroke that began on the
pet before the classifier could see it, so a swipe across Totoro *was* a drag and a brush
could never be petting. Non-gesture builds keep the travel trigger and open the menu on a
plain tap, exactly as before.

Arming has to be distinguished from a slow brush stroke over the same spot, which
duration alone cannot do: 10 captured brushes hold one stroke past 400 ms. Travel
separates them completely. At the instant contact reaches 400 ms, measured as **path
length** — a brush that strokes back and forth ends up near where it started, so
displacement would read it as a hold:

| | travel covered |
|---|---|
| `long_press`, 40 captures that get that far | 5–23 px |
| every brush/circle/zigzag/scribble that lasts that long | 96–599 px |

`PET_GRAB_MAX_TRAVEL_PX` is 80, in a gap that nothing in 642 episodes occupies. No poke,
double poke or swipe stays down for 400 ms at all.

Both the duration and the travel are measured from the **scene's own** touch reads on the
50 ms tick, not from `TouchSampler`. That is deliberate: the fast sampler only engages
above `TOUCH_FAST_Z_ENGAGE`, so a press too light for it would otherwise leave the pet
with no reachable menu, and a stale travel figure from the previous episode could block
arming outright. The figures above are resampled to that same 50 ms tick, which widens
the gap rather than narrowing it.

Everything else that owns a touch aborts the episode too: the radial menu, the play
picker and cleaning soot. Without that, a poke that dismissed the menu would also poke
the pet a moment later. Dismissing works as it always did — anything that is not an icon
closes the overlay — so a poke off to the side is enough.

### Tap-to-open-menu is gone in gesture builds

A single tap on Totoro used to open the menu on release. A single tap is now a poke, and
the two cannot both fire, so the menu moved to a hold. The Home button still opens it
instantly either way. Non-gesture builds keep the old tap.

### Sampling has to survive a repaint

`TouchSampler::poll()` is called once per `loop()`, so its real interval is the frame
time, not `GESTURE_SAMPLE_INTERVAL_MS`. Measured on the *capture* screen, which draws
almost nothing, **38% of samples already land more than 32 ms apart and the worst gap is
95 ms** — the sampler is running nearer 30 Hz than 60. The pet room draws a great deal
more per frame, and a tap only lasts 60–100 ms, so an entire second tap of a double poke
could begin and end inside one frame and never be seen at all. That is the most likely
reason gestures behaved noticeably worse in the room than on the capture screen.

`GameScene::serviceTouchSampler()` fixes it by sampling between rows of a repaint,
closing and reopening the SPI transaction around the read. It is safe there because
`renderScene()` sets its address window per span anyway, and it costs nothing on rows
where no sample is due (`TouchSampler::pollDue()`). `renderFullScreen()` deliberately
does *not* do this: it holds one address window across all 320 rows, so breaking the
transaction mid-stream would corrupt the frame. It is only used for one-off repaints, not
for animation.

### Commanded poses are deadlines, not a mode

`commandPose()` sets the pose and pushes `nextPoseMs` out, rather than setting a
"commanded" flag that `updatePose` would have to respect. Idle posing then resumes on
its own, and hunger and sickness keep overriding poses exactly as they do normally —
which is the behaviour you want: a starving Totoro should clutch its belly whatever
you drew at it. `commandPose()` refuses outright when the pet is sick, because
`updatePose` skips a sick pet entirely and the pose would stick for good.

Walking to a poked spot reuses the ordinary `TOTORO_POSE_WALK` (so the two-frame walk
animation and wall clamping come for free) plus a target x; `updateWalkTarget()` stops
it on arrival and gives up if anything else takes the pose over.

### Cost

`esp32-gesture` uses 1001585 bytes of flash against 933409 for `esp32-hw`: **+66 KB**
for the interpreter, two kernels and the 9.9 KB model. RAM is +9.7 KB, most of it the
8 KB arena.
