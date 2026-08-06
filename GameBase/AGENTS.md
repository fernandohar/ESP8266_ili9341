# Contributor / Agent Guide

Internal notes for humans and AI agents working on this project. For hardware,
flashing, wiring, and calibration, see [`README.md`](README.md).

---

## 1. Asset pipeline — generating sprites & backgrounds

All on-screen graphics are compiled into the firmware as **PROGMEM C headers**
(`src/*.h`). Source art lives in `assets/`; the generated headers live in
`src/`. The conversion tools are in `tools/`.

### Unified sprite converter: `tools/sprite_converter.py` (recommended for new sprites)

Converts PNG sprite(s) into a single header with **4-, 8-, or 16-bit** pixel
data, a shared **RGB565 palette** (4/8-bit), one or more **bitmaps/regions** in
the same file, and the usual **1-bit opacity mask**.

```bash
python3 tools/sprite_converter.py assets/foo.png \
  -o src/sprite_foo.h -n sprite_foo \
  --bpp 8 \
  --transparent 255,0,255

# Multiple frames sharing one palette:
python3 tools/sprite_converter.py assets/foo_idle.png assets/foo_walk.png \
  -o src/sprite_foo.h -n sprite_foo \
  --bpp 4 --layout horizontal --regions idle,walk
```

Browser UI with live preview: open
[`tools/sprite_converter/index.html`](tools/sprite_converter/index.html).

Generated **unified** headers define:

- `static const SpriteAsset sprite_foo PROGMEM` — bit depth + pointers
- `sprite_fooPalette[]` — RGB565 LUT (4/8-bit only)
- `sprite_fooPixels[]` — packed indices or RGB565 values
- `sprite_fooMask[]` — 1-bit-per-pixel mask
- `sprite_fooBitmaps[]` — sub-rectangles (`SpriteBitmapRegion`) for frames

Use in scenes:

```cpp
#include "sprite_foo.h"
SpriteSheet sheet(&sprite_foo);
Avatar *avatar = sheet.createAvatar(
    x, y, SpriteSheet::readBitmapRegion(sprite_fooBitmaps, 0));
```

Pass `--bpp 16 --legacy` to emit the older flat RGB565 header format instead.
**Existing legacy headers keep working** — the renderer still supports direct
`uint16_t` bitmap + mask without migration.

Runtime support lives in [`src/SpriteAsset.h`](src/SpriteAsset.h),
[`src/Avatar.h`](src/Avatar.h), and [`src/GameScene.cpp`](src/GameScene.cpp).

### Legacy converter: `tools/png_to_spritesheet.py`

This is the workhorse. It turns a PNG into a header with:

- `const unsigned short <name>[] PROGMEM` — RGB565 bitmap
  (`((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3)`).
- `const uint8_t <name>Mask[] PROGMEM` — 1-bit-per-pixel opacity mask, each row
  padded up to a whole number of bytes. **This mask is the real source of
  transparency** (needed because opaque pure white and the transparent sentinel
  both encode as `0xFFFF`).
- `#define <NAME>_WIDTH` / `#define <NAME>_HEIGHT`.
- Optional `static const SpriteSheetRegion <name>Regions[] PROGMEM` when
  `--regions` is passed (per-frame/glyph rectangles for a sprite sheet).

```bash
python3 tools/png_to_spritesheet.py assets/foo.png \
  -o src/sprite_foo.h -n sprite_foo \
  --transparent 255,0,255            # optional: key out a bg color \
  --regions "idle,0,0,32,32;run,32,0,32,32"   # optional: named sub-rects \
  --fill-digits                      # optional: flood-fill hollow interiors
```

**Indexed color** (same shared encoder as `sprite_converter.py`):

```bash
python3 tools/png_to_spritesheet.py assets/foo.png \
  -o src/sprite_foo.h -n sprite_foo \
  --indexed auto          # off | auto | 4 | 8 | 16 \
  --quantize              # merge colors when palette is too large \
  --max-colors 16         # optional cap when quantizing
```

Key flags:
- `--transparent R,G,B` — treat a near-match color as transparent (tolerance ±12).
- `--regions "label,x,y,w,h;..."` — emit region metadata for sheets.
- `--fill-digits` — flood-fill hollow glyph interiors solid white (used for the
  number/letter sheets).
- `--indexed auto` — pick 4-bit (≤16 colors), 8-bit (≤256), or fall back to 16-bit.
- `--quantize` — reduce a large palette to fit the chosen bit depth (needed for
  photo-like backgrounds).

### Re-indexing existing headers (no PNG required)

When source PNGs are unavailable, convert legacy RGB565 headers in place:

```bash
python3 tools/reindex_header.py src/sprite_totoro_baby.h --indexed auto
python3 tools/reindex_header.py src/image_acorn_catch_bg.h --indexed 8 --quantize
```

Estimate savings across all legacy headers:

```bash
python3 tools/compare_indexed_savings.py
```

Shared encoding logic lives in [`tools/sprite_encoding.py`](tools/sprite_encoding.py).

### Tamagotchi-style art guidelines

Solid pixel art with thick outlines and flat fills compress best:

- **≤16 unique opaque colors** → `--indexed 4` or `--indexed auto` (4-bit)
- **17–256 colors** → `--indexed 8` or `--indexed auto`
- **Gradients / photos** → `--indexed 8 --quantize` or keep `--indexed off` (RGB565)
- Always keep the **1-bit mask**; do not rely on palette index 0 for transparency
  (opaque white is still `0xFFFF` in legacy art).
- Pre-process PNGs in Gimp: *Image → Mode → Indexed* with a small fixed palette
  before converting, for maximum flash savings.

### Indexed backgrounds

Full-screen backgrounds can use `SpriteAsset` headers and:

