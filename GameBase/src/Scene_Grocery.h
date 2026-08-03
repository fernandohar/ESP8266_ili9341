#ifndef _SCENE_GROCERY_H_
#define _SCENE_GROCERY_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "SpriteSheet.h"
#include "Input.h"
#include "TouchInput.h"
#include "GameProgress.h"
#include "PetTotoroState.h"
#include "PetSave.h"
#include "PendingMeal.h"
#include "image_grass_tile.h"
#include "image_wood_branch.h"
#include "sprite_grocery_food.h"

// Grocery store reached from the pet's radial menu ("Eat"). To keep the flash
// cost tiny the backdrop is the shared grass tile (like the mini-games) with two
// wood-branch "shelves" the food rests on, giving a forest-market feel.
//
// Each food has three frames in sprite_grocery_food (new / half / eaten); the
// store shows the "new" frame, and the eating screen (later) will step through
// all three. Food frames are stored small and blitted at GROCERY_STORE_SCALE so
// the sheet stays tiny. LEFT/RIGHT move the selection (auto-paging), HOME opens
// the selected item's detail/purchase card, and an on-screen Back arrow (touch)
// returns to the pet's home. Buying applies the food's hunger/happiness effect
// immediately (on-avatar eating is a later phase).
//
// The screen is mostly static: it is repainted on selection/state changes only,
// so render() is a no-op.

#define GROCERY_FOOD_COUNT 12
#define GROCERY_FRAMES_PER_FOOD 3   // new, half, eaten
#define GROCERY_FRAME_W 24          // one frame cell in the sprite sheet
#define GROCERY_FRAME_H 24
#define GROCERY_STORE_SCALE 2       // frames are blitted at this integer scale
#define GROCERY_SLOTS_PER_PAGE 8    // 4 columns x 2 rows
#define GROCERY_BRANCH_ROWS 2

// Vertical span covering both shelf rows + the food resting on them. Used to
// rebuild only this band on a page change instead of the whole screen.
#define GROCERY_BAND_Y 78
#define GROCERY_BAND_H 170

// Frame indices within each food's 3-frame group.
#define GROCERY_FRAME_NEW 0
#define GROCERY_FRAME_HALF 1
#define GROCERY_FRAME_EATEN 2

struct GroceryFood {
  const char *name;
  int16_t cost;       // coins (0 == free)
  int8_t hunger;      // stat delta applied on eat (0..100 scale)
  int8_t happiness;   // stat delta applied on eat (may be negative)
};

class Scene_Grocery : public GameScene {
  public:
    Scene_Grocery(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();

      if (state == GROCERY_STORE) {
        updateStore(isTouching, input, needChangeScene, nextSceneIndex);
      } else {
        updateDetail(isTouching, input, needChangeScene, nextSceneIndex);
      }

      wasTouching = isTouching;
    }

    void render() {
      // Static screen: repainted on demand from update(), nothing to animate.
    }

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 250;
      state = GROCERY_STORE;
      selIndex = 0;

      setBackgroundTile(grass_tile, GRASS_TILE_WIDTH, GRASS_TILE_HEIGHT);
      drawStore();
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    enum GroceryState { GROCERY_STORE, GROCERY_DETAIL };

    // Column centres and shelf-top baselines (food bottoms rest on these).
    static int16_t colCenter(int col) {
      static const int16_t CX[4] = {30, 90, 150, 210};
      return CX[col];
    }
    // Top of each wood-branch ledge.
    static int16_t branchTop(int row) {
      static const int16_t BT[2] = {128, 214};
      return BT[row];
    }

    static int pageOf(int index) { return index / GROCERY_SLOTS_PER_PAGE; }
    static int pageCount() {
      return (GROCERY_FOOD_COUNT + GROCERY_SLOTS_PER_PAGE - 1) / GROCERY_SLOTS_PER_PAGE;
    }

