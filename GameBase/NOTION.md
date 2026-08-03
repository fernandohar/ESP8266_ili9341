# Notion Demo Page — Copy-Ready Content

Paste sections below into your Notion page. This matches your current structure but
corrects inaccuracies and fills placeholders based on the actual firmware.

---

## Overview

A custom 2D game engine built to run on **ESP32** microcontrollers, paired with a
complete **handheld game console** demo: a Studio Ghibli–themed virtual pet hub,
three mini-games, and a grocery store — all on a 240×320 ILI9341 TFT with
resistive touch.

**Goals**

- Smooth 2D rendering on constrained hardware
- Simple workflow for building small games/demos
- Support multiple small game genres out of the box (e.g., **digital pet**, **tic-tac-toe**, **whack-a-mole**, **catch/collection arcade**)
- Keep **input polling** and **simulation/game speed** deterministic even when rendering slows down (render frame skipping / decoupled update)
- Overcome slow **SPI TFT** full-frame refresh by avoiding whole-screen redraws (reduce flicker via **dirty-rectangle / partial updates**)
- Support **stacked sprite compositing** (avatars + parented attachments, z-ordered with 1-bit opacity masks)

**Current scope**

- 2D rendering engine with **sprite** support (PROGMEM RGB565 + 1-bit masks)
- **Parented attachments** (e.g. food held by the pet during eating animation)
- **Sprite sheets** — one bitmap, many sub-regions (growth stages, soot variants, UI digits)
- **Tiled backgrounds** — small repeating tiles (~5 KB) instead of full-screen bitmaps (~150 KB)
- **Rectangle collision detection** (AABB) + **circle tests** for physics
- **Mask-based hit testing** for touch (per-pixel opacity lookup after AABB reject)
- **Scene swapping** (scene manager, up to 10 scenes; 7 registered today)
- **Minimal rendering** with a **dirty-rectangle compositor**
- **Input**: resistive touch (XPT2046) + optional 3-button pad (Left / Home / Right)
- Deterministic **20 Hz game loop** that prioritizes update/input when rendering becomes expensive
- **Non-blocking audio** via FreeRTOS tone queue
- **NVS persistence** for pet progress and touch calibration

### Demo game

Built a **Pet Totoro virtual pet** hub on top of the engine, with three mini-games:

| Scene | What it demonstrates |
|-------|---------------------|
| **Pet Totoro** (hub) | Radial menu, stat decay, growth stages, soot cleaning, touch-drag movement, NVS save |
| **Acorn Catch** | Physics, multi-avatar animation, game modes, score HUD with sprite digits |
| **Tic-Tac-Toe** | Tiled background + overlay grid, AI opponent, mask-based token placement |
| **Whack-a-Mole** | Timed rounds, difficulty scaling, mask-based tap targets |
| **Grocery** | Paginated store UI, coin economy, cross-scene handoff (PendingMeal → eating animation) |
| **Status / Settings** | Static screens, calibration, factory reset |

All scenes return to the hub via **Home**. Progress saves to ESP32 NVS on every scene change.

---

## Demo

<aside>
▶️

**Suggested caption:** Tap Totoro to open the radial menu → pick a mini-game → earn coins → buy food at the grocery → return home to watch Totoro eat. The engine repaints only dirty sprite regions, so animation stays smooth on SPI TFT.

</aside>

