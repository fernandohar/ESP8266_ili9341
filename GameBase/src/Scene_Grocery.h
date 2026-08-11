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

// Grocery store reached from the pet's radial menu ("Eat"). Grass tile backdrop
// with two wood-branch shelves. Select a food with LEFT/RIGHT or tap; HOME buys
// the selection (if affordable) and returns to the pet. Tap Home at the bottom
// to leave without buying. Previous / Next flip pages (8 items per page).
//
// Layout:
//   [Coins][Cost]
//   [Selected item name]
//   [Hunger delta][Happy delta]
//   [food x4]
//   wood shelf
//   [food x4]
//   wood shelf
//   [Previous][Home][Next]

#define GROCERY_FOOD_COUNT 12
#define GROCERY_FRAMES_PER_FOOD 3
#define GROCERY_FRAME_W 24
#define GROCERY_FRAME_H 24
#define GROCERY_STORE_SCALE 2
#define GROCERY_SLOTS_PER_PAGE 8
#define GROCERY_BRANCH_ROWS 2

#define GROCERY_BAND_Y 116
#define GROCERY_BAND_H 162

#define GROCERY_FRAME_NEW 0
#define GROCERY_FRAME_HALF 1
#define GROCERY_FRAME_EATEN 2

struct GroceryFood {
  const char *name;
  int16_t cost;
  int8_t hunger;
  int8_t happiness;
};

class Scene_Grocery : public GameScene {
  public:
    Scene_Grocery(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();

      if (input.leftPressed) {
        moveSelection(-1);
      } else if (input.rightPressed) {
        moveSelection(+1);
      }

      if (input.homePressed) {
        tryBuyAndReturn(needChangeScene, nextSceneIndex);
        wasTouching = isTouching;
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t tx = 0, ty = 0;
        if (getTouchPoint(_tft, &tx, &ty)) {
          if (hasPrevPage() && inRect(tx, ty, PREV_X, NAV_Y, NAV_W, NAV_H)) {
            changePage(-1);
            wasTouching = isTouching;
            return;
          }
          if (inRect(tx, ty, HOME_X, NAV_Y, NAV_W, NAV_H)) {
            addSound(NOTE_G5, noteDurationMs(8, 800));
            *needChangeScene = true;
            *nextSceneIndex = SCENE_PET_TOTORO;
            wasTouching = isTouching;
            return;
          }
          if (hasNextPage() && inRect(tx, ty, NEXT_X, NAV_Y, NAV_W, NAV_H)) {
            changePage(+1);
            wasTouching = isTouching;
            return;
          }
          int tapped = slotAtPoint(tx, ty);
          if (tapped >= 0) {
            int foodIndex = pageOf(selIndex) * GROCERY_SLOTS_PER_PAGE + tapped;
            if (foodIndex >= GROCERY_FOOD_COUNT) {
              wasTouching = isTouching;
              return;
            }
            if (foodIndex == selIndex) {
              tryBuyAndReturn(needChangeScene, nextSceneIndex);
            } else {
              int oldSlot = selIndex % GROCERY_SLOTS_PER_PAGE;
              selIndex = foodIndex;
              addSound(NOTE_E5, noteDurationMs(24, 900));
              eraseCell(oldSlot);
              drawSelection();
              drawTopBar();
            }
          }
        }
      }

      wasTouching = isTouching;
    }

    void render() {
    }

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 250;
      selIndex = 0;
      setBackgroundTile(grass_tile, GRASS_TILE_WIDTH, GRASS_TILE_HEIGHT);
      drawStore();
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    static int16_t colCenter(int col) {
      static const int16_t CX[4] = {30, 90, 150, 210};
      return CX[col];
    }

    static int16_t branchTop(int row) {
      static const int16_t BT[2] = {152, 238};
      return BT[row];
    }

    bool hasPrevPage() const {
      return pageOf(selIndex) > 0;
    }

    bool hasNextPage() const {
      return pageOf(selIndex) < pageCount() - 1;
    }

    static int pageOf(int index) { return index / GROCERY_SLOTS_PER_PAGE; }

    static int pageCount() {
      return (GROCERY_FOOD_COUNT + GROCERY_SLOTS_PER_PAGE - 1) / GROCERY_SLOTS_PER_PAGE;
    }

