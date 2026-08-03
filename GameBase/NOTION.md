# Custom 2D Game Engine on ESP32

> Copy sections below into your Notion demo page. This file is the source of truth
> for project descriptions — keep it in sync when features change.

---

## One-liner

A **handheld game console** on an ESP32: custom 2D engine, resistive touch UI,
virtual pet hub, and three mini-games — all running on a 240×320 ILI9341 display
with ~2.8 MB of PROGMEM sprite assets.

---

## Project overview

This project is a **custom 2D game engine** written in C++ for the ESP32, paired
with a complete game experience themed around Studio Ghibli's forest spirits.

There is no third-party game framework. The engine provides:

- A **scene graph** with fixed-tick game loop (20 Hz)
- **Dirty-rectangle rendering** for flicker-free sprite animation on SPI TFT
- An **avatar system** with PROGMEM sprites, opacity masks, animation, and attachments
- Lightweight **2D physics** (AABB + circle collision)
- **Touch + button input** with NVS-persisted calibration
- **Non-blocking audio** via a FreeRTOS tone queue
- **NVS persistence** for virtual pet progress and economy

The player experience centers on **Pet Totoro** — a virtual pet hub — with radial
menu navigation to mini-games, a grocery store, stats, and settings.

**GitHub:** https://github.com/fernandohar/ESP8266_ili9341

---

## Hardware

| Component | Details |
|-----------|---------|
| **MCU** | ESP32 DevKit (ESP32-WROOM-32) |
| **Display** | ILI9341 240×320 TFT, portrait, SPI @ 27 MHz |
| **Touch** | XPT2046 resistive (physical) / FT6206 capacitive (Wokwi sim) |
| **Buttons** | 3× momentary (Left GPIO13, Home GPIO27, Right GPIO14) — optional |
| **Speaker** | Piezo buzzer on GPIO16 |
| **RTC** | Optional DS3231 for offline pet stat decay |

### Critical wiring note

**Do NOT connect the LCD SDO/MISO pin to GPIO19.** GPIO19 is reserved for the
touch controller's T_DO only. Connecting both breaks touch reads — the #1 cause
of "touch stopped working."

See `GameBase/docs/images/wiring_display.png` and `wiring_buttons.png` for diagrams.

---

## Try it (no hardware)

The project includes a **Wokwi simulator** configuration:

```bash
cd GameBase
pio run -e esp32-wroom
# Then: VS Code → "Wokwi: Start Simulator"
```

The simulator uses capacitive touch (no calibration needed) and matches the
physical firmware's scene logic.

---

## Game experience

### Hub — Pet Totoro (virtual pet home)

The forest room is the **central hub**. Tap Totoro to open a **radial menu**:

```
        [ Pet ]
  [Play]     [Eat]
[Set]  Totoro  [Info]
       [Bath]
```

| Action | Description |
|--------|-------------|
| **Play** | Choose a mini-game: Acorn Catch, Tic-Tac-Toe, or Whack-a-Mole |
| **Eat** | Open the grocery store to buy food |
| **Pet** | Boost happiness (rate-limited) |
| **Bath** | Clean Totoro, gain care XP |
| **Info** | View stats, growth stage, coins |
| **Set** | Touch calibration, factory reset |

**Virtual pet mechanics:**

- **Four stats:** Health, Hunger, Happiness, Cleanness (0–100, shown as pips)
- **Growth stages:** Baby → Junior (150 care XP) → Adult (500 care XP)
- **Life states:** Alive → Sick (health = 0, 60 s grace) → Escaped (requires reset)
- **Care actions:** Walk Totoro (touch-drag), clean soot, pet, bathe, feed
- **Stat decay:** Hunger ~45 s, Happiness ~60 s, Health ~30 s
- **Economy:** Earn coins in mini-games → spend at grocery → eating animation at home
- **Persistence:** All progress saved to ESP32 NVS on every scene change

### Mini-game 1 — Acorn Catch

Side-view catcher starring **Mei** vs rival **Chu Totoro**.

| Mode | Rules |
|------|-------|
| **Time Attack** | 30 seconds + 1 s bonus per acorn collected |
| **Survival** | 3 lives; dodge falling soot hazards |
| **Collector** | Reach 30 acorns to win |

Features: ramping fall speed, Chu can jump and steal acorns, sonic-style scatter
visual on hit, coin/happiness rewards on return to hub.

### Mini-game 2 — Tic-Tac-Toe

- **1P vs CPU** or **2P hot-seat**
- Grass tiled background + wooden grid overlay
- Tokens: Mei = O, Cat Bus = X (circular masked sprites)
- AI uses win/block heuristic with 35% fumble chance
- Win reward: 6 coins

### Mini-game 3 — Whack-a-Mole

- 30-second timed rounds on grass tile background
- 10 soot-mole variants, up to 3 active at once
- 5 difficulty levels (faster spawn/visibility per level)
- 1 coin per 2 hits

### Grocery store

- 12 food items (Broccoli, Cotton candy, Salad, Bun, Onigiri, Soft serve, Sushi,
  Hamburger, Dorayaki, Yam, Green onion, Ramen)
- Paginated 4×2 store UI
- Purchase → return to pet home → 3-frame eating animation attached to Totoro

---

## Engine architecture