- **Video**: (record Wokwi sim or physical board — see Try it below)
- **Playable (no hardware)**: build `esp32-wroom` and run in [Wokwi](https://wokwi.com) via `GameBase/diagram.json`
- **Repo**: https://github.com/fernandohar/ESP8266_ili9341

**Screenshots in repo** (upload to Notion):

- `GameBase/assets/Screenshot_*.png` — in-game captures
- `GameBase/docs/images/wiring_display.png` — hardware wiring
- `GameBase/assets/sprite_ttt_tokens_preview.png` — Tic-Tac-Toe tokens

---

## Features

### Rendering

- Sprites — PROGMEM RGB565 blitting with 1-bit opacity masks
- Parented **attachments** (child sprites follow a parent avatar)
- **Sprite sheets** — sub-rectangle extraction without duplicating pixel data
- **Dirty-rectangle compositor** — repaints only spans affected by moved/animated sprites
- **Tiled backgrounds** — modulo-wrapped repeat tiles (50×50 grass ≈ 5 KB vs 150 KB full screen)
- **Transparency** — mask-based; handles cases where opaque white ≠ transparent sentinel
- Horizontal flip (`flipX`) for facing direction
- Multi-frame animation (up to 50 frames per avatar)
- ~~Tilemap engine~~ — not a general tilemap; scenes use tiled backgrounds + avatar overlays
- ~~Camera / parallax~~ — not implemented

### Game framework

- Scene/state system — `GameSceneManager` with init / update / render / destroy lifecycle
- Collision — AABB + circle tests with impulse resolution (`Physics.h`)
- Touch hit testing — AABB reject, then mask pixel lookup (`Avatar::contains()`)
- Input — touch (primary) + debounced buttons (Left GPIO13, Home GPIO27, Right GPIO14)
- Game loop — deWiTTERS constant game speed; skips up to 5 render frames before forcing a draw
- Audio — queued tones via FreeRTOS task on ESP32 (`SoundPlayer`, GPIO16 piezo)
- Persistence — NVS for pet stats/coins (`PetSave`) and touch calibration (`TouchCalibration`)
- Cross-scene handoffs — `GameResult` (mini-game rewards), `PendingMeal` (grocery → eating)

### Tools

- **Asset pipeline** — Python scripts in `tools/` convert PNG → PROGMEM headers (`png_to_spritesheet.py` + purpose-built generators)
- ~~Level editor~~ — not implemented
- ~~Debug overlays (FPS, heap)~~ — not implemented (commented debug hooks exist in scene manager)

---

## Architecture

### Hardware + display

- **Started on:** ESP8266 (early prototype; `nodemcuv2` env still in `platformio.ini`)
- **Current target:** ESP32-WROOM-32 DevKit (`board = esp32dev`)
- **Display:** ILI9341, 240×320 portrait, SPI @ 27 MHz
- **Touch:** XPT2046 resistive (physical) / FT6206 capacitive (Wokwi simulator)
- **Update rate:** 20 Hz fixed tick (`UPDATES_PER_SECOND = 20`)
- **Render rate:** ~20 FPS (50 ms) or ~30 FPS (33 ms) on urgent `requestRender()`

### Software stack

- **Framework:** Arduino (PlatformIO, `espressif32`)
- **Graphics driver:** TFT_eSPI v2.4.2 (vendored + patched in `lib/TFT_eSPI/`)
- **Memory strategy:**
  - All sprites/backgrounds in **flash** (PROGMEM) — ~2.8 MB of generated headers
  - **No full-screen framebuffer** — two line buffers (`renderbuf[2][240]`) for dirty-rect compositing
  - No PSRAM required
  - Tiled backgrounds to minimize flash usage per scene

### Scene map

| Index | Scene | Role |
|-------|-------|------|
| 0 | Pet Totoro | Hub — virtual pet home + radial menu |
| 1 | Acorn Catch | Mini-game |
| 2 | Settings | Calibration + factory reset |
| 3 | Tic-Tac-Toe | Mini-game |
| 4 | Whack-a-Mole | Mini-game |
| 5 | Status | Read-only pet stats |
| 6 | Grocery | Food shop |

> The old map-style location hub has been removed. Pet Totoro is the sole navigation center.

---

## Performance notes

### Frame skipping / timing behavior

The engine uses a **fixed 20 Hz update tick** (50 ms). Rendering runs asynchronously
at ~20 FPS. When update + logic consume multiple ticks before the next render slot,
the scene manager **skips render frames** (up to `MAX_FRAMESKIP = 5`) so that:

- **Game update speed** stays consistent (simulation time stays accurate)
- **Input polling** remains responsive (touch/buttons polled once per tick, not per loop iteration)

After 5 skipped frames, a render is forced and the game visually slows down.

### Dirty rectangles with stacked sprites

Avatars are z-ordered in an array; the compositor unions bounding boxes of sprites
that moved or changed animation frame, then repaints only those screen spans row-by-row.
Parented **attachments** (e.g. food during eating) inherit the parent's dirty region.
Opacity masks ensure only visible pixels are drawn — no full-screen clears.

| Scenario | Resolution | Update | Render | Notes |
| --- | --- | --- | --- | --- |
| Pet Totoro hub (8 soot + animated Totoro) | 240×320 | 20 Hz | ~20 FPS | Dirty-rect; forest background is a full bitmap |
| Acorn Catch (Mei + Chu + falling objects) | 240×320 | 20 Hz | ~20 FPS | Physics + multi-avatar animation |
| Tic-Tac-Toe / Whack-a-Mole / Grocery | 240×320 | 20 Hz | ~20 FPS | Tiled grass background (~5 KB tile) |
| Static screens (Status, Settings) | 240×320 | 20 Hz | draw once | `initScene()` draws; `render()` is no-op |

### ESP8266 → ESP32 migration

Early work ran on ESP8266 (80 MHz). Migration to ESP32 was driven by GPIO
availability, FreeRTOS (non-blocking audio), and NVS persistence — not primarily
for rendering speed. The `nodemcuv2` environment remains but is not the active target.

---

## Example code

Real structure (simplified from `main.cpp` + `GameSceneManager.h`):

```cpp
// Construct in setup(), NOT as globals (ESP32 boot crash otherwise)
TFT_eSPI *tft = new TFT_eSPI();
GameSceneManager *manager = new GameSceneManager(tft, TOUCH_IRQ, isTouching);

void setup() {
  tft->init();
  TouchCalibration::loadOrCalibrate(tft);
  PetSave::load();

  manager->appendScene(new Scene_PetTotoro(...));
  manager->appendScene(new Scene_AcornCatch(...));
  // ... register all scenes ...
  manager->startScene(SCENE_PET_TOTORO);
}

void loop() {
  manager->update();  // fixed 20 Hz tick + decoupled render
}
```

Scene pattern:

```cpp
class Scene_AcornCatch : public GameScene {
  void initScene() {
    setBackground(acorn_catch_bg);
    appendAvatar(new Avatar(...));  // Mei, Chu, acorns, soot
  }
  void update(bool isTouching, bool *needChangeScene, int *nextSceneIndex) {
    // physics, input, game logic
    if (gameOver) { *needChangeScene = true; *nextSceneIndex = SCENE_PET_TOTORO; }
  }
  void render() { renderScene(); }  // dirty-rect, NOT full screen every frame
};
```

---

## Roadmap

- [ ] **Tetris** — listed as a goal genre; not yet implemented
- [ ] Restore **map-style hub** as an optional scene (removed in favor of Pet Totoro radial menu)
- [ ] Enable **DS3231 RTC** for real offline pet stat decay (`PET_USE_RTC` in `PetClock.h`)
- [ ] Grocery item pricing (currently all items cost 0 coins — testing mode)
- [ ] Debug overlay — FPS counter, heap monitor (hooks exist but commented out)
- [ ] Additional mini-games using the existing scene/avatar framework

---

## Credits / Inspiration

- **[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)** — display driver (vendored + patched for `TOUCH_CS`)
- **[komrad36/ArduinoPhysics](https://github.com/komrad36/ArduinoPhysics)** — impulse collision resolution adapted in `Physics.h`
- **[deWiTTERS game loop](https://www.koonsolo.com/news/dewitters-gameloop/)** — constant game speed with max FPS / frame skip
- **[Wokwi](https://wokwi.com)** — ESP32 + ILI9341 capacitive-touch simulator (`diagram.json`)
- Studio Ghibli characters used as fan-art demo theme (Totoro, Mei, Cat Bus, soot sprites)

---

## Contact

- GitHub: https://github.com/fernandohar
- Repo: https://github.com/fernandohar/ESP8266_ili9341

---

## Fix checklist (what was wrong in the original draft)

- [x] Remove **Tetris** from "current scope" — not built yet (move to Roadmap)
- [x] Replace "multi-layer sprite compositing" with accurate description (attachments + z-ordered avatars + masks)
- [x] Hub is **Pet Totoro**, not a map-style location menu
- [x] Add missing scenes: **Grocery**, **Status**, **Acorn Catch** game modes
- [x] Fill hardware placeholders (ESP32-WROOM, ILI9341 240×320 SPI, 20 Hz / ~20 FPS)
- [x] Fill software stack (Arduino, TFT_eSPI, PROGMEM + line buffers, no PSRAM)
- [x] Fill performance table with real scenarios
- [x] Replace generic pseudocode with actual project structure
- [x] Add repo link and Wokwi playable path
- [x] Document mask-based touch hit testing (not just "pixel-perfect collision" in physics)