    const GroceryFood &food(int index) {
      // All free for now (cost 0) so the loop is easy to test. Hunger/happiness
      // deltas are tunable: fillers (bun, onigiri, yam, ramen) top up hunger;
      // treats (cotton candy, soft serve, dorayaki) lift happiness; veg
      // (broccoli, green onion) fill a little but dent happiness.
      static const GroceryFood FOODS[GROCERY_FOOD_COUNT] = {
        {"Broccoli",    0, 15, -2},
        {"Cotton candy",0,  3, 22},
        {"Salad",       0, 14,  3},
        {"Bun",         0, 25,  6},
        {"Onigiri",     0, 22,  4},
        {"Soft serve",  0,  6, 20},
        {"Sushi",       0, 20, 15},
        {"Hamburger",   0, 35, 10},
        {"Dorayaki",    0, 12, 18},
        {"Yam",         0, 24,  5},
        {"Green onion", 0, 10, -3},
        {"Ramen",       0, 34, 14},
      };
      return FOODS[index];
    }

    // Region index in sprite_grocery_food for a given food + frame.
    static int regionIndex(int foodIndex, int frame) {
      return foodIndex * GROCERY_FRAMES_PER_FOOD + frame;
    }

    // ---- layout ------------------------------------------------------------

    static const int16_t GROCERY_CELL_W = GROCERY_FRAME_W * GROCERY_STORE_SCALE;
    static const int16_t GROCERY_CELL_H = GROCERY_FRAME_H * GROCERY_STORE_SCALE;

    // Display rect for a slot's food (frame bottom rests on the branch top).
    void slotRect(int slot, int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
      int col = slot % 4;
      int row = slot / 4;
      *w = GROCERY_CELL_W;
      *h = GROCERY_CELL_H;
      *x = colCenter(col) - GROCERY_CELL_W / 2;
      *y = (branchTop(row) + 4) - GROCERY_CELL_H;  // bottom just onto the branch
    }

    void moveSelection(int delta) {
      int prev = selIndex;
      selIndex += delta;
      if (selIndex < 0) selIndex = GROCERY_FOOD_COUNT - 1;
      if (selIndex >= GROCERY_FOOD_COUNT) selIndex = 0;
      addSound(NOTE_E5, noteDurationMs(24, 900));

      if (pageOf(selIndex) != pageOf(prev)) {
        // Different page: rebuild just the shelf band (not the whole screen).
        reconstructRegion(0, GROCERY_BAND_Y, SCREENWIDTH, GROCERY_BAND_H);
        drawSelection();
        drawTopBar();
        return;
      }

      // Same page: erase the old highlight cell, draw the new one, refresh the
      // name / cost bar. No full-screen repaint, so no flicker.
      eraseCell(prev % GROCERY_SLOTS_PER_PAGE);
      drawSelection();
      drawTopBar();
    }

    // ---- STORE state -------------------------------------------------------

    void updateStore(boolean isTouching, const GameInput &input,
                     boolean *needChangeScene, int *nextSceneIndex) {
      if (input.leftPressed) {
        moveSelection(-1);
      } else if (input.rightPressed) {
        moveSelection(+1);
      }

      if (input.homePressed) {
        enterDetail();
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t tx = 0, ty = 0;
        if (getTouchPoint(_tft, &tx, &ty)) {
          if (inRect(tx, ty, BACK_X, BACK_Y, BACK_W, BACK_H)) {
            addSound(NOTE_G5, noteDurationMs(8, 800));
            *needChangeScene = true;
            *nextSceneIndex = SCENE_PET_TOTORO;
            return;
          }
          int tapped = slotAtPoint(tx, ty);
          if (tapped >= 0) {
            selIndex = pageOf(selIndex) * GROCERY_SLOTS_PER_PAGE + tapped;
            enterDetail();
          }
        }
      }
    }

    // Which visible slot (0..7) contains the touch point, or -1.
    int slotAtPoint(uint16_t tx, uint16_t ty) {
      for (int slot = 0; slot < GROCERY_SLOTS_PER_PAGE; slot++) {
        int foodIndex = pageOf(selIndex) * GROCERY_SLOTS_PER_PAGE + slot;
        if (foodIndex >= GROCERY_FOOD_COUNT) continue;
        int16_t x = 0, y = 0, w = 0, h = 0;
        slotRect(slot, &x, &y, &w, &h);
        if (inRect(tx, ty, x - 4, y - 4, w + 8, h + 8)) {
          return slot;
        }
      }
      return -1;
    }