    const GroceryFood &food(int index) {
      static const GroceryFood FOODS[GROCERY_FOOD_COUNT] = {
        {"Broccoli",     5, 15,  -8},
        {"Green onion", 10, 10, -10},
        {"Salad",       10, 14,   1},
        {"Onigiri",     15, 22,   1},
        {"Yam",         10,  5,   1},
        {"Bun",         18, 25,   2},
        {"Hamburger",   22, 35,   2},
        {"Sushi",       25, 20,   3},
        {"Ramen",       41, 34,   3},
        {"Dorayaki",    21, 12,   4},
        {"Cotton candy",23,  3,   5},
        {"Soft serve",  26,  6,   5},
      };
      return FOODS[index];
    }

    static int regionIndex(int foodIndex, int frame) {
      static const uint8_t SPRITE[GROCERY_FOOD_COUNT] = {
          0, 10, 2, 4, 9, 3, 7, 6, 11, 8, 1, 5};
      return SPRITE[foodIndex] * GROCERY_FRAMES_PER_FOOD + frame;
    }

    static const int16_t GROCERY_CELL_W = GROCERY_FRAME_W * GROCERY_STORE_SCALE;
    static const int16_t GROCERY_CELL_H = GROCERY_FRAME_H * GROCERY_STORE_SCALE;

    static const int16_t NAV_Y = 278;
    static const int16_t NAV_W = 72;
    static const int16_t NAV_H = 32;
    static const int16_t NAV_GAP = 6;
    static const int16_t PREV_X = 12;
    static const int16_t HOME_X = PREV_X + NAV_W + NAV_GAP;
    static const int16_t NEXT_X = HOME_X + NAV_W + NAV_GAP;

    void slotRect(int slot, int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
      int col = slot % 4;
      int row = slot / 4;
      *w = GROCERY_CELL_W;
      *h = GROCERY_CELL_H;
      *x = colCenter(col) - GROCERY_CELL_W / 2;
      *y = (branchTop(row) + 4) - GROCERY_CELL_H;
    }

    void moveSelection(int delta) {
      int prev = selIndex;
      selIndex += delta;
      if (selIndex < 0) {
        selIndex = GROCERY_FOOD_COUNT - 1;
      }
      if (selIndex >= GROCERY_FOOD_COUNT) {
        selIndex = 0;
      }
      addSound(NOTE_E5, noteDurationMs(24, 900));

      if (pageOf(selIndex) != pageOf(prev)) {
        reconstructRegion(0, GROCERY_BAND_Y, SCREENWIDTH, GROCERY_BAND_H);
        drawSelection();
        drawTopBar();
        drawNavBar();
        return;
      }

      eraseCell(prev % GROCERY_SLOTS_PER_PAGE);
      drawSelection();
      drawTopBar();
    }

    void changePage(int delta) {
      int page = pageOf(selIndex);
      int slot = selIndex % GROCERY_SLOTS_PER_PAGE;
      int newPage = page + delta;
      if (newPage < 0 || newPage >= pageCount()) {
        return;
      }
      selIndex = newPage * GROCERY_SLOTS_PER_PAGE + slot;
      if (selIndex >= GROCERY_FOOD_COUNT) {
        selIndex = GROCERY_FOOD_COUNT - 1;
      }
      addSound(NOTE_E5, noteDurationMs(24, 900));
      reconstructRegion(0, GROCERY_BAND_Y, SCREENWIDTH, GROCERY_BAND_H);
      drawSelection();
      drawTopBar();
      drawNavBar();
    }

    int slotAtPoint(uint16_t tx, uint16_t ty) {
      for (int slot = 0; slot < GROCERY_SLOTS_PER_PAGE; slot++) {
        int foodIndex = pageOf(selIndex) * GROCERY_SLOTS_PER_PAGE + slot;
        if (foodIndex >= GROCERY_FOOD_COUNT) {
          continue;
        }
        int16_t x = 0, y = 0, w = 0, h = 0;
        slotRect(slot, &x, &y, &w, &h);
        if (inRect(tx, ty, x - 4, y - 4, w + 8, h + 8)) {
          return slot;
        }
      }
      return -1;
    }

