# Ghibli-Style 8-Bit Art Guide — Totoro Home Console

Art pack for a **Sumikko Gacha–style** virtual pet toy: a cozy Ghibli-inspired room where a forest spirit lives, furniture can be placed and removed, and the character wanders and naps.

**Screen:** 240 × 320 px (matches your `GameBase` TFT_eSPI project)  
**Format:** RGB565 bitmaps + 1-bit alpha masks (same as existing `pork.h` / `Avatar` sprites)

---

## Product Reference

| Feature | Sumikko Gacha | Your Totoro Home |
|---------|---------------|------------------|
| Form factor | Handheld gacha dial + screen | Your custom ESP + TFT console |
| Room decorate | DIY craft furniture | Tap-to-place furniture on tatami |
| Pet care | Feed, clean, pet | Wander, breathe, sleep on cushion |
| Art style | Pastel San-X kawaii | Warm Ghibli washitsu evening |
| Resolution | ~small LCD | 240×320 |

---

## Art Style Rules (Ghibli → 8-bit)

### 1. Limited palette (16–24 colors per scene)
Use earthy, muted tones. Avoid pure black/white except for eyes and lamp highlights.

| Role | Hex | Use |
|------|-----|-----|
| Wall shadow | `#3A482E` | Corners, ceiling |
| Wall mid | `#5A6944` | Main walls |
| Tatami dark | `#6B5E3A` | Mat weave |
| Tatami light | `#A8964C` | Mat highlight |
| Wood dark | `#2D1E12` | Furniture frame |
| Wood mid | `#48301C` | Tansu, table |
| Shoji glow | `#F5E4B0` | Evening lamp through paper |
| Warm highlight | `#FFDC8C` | Lamp cone |
| Spirit gray | `#8C9196` | Character body |
| Leaf green | `#48783A` | Leaf, plants |

### 2. Lighting (key Ghibli feel)
- **Single warm source** — pendant lamp center-top, cone of light on shoji
- **Cool shadows** in corners (green-grey, not blue-grey)
- **No harsh outlines** on backgrounds; use color steps for depth
- Sprites may use 1px dark outline for readability on busy floors

### 3. Composition (240×320 layout)

```
┌────────────────────────────┐  y=0
│  [catalog icons - 40px]    │  ← furniture picker bar (UI layer)
├────────────────────────────┤  y=40
│   ceiling beam + lamp      │  y=40-70
│   ┌──── shoji glow ────┐   │  y=70-200
│   │   warm paper doors  │   │
│   └─────────────────────┘   │
│ tansu              cabinet  │  y=130-200 (bg silhouettes)
├────────────────────────────┤  y=200
│ ████ tatami floor ████████ │  y=200-320
│    [walk zone 50-190]      │  y=210-290 ← spirit + furniture
└────────────────────────────┘
```

### 4. Two room themes (included)

| File | Mood | Based on your refs |
|------|------|-------------------|
| `washitsu_evening.png` | Cozy lamp + shoji | Tatami room reference |
| `garden_room_day.png` | Open doors + garden | Sunlit porch reference |

---

## Asset Inventory

### Backgrounds (`art/backgrounds/`)
- `washitsu_evening.png` — main room (converted → `BackgroundTotoroHome.h`)
- `garden_room_day.png` — alternate room

### Sprites (`art/sprites/`)
| Sprite | Size | Purpose |
|--------|------|---------|
| `forest_spirit.png` | 40×48 | Main pet (original design; see legal note) |
| `forest_spirit_sleep.png` | 40×44 | Idle nap animation |
| `chabudai.png` | 48×24 | Low tea table |
| `tansu.png` | 32×48 | Chest of drawers |
| `cushion.png` | 24×16 | Floor cushion |
| `plant.png` | 20×28 | Potted plant |
| `acorn.png` | 8×8 | Small collectible |
| `lamp.png` | 16×22 | Placeable lamp |
| `tatami_tile.png` | 40×20 | Repeating floor tile |

---

## Workflow: Create → Convert → Flash

### Step 1 — Edit art
Use **Aseprite**, **LibreSprite**, or **Piskel** at native 240×320 (background) or sprite sizes above.  
Keep palette consistent with `tools/generate_ghibli_assets.py` → `PALETTE` dict.

### Step 2 — Regenerate from script (optional)
```bash
python3 tools/generate_ghibli_assets.py
```

### Step 3 — Convert PNG → Arduino headers
```bash
# Background (no mask)
python3 tools/png_to_header.py art/backgrounds/washitsu_evening.png \
  -o GameBase/BackgroundTotoroHome.h --no-mask

# Sprite with transparency
python3 tools/png_to_header.py art/sprites/forest_spirit.png \
  -o GameBase/forest_spirit.h --mask
```

### Step 4 — Enable scene in firmware
In `GameBase.ino`:
```cpp
#include "Scene_TotoroHome.h"
// ...
Scene_TotoroHome *totoroHome = new Scene_TotoroHome(&tft);
manager.appendScene(totoroHome);
```

---

## Furniture Add/Remove (game design)

`Scene_TotoroHome.h` implements:

1. **Catalog bar** (top 40px) — tap slot 0–3 to select item type
2. **Place** — tap tatami floor (y 210–290) to drop furniture
3. **Remove** — tap placed item to delete it
4. **Max 8** placed items (`TOTORO_HOME_MAX_FURNITURE`)

### Suggested expansions
- **Grid snap** — snap x to 8px grid for tidy rooms
- **Crafting** — collect acorns → unlock new furniture (like Sumikko DIY)
- **Happiness** — spirit sits on cushion when placed nearby
- **Day/night** — swap backgrounds by clock
- **Save layout** — EEPROM store of placed item IDs + positions

---

## Forest Spirit Behavior

| State | Trigger | Visual |
|-------|---------|--------|
| Walk | Every 4s if idle | `forest_spirit.png`, velocity ±1 |
| Breathe | Always | `enableBreathing()` on belly |
| Sleep | 8s no movement | `forest_spirit_sleep.png` |
| Bound | Floor edges | y clamped 210–290, x bounce |

---

## Legal Note — Totoro / Ghibli

The included **forest spirit** is an **original pixel design** inspired by Ghibli's aesthetic. It is **not** an official Totoro asset.

For a commercial product using the name or likeness of **Totoro** or other Ghibli characters, obtain a license from **Studio Ghibli / rights holders**. For prototypes, use generic branding ("Forest Spirit Home").

---

## File Map

```
art/
  backgrounds/     PNG source backgrounds
  sprites/         PNG source sprites
tools/
  generate_ghibli_assets.py   Procedural pixel art generator
  png_to_header.py            PNG → .h converter
GameBase/
  BackgroundTotoroHome.h      240×320 room bitmap
  forest_spirit.h             Pet sprite + mask
  furniture_*.h               Placeable items
  Scene_TotoroHome.h          Demo scene with place/remove
```