```
┌─────────────────────────────────────────────────────────┐
│  main.cpp                                               │
│  Boot → Touch cal (NVS) → PetSave load → SceneManager   │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  GameSceneManager (20 Hz tick)                          │
│  initScene → update → render → destroyScene on switch   │
│  PetSave::save() on every scene transition              │
└──────────────────────────┬──────────────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
  Scene_PetTotoro    Scene_AcornCatch   Scene_TicTacToe  …
        │                  │                  │
        └──────────────────┴──────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  GameScene (base class)                                 │
│  Avatars (≤50) + Attachments + Background               │
│  renderScene() — dirty-rect compositor                  │
│  renderFullScreen() — full 240×320 repaint (rare)       │
└──────────────────────────┬──────────────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
   TFT_eSPI           Physics.h         SoundPlayer
   (SPI display)      (collision)       (FreeRTOS task)
```

### Key technical decisions

| Decision | Why |
|----------|-----|
| **Dirty-rectangle rendering** | Full 240×320 framebuffer won't fit in RAM; repaint only changed sprite spans |
| **PROGMEM + 1-bit masks** | RGB565 bitmaps in flash; masks handle transparency (white ≠ transparent) |
| **Tiled backgrounds** | 50×50 grass tile (~5 KB) vs full screen (~150 KB) — saves flash |
| **Sprite sheets** | One bitmap, many sub-regions (Totoro stages, soot variants, digits) |
| **Dual touch targets** | Same code on hardware (XPT2046 + cal) and Wokwi (FT6206) via compile flag |
| **Deferred globals** | TFT_eSPI constructed in `setup()`, not static scope — avoids ESP32 boot crash |
| **Vendored TFT_eSPI** | Patched TOUCH_CS fix; config via `platformio.ini` build flags |

### Asset pipeline

Source PNG art in `assets/` → Python tools in `tools/` → PROGMEM headers in `src/`.

```bash
python3 tools/png_to_spritesheet.py assets/foo.png -o src/sprite_foo.h -n sprite_foo
pio run
```

See `AGENTS.md` for generator scripts and rendering rules.

---

## Scene map

| Index | Scene | Role |
|-------|-------|------|
| 0 | Pet Totoro | **Hub** — virtual pet home + radial menu |
| 1 | Acorn Catch | Mini-game |
| 2 | Settings | Calibration + factory reset |
| 3 | Tic-Tac-Toe | Mini-game |
| 4 | Whack-a-Mole | Mini-game |
| 5 | Status | Read-only pet stats |
| 6 | Grocery | Food shop |

> The old **map-style location hub** has been removed. Pet Totoro is scene 0 and
> the sole navigation center.

---

## Build & flash (physical board)

```bash
cd GameBase
pio run -t upload          # default env: esp32-hw
pio device monitor       # 115200 baud; type 'c' to recalibrate touch
```

| Environment | Target | Touch |
|-------------|--------|-------|
| `esp32-hw` (default) | Physical ESP32 + ILI9341 + XPT2046 | Resistive, calibrated |
| `esp32-wroom` | Wokwi simulator | Capacitive, no calibration |

**Never flash `esp32-wroom` to physical hardware.**

---

## Screenshots & media

Suggested assets for the Notion page (already in repo):

| File | Use |
|------|-----|
| `GameBase/docs/images/wiring_display.png` | Hardware wiring diagram |
| `GameBase/docs/images/wiring_buttons.png` | Button wiring |
| `GameBase/assets/Screenshot_*.png` | In-game screenshots |
| `GameBase/assets/grass_tile_preview.png` | Tiled background example |
| `GameBase/assets/sprite_ttt_tokens_preview.png` | Tic-Tac-Toe tokens |

Embed a Wokwi simulation link or screen recording of the pet hub + mini-games
for the strongest demo impact.

---

## What was wrong / outdated (fix checklist)

Use this checklist when updating the Notion page:

- [ ] **Hub description** — It is **Pet Totoro**, not a map-style location menu
- [ ] **Navigation** — Radial menu on tap (Play / Eat / Pet / Bath / Info / Set)
- [ ] **Missing scenes** — Add **Grocery** and **Status**
- [ ] **Virtual pet** — Document stats, growth stages, soot cleaning, coin economy
- [ ] **Acorn Catch modes** — Time Attack, Survival, Collector (not just "catch acorns")
- [ ] **Whack-a-Mole** — 5 difficulty levels, 30 s rounds
- [ ] **Tic-Tac-Toe** — Mei/Cat Bus tokens, AI fumble, 1P/2P modes
- [ ] **Persistence** — NVS saves pet + calibration; factory reset in Settings
- [ ] **Platform** — ESP32 (repo name `ESP8266_ili9341` is legacy)
- [ ] **MISO warning** — LCD SDO must stay disconnected from GPIO19
- [ ] **Simulator** — Wokwi works with `esp32-wroom` build, no hardware needed

---

## Tech stack summary (for resume / portfolio)

- **Language:** C++ (Arduino framework)
- **Platform:** ESP32, PlatformIO
- **Display:** TFT_eSPI (vendored, patched)
- **Storage:** ESP32 NVS (Preferences API)
- **Concurrency:** FreeRTOS audio task
- **Tools:** Python asset pipeline (PNG → PROGMEM)
- **Simulation:** Wokwi (ESP32 + ILI9341 cap-touch board)