```cpp
setBackgroundAsset(&acorn_catch_bg);
drawBackgroundAsset(&acorn_catch_bg);   // one-time full repaint
```

Tiled backgrounds can use `setBackgroundTileAsset(&grass_tile)` when converted.

Already migrated in-tree (8-bit indexed): `sprite_totoro_baby.h`, `sprite_acorn.h`,
`image_acorn_catch_bg.h`. Flash usage dropped ~85 KB vs the prior RGB565 versions.

### Purpose-built generators (wrap the core converter)

These import `image_to_sheet` / `write_header` from `png_to_spritesheet.py` and
add pre-processing. Use them as templates when a new asset needs special
handling:

| Script | Input → Output | What it adds |
|--------|----------------|--------------|
| `generate_soot_sheet.py` | `assets/soot_source.png` → `src/sprite_soot.h` | Finds sprite blobs and packs them into uniform 16px cells. |
| `generate_soot_mole_sheet.py` | `assets/soot_mole_source.png` → `src/sprite_soot_mole.h` | Like `generate_soot_sheet.py` but the source has no real alpha and its blobs touch/overlap, so it labels connected regions, filters for plausible single-creature bbox size/aspect, and isolates each blob's own pixels (keeping enclosed eye-whites) before packing into 44px cells. Used for the Whack-a-Mole "mole". |
| `generate_grass_tile.py` | `assets/tictactoe_bg_elements_source.png` → `src/image_grass_tile.h` | Crops a clean 50x50 grass patch and makes it seamless (offset+feather) for use as a *repeating* background tile - far smaller than a full-screen image. See "tiled backgrounds" below. |
| `generate_ttt_grid.py` | `assets/tictactoe_bg_elements_source.png` → `src/sprite_ttt_grid.h` | Crops the wooden "#" grid and keys out its black background so it overlays as a transparent 210x210 Avatar over the tiled grass. |
| `generate_ttt_tokens.py` | `assets/ttt_mei_source.png` + `assets/ttt_catbus_source.png` → `src/sprite_ttt_tokens.h` | Crops a square around each character's face and applies a circular alpha mask + coloured ring, giving clean round game tokens (Mei = O, Cat Bus = X) that read over the grass board. Edit the per-token crop boxes/ring colours in `TOKENS`. |
| `generate_digit_sheet.py` / `generate_letter_sheet.py` / `pack_digit_sheet.py` | procedural / source PNG → glyph headers | Lays out digit/letter glyphs into a sheet. |

### Typical workflow to add or replace an image

1. Put the source PNG in `assets/` (hand-drawn, AI-generated, or a photo/render).
   For **backgrounds**, the display is **240 (W) x 320 (H)**, portrait.
2. Convert it:
   - Simple sprite/background → run `png_to_spritesheet.py` directly.
   - Needs cropping / palette reduction / blob packing → copy an existing
     generator in `tools/` and adjust its constants.
3. The tool writes a header into `src/`.
4. `#include` the header in the relevant scene and draw it (see the background /
   avatar helpers in `GameScene`).
5. Rebuild: `pio run` (default `esp32-hw`). PROGMEM assets increase flash usage —
   watch the "Flash: [....]" line in the build output.

> Prefer regenerating an asset from a source PNG over hand-editing the generated
> header. Never hand-edit the large PROGMEM arrays.

### Tiled (repeating) backgrounds — save flash

A full-screen 240x320 background costs ~150 KB of PROGMEM. When the background
is a repeating texture (e.g. grass), use a small tile instead:

- Generate a small seamless tile (see `generate_grass_tile.py`, 50x50 ≈ 5 KB).
- In the scene call `setBackgroundTile(tile, TILE_W, TILE_H)` instead of
  `setBackground(...)`. Both `renderScene()` and `renderFullScreen()` then wrap
  the tile with modulo indexing across the whole screen (handled in
  `drawBg2Buffer`), so nothing else in the scene changes.
- Overlay any non-repeating elements (e.g. a board grid) as transparent
  Avatars on top (see `Scene_TicTacToe` + `generate_ttt_grid.py`).

---

## 2. Rendering: `renderScene()` vs `renderFullScreen()`

Both live in `src/GameScene.cpp`. Choosing the right one matters for performance
and flicker.

### `renderScene()` — use this for normal frame updates ✅

- **Incremental / dirty-rectangle renderer.** It figures out which avatars moved
  or animated, unions their bounding boxes, and repaints **only the affected
  spans** of the screen.
- Cheap and **flicker-free** — most of the screen is left untouched between
  frames.
- This is the default for animated gameplay (moving/animated avatars over a
  static background).
- Variant `renderScene(bool refreshBackground)` lets you force the background
  under the dirty region to be re-read.

### `renderFullScreen()` — expensive, use sparingly ⚠️

- **Repaints the entire 240x320 screen every call**: for every row it rebuilds a
  line buffer from the background bitmap and composites all avatars, then pushes
  all rows.
- **Costly and can cause visible flicker** because the whole frame is repainted.
- Appropriate only for:
  - a one-time full repaint (e.g. right after a scene loads / the whole
    background changes), or
  - simple static screens that redraw rarely.
- Requires a valid background set via `setBackground(...)`; it reads the bitmap
  with `pgm_read_word_far`, so a `NULL` background reads garbage. For fully
  static screens with no background image (e.g. a menu drawn with primitives),
  **draw once in `initScene()` and make `render()` a no-op** instead of calling
  `renderFullScreen()` every frame (see `src/Scene_Settings.h`).

### Rule of thumb

> Animate with `renderScene()`. Reach for `renderFullScreen()` only for a
> deliberate full repaint, and never call it every frame for animation — it is
> expensive and flickers.

Use `requestRender()` to ask the scene manager for a render on the next tick
rather than forcing a full-screen repaint yourself.