    void tryBuyAndReturn(boolean *needChangeScene, int *nextSceneIndex) {
      const GroceryFood &f = food(selIndex);
      if (GameProgress::getCoins() < f.cost) {
        addSound(NOTE_A3, noteDurationMs(16, 700));
        return;
      }
      GameProgress::setCoins(GameProgress::getCoins() - f.cost);
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

    void drawStore() {
      renderFullScreen();
      drawBranches();
      drawFoods();
      drawTopBar();
      drawSelection();
      drawNavBar();
    }

    void drawBranches() {
      for (int row = 0; row < GROCERY_BRANCH_ROWS; row++) {
        drawImageMasked(image_wood_branch, image_wood_branchMask,
                        IMAGE_WOOD_BRANCH_WIDTH, IMAGE_WOOD_BRANCH_HEIGHT,
                        0, branchTop(row));
      }
    }

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

    void eraseCell(int slot) {
      int16_t x = 0, y = 0, w = 0, h = 0;
      slotRect(slot, &x, &y, &w, &h);
      reconstructRegion(x - 6, y - 6, w + 12, h + 12);
    }

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
      const GroceryFood &f = food(selIndex);
      char buf[24];

      snprintf(buf, sizeof(buf), "Coins %d", GameProgress::getCoins());
      pill(6, 6, 112, 26, rgb565(70, 120, 120), rgb565(200, 240, 240), buf);

      costText(f.cost, buf, sizeof(buf));
      pill(SCREENWIDTH - 6 - 112, 6, 112, 26, rgb565(120, 100, 70),
           rgb565(245, 225, 190), buf);

      uint16_t nameBg = rgb565(60, 110, 110);
      _tft->fillRoundRect(6, 36, SCREENWIDTH - 12, 28, 8, nameBg);
      _tft->drawRoundRect(6, 36, SCREENWIDTH - 12, 28, 8, rgb565(210, 245, 245));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, nameBg);
      _tft->drawString(f.name, SCREENWIDTH / 2, 50, 4);

      snprintf(buf, sizeof(buf), "Hunger %+d", f.hunger);
      pill(6, 68, 112, 24, rgb565(50, 100, 60), rgb565(180, 230, 190), buf);

      snprintf(buf, sizeof(buf), "Happy %+d", f.happiness);
      uint16_t happyFill = (f.happiness >= 0) ? rgb565(120, 90, 40) : rgb565(110, 50, 50);
      uint16_t happyBorder = (f.happiness >= 0) ? rgb565(240, 210, 140) : rgb565(230, 160, 160);
      pill(SCREENWIDTH - 6 - 112, 68, 112, 24, happyFill, happyBorder, buf);

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

    void drawNavBar() {
      // Clear the nav strip back to grass before redraw (hides removed arrows).
      reconstructRegion(0, NAV_Y - 2, SCREENWIDTH, NAV_H + 4);
      navButton(HOME_X, "Home", rgb565(74, 42, 42), rgb565(210, 150, 150));
      if (hasPrevPage()) {
        drawArrowIcon(PREV_X + NAV_W / 2, NAV_Y + NAV_H / 2, /*pointRight=*/false);
      }
      if (hasNextPage()) {
        drawArrowIcon(NEXT_X + NAV_W / 2, NAV_Y + NAV_H / 2, /*pointRight=*/true);
      }
    }

    void drawArrowIcon(int16_t cx, int16_t cy, bool pointRight) {
      uint16_t outline = rgb565(12, 36, 108);
      uint16_t body = rgb565(48, 168, 248);
      uint16_t shine = rgb565(176, 228, 255);
      uint16_t shade = rgb565(28, 92, 168);

      if (pointRight) {
        _tft->fillTriangle(cx - 7, cy - 8, cx - 7, cy + 8, cx + 8, cy, outline);
        _tft->fillTriangle(cx - 6, cy - 7, cx - 6, cy + 7, cx + 6, cy, body);
        _tft->fillTriangle(cx - 6, cy - 5, cx - 6, cy + 5, cx - 2, cy, shine);
        _tft->fillTriangle(cx - 5, cy + 1, cx - 5, cy + 6, cx + 4, cy + 1, shade);
      } else {
        _tft->fillTriangle(cx + 7, cy - 8, cx + 7, cy + 8, cx - 8, cy, outline);
        _tft->fillTriangle(cx + 6, cy - 7, cx + 6, cy + 7, cx - 6, cy, body);
        _tft->fillTriangle(cx + 6, cy - 5, cx + 6, cy + 5, cx + 2, cy, shine);
        _tft->fillTriangle(cx + 5, cy + 1, cx + 5, cy + 6, cx - 4, cy + 1, shade);
      }
    }

    void navButton(int16_t x, const char *label, uint16_t fill, uint16_t border) {
      _tft->fillRoundRect(x, NAV_Y, NAV_W, NAV_H, 8, fill);
      _tft->drawRoundRect(x, NAV_Y, NAV_W, NAV_H, 8, border);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, fill);
      _tft->drawString(label, x + NAV_W / 2, NAV_Y + NAV_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

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
      if (cost <= 0) {
        snprintf(buf, n, "Free");
      } else {
        snprintf(buf, n, "Cost %d", cost);
      }
    }

    int selIndex = 0;
    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;
};

#endif