    // Full paint of the store. Only used on scene entry and when returning from
    // the detail card (both are full transitions, so a one-time full repaint is
    // fine); selection changes use the incremental path in moveSelection().
    void drawStore() {
      renderFullScreen();  // grass tile (no avatars)
      drawBranches();      // wood-branch shelves
      drawFoods();         // "new" frame of each food on this page, resting on the shelves
      drawTopBar();
      drawSelection();
      drawBackButton();
      drawPager();
    }

    // Draw the two wood-branch shelves directly (mask-aware blit) so they never
    // depend on avatar compositing.
    void drawBranches() {
      for (int row = 0; row < GROCERY_BRANCH_ROWS; row++) {
        drawImageMasked(image_wood_branch, image_wood_branchMask,
                        IMAGE_WOOD_BRANCH_WIDTH, IMAGE_WOOD_BRANCH_HEIGHT,
                        0, branchTop(row));
      }
    }

    // Rebuild an arbitrary screen rectangle from scratch (grass tile, then the
    // wood shelves, then the "new" frame of every food on the current page)
    // straight into row buffers. This is the incremental alternative to a full
    // renderFullScreen(): moving the selection only rebuilds the small cell that
    // changed, so there is no whole-screen repaint / flicker.
    void reconstructRegion(int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
      if (rx < 0) { rw += rx; rx = 0; }
      if (ry < 0) { rh += ry; ry = 0; }
      if (rx + rw > SCREENWIDTH) rw = SCREENWIDTH - rx;
      if (ry + rh > SCREENHEIGHT) rh = SCREENHEIGHT - ry;
      if (rw <= 0 || rh <= 0) return;

      const uint16_t bStride = (IMAGE_WOOD_BRANCH_WIDTH + 7) / 8;
      const uint16_t fStride = (SPRITE_GROCERY_FOOD_WIDTH + 7) / 8;
      const int page = pageOf(selIndex);
      static uint16_t line[SCREENWIDTH];

      for (int16_t y = ry; y < ry + rh; y++) {
        uint16_t gy = y % GRASS_TILE_HEIGHT;
        for (int16_t i = 0; i < rw; i++) {
          int16_t x = rx + i;
          uint16_t gx = x % GRASS_TILE_WIDTH;
          uint16_t c = pgm_read_word(&grass_tile[gy * GRASS_TILE_WIDTH + gx]);

          // Shelves.
          for (int row = 0; row < GROCERY_BRANCH_ROWS; row++) {
            int16_t bt = branchTop(row);
            if (y >= bt && y < bt + IMAGE_WOOD_BRANCH_HEIGHT) {
              uint16_t by = y - bt;
              uint8_t mb = pgm_read_byte(&image_wood_branchMask[by * bStride + (x >> 3)]);
              if ((mb >> (7 - (x & 7))) & 0x1) {
                c = pgm_read_word(&image_wood_branch[by * IMAGE_WOOD_BRANCH_WIDTH + x]);
              }
            }
          }

          // Food (topmost).
          for (int slot = 0; slot < GROCERY_SLOTS_PER_PAGE; slot++) {
            int foodIndex = page * GROCERY_SLOTS_PER_PAGE + slot;
            if (foodIndex >= GROCERY_FOOD_COUNT) continue;
            int16_t cx = 0, cy = 0, cw = 0, ch = 0;
            slotRect(slot, &cx, &cy, &cw, &ch);
            if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
              SpriteSheetRegion r = SpriteSheet::readRegion(
                  sprite_grocery_foodRegions, regionIndex(foodIndex, GROCERY_FRAME_NEW));
              uint16_t fgx = r.x + (uint16_t)((x - cx) / GROCERY_STORE_SCALE);
              uint16_t fgy = r.y + (uint16_t)((y - cy) / GROCERY_STORE_SCALE);
              uint8_t mb = pgm_read_byte(&sprite_grocery_foodMask[fgy * fStride + (fgx >> 3)]);
              if ((mb >> (7 - (fgx & 7))) & 0x1) {
                c = pgm_read_word(&sprite_grocery_food[fgy * SPRITE_GROCERY_FOOD_WIDTH + fgx]);
              }
              break;
            }
          }
          line[i] = c;
        }
        _tft->pushImage(rx, y, rw, 1, line);
      }
    }

    // Erase one slot's selection frame by rebuilding its cell (plus the little
    // margin the highlight border occupies).
    void eraseCell(int slot) {
      int16_t x = 0, y = 0, w = 0, h = 0;
      slotRect(slot, &x, &y, &w, &h);
      reconstructRegion(x - 6, y - 6, w + 12, h + 12);
    }

    // Blit the "new" frame of every food on the current page.
    void drawFoods() {
      int page = pageOf(selIndex);
      for (int slot = 0; slot < GROCERY_SLOTS_PER_PAGE; slot++) {
        int foodIndex = page * GROCERY_SLOTS_PER_PAGE + slot;
        if (foodIndex >= GROCERY_FOOD_COUNT) continue;
        int16_t x = 0, y = 0, w = 0, h = 0;
        slotRect(slot, &x, &y, &w, &h);
        drawFrame(regionIndex(foodIndex, GROCERY_FRAME_NEW), x, y, GROCERY_STORE_SCALE);
      }
    }

    void drawTopBar() {
      // Coins (left) and selected price (right) pills.
      char buf[20];
      snprintf(buf, sizeof(buf), "Coins %d", GameProgress::getCoins());
      pill(6, 6, 112, 26, rgb565(70, 120, 120), rgb565(200, 240, 240), buf);

      costText(food(selIndex).cost, buf, sizeof(buf));
      pill(SCREENWIDTH - 6 - 112, 6, 112, 26, rgb565(120, 100, 70),
           rgb565(245, 225, 190), buf);

      // Selected item name banner.
      uint16_t nameBg = rgb565(60, 110, 110);
      _tft->fillRoundRect(6, 36, SCREENWIDTH - 12, 30, 8, nameBg);
      _tft->drawRoundRect(6, 36, SCREENWIDTH - 12, 30, 8, rgb565(210, 245, 245));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, nameBg);
      _tft->drawString(food(selIndex).name, SCREENWIDTH / 2, 51, 4);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawSelection() {
      int slot = selIndex % GROCERY_SLOTS_PER_PAGE;
      int16_t x = 0, y = 0, w = 0, h = 0;
      slotRect(slot, &x, &y, &w, &h);
      int16_t fx = x - 4, fy = y - 4, fw = w + 8, fh = h + 8;
      _tft->drawRoundRect(fx, fy, fw, fh, 8, rgb565(255, 210, 70));
      _tft->drawRoundRect(fx + 1, fy + 1, fw - 2, fh - 2, 7, rgb565(255, 240, 150));
    }

    void drawBackButton() {
      uint16_t c = rgb565(74, 42, 42);
      _tft->fillRoundRect(BACK_X, BACK_Y, BACK_W, BACK_H, 8, c);
      _tft->drawRoundRect(BACK_X, BACK_Y, BACK_W, BACK_H, 8, rgb565(210, 150, 150));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, c);
      _tft->drawString("< Back", BACK_X + BACK_W / 2, BACK_Y + BACK_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawPager() {
      if (pageCount() <= 1) return;
      char buf[8];
      snprintf(buf, sizeof(buf), "%d / %d", pageOf(selIndex) + 1, pageCount());
      _tft->setTextDatum(MR_DATUM);
      _tft->setTextColor(rgb565(60, 90, 90), rgb565(180, 220, 220));
      _tft->drawString(buf, SCREENWIDTH - 12, BACK_Y + BACK_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // ---- DETAIL / purchase state ------------------------------------------

    void enterDetail() {
      state = GROCERY_DETAIL;
      suppressTouchUntilMs = millis() + 250;
      addSound(NOTE_C5, noteDurationMs(16, 900));
      drawDetail();
    }

    void exitDetailToStore() {
      state = GROCERY_STORE;
      suppressTouchUntilMs = millis() + 200;
      drawStore();
    }

    void updateDetail(boolean isTouching, const GameInput &input,
                      boolean *needChangeScene, int *nextSceneIndex) {
      if (input.homePressed) {
        exitDetailToStore();
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t tx = 0, ty = 0;
        if (getTouchPoint(_tft, &tx, &ty)) {
          bool affordable = GameProgress::getCoins() >= food(selIndex).cost;
          if (affordable && inRect(tx, ty, OK_X, OK_Y, BTN_W, BTN_H)) {
            buyAndReturn(needChangeScene, nextSceneIndex);
          } else if (inRect(tx, ty, CANCEL_X, OK_Y, BTN_W, BTN_H)) {
            addSound(NOTE_A3, noteDurationMs(16, 700));
            exitDetailToStore();
          }
        }
      }
    }

    void buyAndReturn(boolean *needChangeScene, int *nextSceneIndex) {
      const GroceryFood &f = food(selIndex);
      GameProgress::setCoins(GameProgress::getCoins() - f.cost);
      // Hand the food to the pet scene. Its hunger/happiness effect is applied
      // there once Totoro finishes eating it (see the eating animation); the
      // grocery only passes the three frames of this food.
      PendingMeal::set(f.name, f.hunger, f.happiness,
                       regionIndex(selIndex, GROCERY_FRAME_NEW),
                       regionIndex(selIndex, GROCERY_FRAME_HALF),
                       regionIndex(selIndex, GROCERY_FRAME_EATEN));
      addSound(NOTE_C5, noteDurationMs(16, 700));
      addSound(NOTE_E5, noteDurationMs(16, 700));
      addSound(NOTE_G5, noteDurationMs(8, 700));
      *needChangeScene = true;
      *nextSceneIndex = SCENE_PET_TOTORO;
    }

    void drawDetail() {
      const GroceryFood &f = food(selIndex);
      uint16_t back = rgb565(206, 234, 234);
      _tft->fillScreen(back);

      char buf[20];
      snprintf(buf, sizeof(buf), "Coins %d", GameProgress::getCoins());
      pill(6, 6, 112, 26, rgb565(70, 120, 120), rgb565(200, 240, 240), buf);
      costText(f.cost, buf, sizeof(buf));
      pill(SCREENWIDTH - 6 - 112, 6, 112, 26, rgb565(120, 100, 70),
           rgb565(245, 225, 190), buf);

      uint16_t nameBg = rgb565(60, 110, 110);
      _tft->fillRoundRect(6, 38, SCREENWIDTH - 12, 30, 8, nameBg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, nameBg);
      _tft->drawString(f.name, SCREENWIDTH / 2, 53, 4);

      // Big food sprite (the "new" frame).
      int scale = 4;
      int spriteW = GROCERY_FRAME_W * scale;
      drawFrame(regionIndex(selIndex, GROCERY_FRAME_NEW),
                (SCREENWIDTH - spriteW) / 2, 96, scale);

      // Stat effects.
      int16_t sy = 210;
      _tft->setTextDatum(MC_DATUM);
      snprintf(buf, sizeof(buf), "Hunger %+d", f.hunger);
      _tft->setTextColor(rgb565(40, 120, 60), back);
      _tft->drawString(buf, SCREENWIDTH / 2, sy, 4);
      snprintf(buf, sizeof(buf), "Happy %+d", f.happiness);
      _tft->setTextColor(f.happiness >= 0 ? rgb565(200, 120, 40) : rgb565(190, 70, 70), back);
      _tft->drawString(buf, SCREENWIDTH / 2, sy + 30, 4);

      bool affordable = GameProgress::getCoins() >= f.cost;
      // OK button (disabled if unaffordable).
      uint16_t okC = affordable ? rgb565(60, 140, 80) : rgb565(120, 130, 125);
      _tft->fillRoundRect(OK_X, OK_Y, BTN_W, BTN_H, 8, okC);
      _tft->drawRoundRect(OK_X, OK_Y, BTN_W, BTN_H, 8, rgb565(220, 245, 225));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, okC);
      _tft->drawString(affordable ? "OK" : "No coins", OK_X + BTN_W / 2, OK_Y + BTN_H / 2, 2);

      uint16_t cxC = rgb565(120, 80, 80);
      _tft->fillRoundRect(CANCEL_X, OK_Y, BTN_W, BTN_H, 8, cxC);
      _tft->drawRoundRect(CANCEL_X, OK_Y, BTN_W, BTN_H, 8, rgb565(230, 200, 200));
      _tft->setTextColor(TFT_WHITE, cxC);
      _tft->drawString("Cancel", CANCEL_X + BTN_W / 2, OK_Y + BTN_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // Blit a whole PROGMEM bitmap (with its 1-bpp opacity mask) at 1x.
    void drawImageMasked(const uint16_t *bitmap, const uint8_t *mask,
                         uint16_t w, uint16_t h, int16_t dstX, int16_t dstY) {
      const uint16_t stride = (w + 7) / 8;
      for (uint16_t sy = 0; sy < h; sy++) {
        for (uint16_t sx = 0; sx < w; sx++) {
          uint8_t mbyte = pgm_read_byte(&mask[sy * stride + (sx >> 3)]);
          if (!((mbyte >> (7 - (sx & 7))) & 0x1)) continue;
          uint16_t color = pgm_read_word(&bitmap[sy * w + sx]);
          _tft->drawPixel(dstX + sx, dstY + sy, color);
        }
      }
    }

    // Blit a food sprite frame (by region index) from PROGMEM at an integer
    // scale, honouring the 1-bpp opacity mask (so interior white pixels stay
    // opaque, unlike a colour key).
    void drawFrame(int region, int16_t dstX, int16_t dstY, int scale) {
      SpriteSheetRegion r = SpriteSheet::readRegion(sprite_grocery_foodRegions, region);
      const uint16_t stride = (SPRITE_GROCERY_FOOD_WIDTH + 7) / 8;
      for (uint16_t sy = 0; sy < r.height; sy++) {
        uint16_t gy = r.y + sy;
        for (uint16_t sx = 0; sx < r.width; sx++) {
          uint16_t gx = r.x + sx;
          uint8_t mbyte = pgm_read_byte(&sprite_grocery_foodMask[gy * stride + (gx >> 3)]);
          if (!((mbyte >> (7 - (gx & 7))) & 0x1)) continue;
          uint16_t color = pgm_read_word(&sprite_grocery_food[gy * SPRITE_GROCERY_FOOD_WIDTH + gx]);
          _tft->fillRect(dstX + sx * scale, dstY + sy * scale, scale, scale, color);
        }
      }
    }

    // ---- helpers -----------------------------------------------------------

    void pill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fill,
              uint16_t border, const char *text) {
      _tft->fillRoundRect(x, y, w, h, h / 2, fill);
      _tft->drawRoundRect(x, y, w, h, h / 2, border);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, fill);
      _tft->drawString(text, x + w / 2, y + h / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    static bool inRect(uint16_t tx, uint16_t ty, int16_t x, int16_t y, int16_t w, int16_t h) {
      return (tx >= x && tx < x + w && ty >= y && ty < y + h);
    }

    static void costText(int cost, char *buf, size_t n) {
      if (cost <= 0) snprintf(buf, n, "Free");
      else snprintf(buf, n, "Cost %d", cost);
    }

    // Layout constants.
    static const int16_t BACK_X = 8;
    static const int16_t BACK_Y = 286;
    static const int16_t BACK_W = 84;
    static const int16_t BACK_H = 28;

    static const int16_t BTN_W = 96;
    static const int16_t BTN_H = 40;
    static const int16_t OK_X = 20;
    static const int16_t CANCEL_X = SCREENWIDTH - 20 - BTN_W;
    static const int16_t OK_Y = 268;

    GroceryState state = GROCERY_STORE;
    int selIndex = 0;
    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;
};

#endif
