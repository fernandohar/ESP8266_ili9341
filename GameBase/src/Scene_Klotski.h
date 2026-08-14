#ifndef _SCENE_KLOTSKI_H_
#define _SCENE_KLOTSKI_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "GameResult.h"
#include "Input.h"
#include "TouchInput.h"
#include "ml/MLGameHooks.h"

// Klotski (Hua Rong Dao): slide the 2x2 block down through the gap in the bottom
// of the frame, past four upright generals, one lying general and a handful of
// soldiers. Unlike the slide puzzle there is nothing to shuffle - a Klotski
// layout is a fixed puzzle whose difficulty is entirely its geometry - so the
// three tiers are three hand-built boards instead of three scramble depths.
//
// Every piece is drawn with primitives, so the whole game costs no flash for art.
// Rendering is paint-on-change like Scene_SlidePuzzle (render() is a no-op, see
// the note in AGENTS.md): a move repaints the cells the piece left and the piece
// itself, and nothing else. The 2px inset on every piece means clearing a whole
// vacated cell can never clip the neighbour sitting against it.
//
// Input is drag-first, because most pieces have more than one legal direction and
// a bare tap cannot say which one is meant: press a piece and pull it, and it
// tracks the finger cell for cell, snapping as you cross each half cell and
// following an L-shaped drag around a corner. The target cell is recomputed from
// the original press point every frame rather than accumulated, so a long drag
// cannot overshoot and a blocked one cannot drift out of sync with the finger.
// A tap still works as a shortcut for the unambiguous case of a piece with
// exactly one legal move.

#define KLOTSKI_COLS 4
#define KLOTSKI_ROWS 5
#define KLOTSKI_CELLS (KLOTSKI_COLS * KLOTSKI_ROWS)
#define KLOTSKI_MAX_PIECES 10

#define KLOTSKI_CELL 50
#define KLOTSKI_INSET 2
#define KLOTSKI_BOARD_W (KLOTSKI_COLS * KLOTSKI_CELL)
#define KLOTSKI_BOARD_H (KLOTSKI_ROWS * KLOTSKI_CELL)
#define KLOTSKI_BOARD_X ((SCREENWIDTH - KLOTSKI_BOARD_W) / 2)
#define KLOTSKI_BOARD_Y 58

// The block escapes by reaching the bottom centre, under the gap in the frame.
#define KLOTSKI_GOAL_COL 1
#define KLOTSKI_GOAL_ROW 3

// Header geometry matches Scene_SlidePuzzle so the two puzzles read the same.
#define KLOTSKI_HDR_H 54
#define KLOTSKI_CLOCK_CX 34
#define KLOTSKI_STATUS_CX 122
#define KLOTSKI_NEW_BTN_X 180
#define KLOTSKI_NEW_BTN_Y 9
#define KLOTSKI_NEW_BTN_W 52
#define KLOTSKI_NEW_BTN_H 36

#define KLOTSKI_MENU_BTN_W 190
#define KLOTSKI_MENU_BTN_X ((SCREENWIDTH - KLOTSKI_MENU_BTN_W) / 2)
#define KLOTSKI_MENU_BTN_H 48
#define KLOTSKI_MENU_BTN1_Y 112
#define KLOTSKI_MENU_BTN2_Y 172
#define KLOTSKI_MENU_BTN3_Y 232

// Travel under this on release is a tap, not a drag.
#define KLOTSKI_TAP_MAX_PX 12

// Payout. Each tier pays for finishing, and solving close to the optimal step
// count pays a bonus on top - the par figures below are exact, so "close" is
// worth stating precisely: within half again as many steps as a perfect solve.
#define KLOTSKI_EASY_COINS 10
#define KLOTSKI_MEDIUM_COINS 15
#define KLOTSKI_CLASSIC_COINS 25
#define KLOTSKI_PAR_BONUS_COINS 10

// Minimum single-cell steps for each board, from an exhaustive breadth-first
// search of its reachable positions (tools/check_klotski.py). The classic board's
// 116 is the published figure for Hengdao Lima, which is the cross-check that the
// solver and these layouts agree with the puzzle as everyone else knows it.
#define KLOTSKI_PAR_EASY 27
#define KLOTSKI_PAR_MEDIUM 51
#define KLOTSKI_PAR_CLASSIC 116

// The three boards. Each letter is one piece, '.' is floor, and the shape of a
// piece is read from the bounding box of its letter at load time - so these rows
// are both the data and a picture of the board. The tiers are the same classic
// arrangement with two, one and no soldiers in the bottom row, which is why they
// look like one puzzle and play like three.
static const char *const KLOTSKI_LAYOUT_EASY[KLOTSKI_ROWS] = {
  "ABBC",
  "ABBC",
  "DEEF",
  "DGHF",
  "....",
};

static const char *const KLOTSKI_LAYOUT_MEDIUM[KLOTSKI_ROWS] = {
  "ABBC",
  "ABBC",
  "DEEF",
  "DGHF",
  "I...",
};

static const char *const KLOTSKI_LAYOUT_CLASSIC[KLOTSKI_ROWS] = {
  "ABBC",
  "ABBC",
  "DEEF",
  "DGHF",
  "I..J",
};

enum KlotskiTier {
  KLOTSKI_TIER_EASY = 0,
  KLOTSKI_TIER_MEDIUM = 1,
  KLOTSKI_TIER_CLASSIC = 2
};

enum KlotskiState {
  KLOTSKI_STATE_TIER_SELECT,
  KLOTSKI_STATE_PLAYING,
  KLOTSKI_STATE_WIN
};

struct KlotskiPiece {
  int8_t x;
  int8_t y;
  int8_t w;
  int8_t h;
};

class Scene_Klotski : public GameScene {
  public:
    Scene_Klotski(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = gameExitSceneIndex();
        return;
      }

      bool touchDown = isTouching && !wasTouching && now > suppressTouchUntilMs;
      uint16_t tx = 0;
      uint16_t ty = 0;
      bool havePoint = isTouching && getTouchPoint(_tft, &tx, &ty);

      if (state == KLOTSKI_STATE_TIER_SELECT) {
        if (touchDown && havePoint) {
          handleTierSelectTouch(tx, ty);
        }
        wasTouching = isTouching;
        return;
      }

      if (state == KLOTSKI_STATE_WIN) {
        if (touchDown) {
          state = KLOTSKI_STATE_TIER_SELECT;
          suppressTouchUntilMs = now + 250;
          drawTierSelect();
        }
        wasTouching = isTouching;
        return;
      }

      if (touchDown && havePoint) {
        if (inRect(tx, ty, KLOTSKI_NEW_BTN_X, KLOTSKI_NEW_BTN_Y,
                   KLOTSKI_NEW_BTN_W, KLOTSKI_NEW_BTN_H)) {
          startGame(tier);
          wasTouching = isTouching;
          return;
        }
        beginGrab(tx, ty);
      } else if (isTouching && havePoint) {
        dragGrabbed(tx, ty);
      } else if (!isTouching && wasTouching) {
        releaseGrab();
      }

      // Only while still playing: a win reached mid-drag has already repainted
      // the header with its verdict, and the clock would land on top of it.
      if (state == KLOTSKI_STATE_PLAYING) {
        drawClock(false);
      }
      wasTouching = isTouching;
    }

    void render() {}

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 400;
      state = KLOTSKI_STATE_TIER_SELECT;
      setBackgroundColor(colorBg());
      drawTierSelect();
    }

    void destroyScene() {
      wasTouching = false;
      grabbed = -1;
      GameScene::destroyScene();
    }

  private:
    KlotskiState state = KLOTSKI_STATE_TIER_SELECT;
    KlotskiTier tier = KLOTSKI_TIER_EASY;
    KlotskiPiece pieces[KLOTSKI_MAX_PIECES];
    int pieceCount = 0;
    int8_t bigPiece = -1;
    int8_t owner[KLOTSKI_CELLS];

    int stepCount = 0;
    unsigned long startMs = 0;
    int lastClockValue = -1;
    int lastStepsValue = -1;

    int8_t grabbed = -1;
    int16_t anchorX = 0;   // press point, fixed for the whole drag
    int16_t anchorY = 0;
    int16_t grabX = 0;
    int16_t grabY = 0;
    int8_t originCol = 0;  // held piece's cell at press time
    int8_t originRow = 0;
    bool grabMoved = false;

    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;
    char verdict1[32];
    char verdict2[32];

    uint16_t colorBg() const { return rgb565(26, 24, 22); }
    uint16_t colorHeaderBg() const { return rgb565(40, 34, 24); }
    uint16_t colorCream() const { return rgb565(238, 226, 196); }
    uint16_t colorDim() const { return rgb565(150, 145, 125); }
    uint16_t colorGold() const { return rgb565(255, 220, 110); }
    uint16_t colorFloor() const { return rgb565(58, 44, 32); }
    uint16_t colorGoalFloor() const { return rgb565(78, 60, 40); }
    uint16_t colorFrame() const { return rgb565(120, 86, 46); }
    uint16_t colorEdgeDark() const { return rgb565(70, 46, 22); }
    uint16_t colorEdgeLight() const { return rgb565(240, 224, 180); }
    uint16_t colorWoodLight() const { return rgb565(214, 176, 120); }
    uint16_t colorWoodMid() const { return rgb565(196, 154, 96); }
    uint16_t colorWoodTan() const { return rgb565(178, 132, 78); }
    uint16_t colorTotoro() const { return rgb565(108, 112, 120); }
    uint16_t colorTotoroEar() const { return rgb565(78, 82, 90); }
    uint16_t colorBelly() const { return rgb565(228, 216, 192); }

    static bool inRect(uint16_t tx, uint16_t ty, int16_t x, int16_t y, int16_t w, int16_t h) {
      return (tx >= x && tx < x + w && ty >= y && ty < y + h);
    }

    static int16_t cellX(int col) { return KLOTSKI_BOARD_X + (int16_t)col * KLOTSKI_CELL; }
    static int16_t cellY(int row) { return KLOTSKI_BOARD_Y + (int16_t)row * KLOTSKI_CELL; }

    // ---- Tier select ---------------------------------------------------

    void drawTierSelect() {
      _tft->fillScreen(colorBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorBg());
      _tft->drawString("KLOTSKI", SCREENWIDTH / 2, 36, 4);
      _tft->setTextColor(colorDim(), colorBg());
      _tft->drawString("Slide Totoro out", SCREENWIDTH / 2, 64, 2);
      _tft->drawString("through the bottom gap", SCREENWIDTH / 2, 82, 2);

      drawTierButton(KLOTSKI_MENU_BTN1_Y, "EASY", KLOTSKI_PAR_EASY, KLOTSKI_EASY_COINS);
      drawTierButton(KLOTSKI_MENU_BTN2_Y, "MEDIUM", KLOTSKI_PAR_MEDIUM, KLOTSKI_MEDIUM_COINS);
      drawTierButton(KLOTSKI_MENU_BTN3_Y, "CLASSIC", KLOTSKI_PAR_CLASSIC, KLOTSKI_CLASSIC_COINS);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawTierButton(int16_t y, const char *title, int par, int coins) {
      _tft->fillRoundRect(KLOTSKI_MENU_BTN_X, y, KLOTSKI_MENU_BTN_W, KLOTSKI_MENU_BTN_H,
                          10, colorWoodTan());
      _tft->drawRoundRect(KLOTSKI_MENU_BTN_X, y, KLOTSKI_MENU_BTN_W, KLOTSKI_MENU_BTN_H,
                          10, colorCream());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(48, 30, 14), colorWoodTan());
      _tft->drawString(title, SCREENWIDTH / 2, y + 17, 4);

      char buf[28];
      snprintf(buf, sizeof(buf), "par %d steps  %d coins", par, coins);
      _tft->drawString(buf, SCREENWIDTH / 2, y + 36, 2);
    }

    void handleTierSelectTouch(uint16_t x, uint16_t y) {
      if (x < KLOTSKI_MENU_BTN_X || x > KLOTSKI_MENU_BTN_X + KLOTSKI_MENU_BTN_W) {
        return;
      }
      if (y >= KLOTSKI_MENU_BTN1_Y && y <= KLOTSKI_MENU_BTN1_Y + KLOTSKI_MENU_BTN_H) {
        startGame(KLOTSKI_TIER_EASY);
      } else if (y >= KLOTSKI_MENU_BTN2_Y && y <= KLOTSKI_MENU_BTN2_Y + KLOTSKI_MENU_BTN_H) {
        startGame(KLOTSKI_TIER_MEDIUM);
      } else if (y >= KLOTSKI_MENU_BTN3_Y && y <= KLOTSKI_MENU_BTN3_Y + KLOTSKI_MENU_BTN_H) {
        startGame(KLOTSKI_TIER_CLASSIC);
      }
    }

    // ---- Board setup ---------------------------------------------------

    const char *const *layoutFor(KlotskiTier which) const {
      switch (which) {
        case KLOTSKI_TIER_MEDIUM: return KLOTSKI_LAYOUT_MEDIUM;
        case KLOTSKI_TIER_CLASSIC: return KLOTSKI_LAYOUT_CLASSIC;
        default: return KLOTSKI_LAYOUT_EASY;
      }
    }

    int parFor(KlotskiTier which) const {
      switch (which) {
        case KLOTSKI_TIER_MEDIUM: return KLOTSKI_PAR_MEDIUM;
        case KLOTSKI_TIER_CLASSIC: return KLOTSKI_PAR_CLASSIC;
        default: return KLOTSKI_PAR_EASY;
      }
    }

    int coinsFor(KlotskiTier which) const {
      switch (which) {
        case KLOTSKI_TIER_MEDIUM: return KLOTSKI_MEDIUM_COINS;
        case KLOTSKI_TIER_CLASSIC: return KLOTSKI_CLASSIC_COINS;
        default: return KLOTSKI_EASY_COINS;
      }
    }

    const char *tierName(KlotskiTier which) const {
      switch (which) {
        case KLOTSKI_TIER_MEDIUM: return "MEDIUM";
        case KLOTSKI_TIER_CLASSIC: return "CLASSIC";
        default: return "EASY";
      }
    }

    // Reads the layout art into pieces. A piece's rectangle is the bounding box
    // of its letter, so the art cannot disagree with the piece list.
    void loadLayout(const char *const *rows) {
      pieceCount = 0;
      bigPiece = -1;
      for (char letter = 'A'; letter <= 'Z'; letter++) {
        int8_t minX = KLOTSKI_COLS;
        int8_t minY = KLOTSKI_ROWS;
        int8_t maxX = -1;
        int8_t maxY = -1;
        for (int8_t row = 0; row < KLOTSKI_ROWS; row++) {
          for (int8_t col = 0; col < KLOTSKI_COLS; col++) {
            if (rows[row][col] != letter) {
              continue;
            }
            if (col < minX) minX = col;
            if (col > maxX) maxX = col;
            if (row < minY) minY = row;
            if (row > maxY) maxY = row;
          }
        }
        if (maxX < 0 || pieceCount >= KLOTSKI_MAX_PIECES) {
          continue;
        }
        KlotskiPiece &p = pieces[pieceCount];
        p.x = minX;
        p.y = minY;
        p.w = (int8_t)(maxX - minX + 1);
        p.h = (int8_t)(maxY - minY + 1);
        if (p.w == 2 && p.h == 2) {
          bigPiece = (int8_t)pieceCount;
        }
        pieceCount++;
      }
      rebuildOwners();
    }

    void rebuildOwners() {
      for (int i = 0; i < KLOTSKI_CELLS; i++) {
        owner[i] = -1;
      }
      for (int i = 0; i < pieceCount; i++) {
        const KlotskiPiece &p = pieces[i];
        for (int8_t dy = 0; dy < p.h; dy++) {
          for (int8_t dx = 0; dx < p.w; dx++) {
            owner[(p.y + dy) * KLOTSKI_COLS + (p.x + dx)] = (int8_t)i;
          }
        }
      }
    }

    void startGame(KlotskiTier chosen) {
      tier = chosen;
      stepCount = 0;
      grabbed = -1;
      state = KLOTSKI_STATE_PLAYING;
      loadLayout(layoutFor(chosen));

      _tft->fillScreen(colorBg());
      drawHeader();
      drawBoardFrame();
      drawFloor();
      for (int i = 0; i < pieceCount; i++) {
        drawPiece(i);
      }

      startMs = millis();
      lastClockValue = -1;
      lastStepsValue = -1;
      drawClock(true);
      drawSteps(true);
      addSound(NOTE_C5, noteDurationMs(16, 800));
      addSound(NOTE_G5, noteDurationMs(16, 800));
    }

    // ---- Moves ---------------------------------------------------------

    int pieceAt(uint16_t px, uint16_t py) const {
      if (px < KLOTSKI_BOARD_X || py < KLOTSKI_BOARD_Y) {
        return -1;
      }
      int col = (px - KLOTSKI_BOARD_X) / KLOTSKI_CELL;
      int row = (py - KLOTSKI_BOARD_Y) / KLOTSKI_CELL;
      if (col >= KLOTSKI_COLS || row >= KLOTSKI_ROWS) {
        return -1;
      }
      return owner[row * KLOTSKI_COLS + col];
    }

    bool canMove(int index, int dx, int dy) const {
      const KlotskiPiece &p = pieces[index];
      int8_t nx = (int8_t)(p.x + dx);
      int8_t ny = (int8_t)(p.y + dy);
      if (nx < 0 || ny < 0 || nx + p.w > KLOTSKI_COLS || ny + p.h > KLOTSKI_ROWS) {
        return false;
      }
      for (int8_t cy = ny; cy < ny + p.h; cy++) {
        for (int8_t cx = nx; cx < nx + p.w; cx++) {
          int8_t at = owner[cy * KLOTSKI_COLS + cx];
          if (at >= 0 && at != (int8_t)index) {
            return false;
          }
        }
      }
      return true;
    }

    // Moves a piece one cell and repaints only what that changed.
    void applyMove(int index, int dx, int dy) {
      KlotskiPiece &p = pieces[index];
      int8_t oldX = p.x;
      int8_t oldY = p.y;
      p.x = (int8_t)(p.x + dx);
      p.y = (int8_t)(p.y + dy);
      rebuildOwners();

      // The vacated cells, plus the moved piece - which for a 1x1 shares no cell
      // with where it was, so it is never covered by the repaint above.
      repaintCells(oldX, oldY, p.w, p.h);
      drawPiece(index);
      stepCount++;
      drawSteps(false);
      addSound(NOTE_A4, noteDurationMs(32, 900));
    }

    void beginGrab(uint16_t px, uint16_t py) {
      grabbed = (int8_t)pieceAt(px, py);
      anchorX = (int16_t)px;
      anchorY = (int16_t)py;
      grabX = (int16_t)px;
      grabY = (int16_t)py;
      grabMoved = false;
      if (grabbed >= 0) {
        originCol = pieces[grabbed].x;
        originRow = pieces[grabbed].y;
      }
    }

    // Finger travel in whole cells, rounded to nearest: the piece commits to a
    // cell as soon as the finger is past that cell's halfway point.
    static int pixelsToCells(int16_t delta) {
      if (delta >= 0) {
        return (delta + KLOTSKI_CELL / 2) / KLOTSKI_CELL;
      }
      return -((-delta + KLOTSKI_CELL / 2) / KLOTSKI_CELL);
    }

    // Walks the held piece towards the cell the finger is over, one step at a
    // time, taking the longer axis first so an L-shaped drag rounds the corner.
    void dragGrabbed(uint16_t px, uint16_t py) {
      if (grabbed < 0) {
        return;
      }
      grabX = (int16_t)px;
      grabY = (int16_t)py;
      int wantCol = originCol + pixelsToCells(grabX - anchorX);
      int wantRow = originRow + pixelsToCells(grabY - anchorY);

      // Each pass either steps the piece (at most COLS-1 + ROWS-1 of those) or
      // gives up on one axis (at most one per axis), so this can never cut a legal
      // drag short - it only stops a bug from spinning.
      for (int guard = 0; guard < KLOTSKI_COLS + KLOTSKI_ROWS + 2; guard++) {
        int needX = wantCol - pieces[grabbed].x;
        int needY = wantRow - pieces[grabbed].y;
        int absX = needX < 0 ? -needX : needX;
        int absY = needY < 0 ? -needY : needY;
        if (absX == 0 && absY == 0) {
          return;
        }
        int stepX = 0;
        int stepY = 0;
        if (absX >= absY) {
          stepX = (needX > 0) ? 1 : -1;
        } else {
          stepY = (needY > 0) ? 1 : -1;
        }
        if (!canMove(grabbed, stepX, stepY)) {
          // Blocked on this axis: drop the request for it so the other axis still
          // gets a turn. The next frame recomputes both from the finger again.
          if (stepX != 0) {
            wantCol = pieces[grabbed].x;
          } else {
            wantRow = pieces[grabbed].y;
          }
          continue;
        }
        applyMove(grabbed, stepX, stepY);
        grabMoved = true;
        if (checkWin()) {
          return;
        }
      }
    }

    void releaseGrab() {
      int8_t index = grabbed;
      grabbed = -1;
      if (index < 0 || grabMoved || state != KLOTSKI_STATE_PLAYING) {
        return;
      }
      int16_t dx = grabX - anchorX;
      int16_t dy = grabY - anchorY;
      int16_t travel = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
      if (travel > KLOTSKI_TAP_MAX_PX) {
        return;
      }
      // A tap only commits when there is nothing to guess: exactly one way to go.
      int onlyX = 0;
      int onlyY = 0;
      int options = 0;
      static const int8_t DIRS[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
      for (int d = 0; d < 4; d++) {
        if (canMove(index, DIRS[d][0], DIRS[d][1])) {
          onlyX = DIRS[d][0];
          onlyY = DIRS[d][1];
          options++;
        }
      }
      if (options == 1) {
        applyMove(index, onlyX, onlyY);
        checkWin();
      }
    }

    bool checkWin() {
      if (bigPiece < 0) {
        return false;
      }
      const KlotskiPiece &big = pieces[bigPiece];
      if (big.x != KLOTSKI_GOAL_COL || big.y != KLOTSKI_GOAL_ROW) {
        return false;
      }
      finishWin(millis());
      return true;
    }

    // ---- Drawing -------------------------------------------------------

    void drawBoardFrame() {
      // Two-pixel frame around the floor, broken by the exit gap under the goal
      // columns so the way out is visible before any piece reaches it.
      for (int8_t i = 1; i <= 2; i++) {
        _tft->drawRect(KLOTSKI_BOARD_X - i, KLOTSKI_BOARD_Y - i,
                       KLOTSKI_BOARD_W + i * 2, KLOTSKI_BOARD_H + i * 2, colorFrame());
      }
      int16_t gapX = cellX(KLOTSKI_GOAL_COL);
      _tft->fillRect(gapX, KLOTSKI_BOARD_Y + KLOTSKI_BOARD_H, KLOTSKI_CELL * 2, 2, colorBg());

      // Two arrows below the gap, pointing the way out.
      uint16_t tip = colorFrame();
      for (int8_t k = 0; k < 2; k++) {
        int16_t cx = gapX + KLOTSKI_CELL / 2 + k * KLOTSKI_CELL;
        int16_t top = KLOTSKI_BOARD_Y + KLOTSKI_BOARD_H + 4;
        _tft->fillTriangle(cx - 7, top, cx + 7, top, cx, top + 7, tip);
      }
    }

    void drawFloor() {
      _tft->fillRect(KLOTSKI_BOARD_X, KLOTSKI_BOARD_Y,
                     KLOTSKI_BOARD_W, KLOTSKI_BOARD_H, colorFloor());
      drawGoalHint();
    }

    // The 2x2 target, tinted and outlined under the pieces. Redrawn after every
    // clear because a vacated region can overlap part of it.
    void drawGoalHint() {
      int16_t gx = cellX(KLOTSKI_GOAL_COL);
      int16_t gy = cellY(KLOTSKI_GOAL_ROW);
      int16_t size = KLOTSKI_CELL * 2;
      _tft->fillRect(gx, gy, size, size, colorGoalFloor());
      for (int16_t x = gx + 3; x < gx + size - 3; x += 6) {
        _tft->drawFastHLine(x, gy + 3, 3, colorFrame());
        _tft->drawFastHLine(x, gy + size - 4, 3, colorFrame());
      }
      for (int16_t y = gy + 3; y < gy + size - 3; y += 6) {
        _tft->drawFastVLine(gx + 3, y, 3, colorFrame());
        _tft->drawFastVLine(gx + size - 4, y, 3, colorFrame());
      }
    }

    // Repaints a rectangle of cells: floor, the goal hint, then every piece still
    // standing in the repainted area. The hint is drawn whole rather than clipped,
    // so when the two overlap the area is widened to cover all of it - otherwise
    // redrawing the hint would wipe a piece parked on the goal square, which on
    // the easy and medium boards is where two of the soldiers start.
    void repaintCells(int8_t col, int8_t row, int8_t w, int8_t h) {
      int8_t x0 = col;
      int8_t y0 = row;
      int8_t x1 = (int8_t)(col + w);
      int8_t y1 = (int8_t)(row + h);
      _tft->fillRect(cellX(x0), cellY(y0),
                     (int16_t)(x1 - x0) * KLOTSKI_CELL,
                     (int16_t)(y1 - y0) * KLOTSKI_CELL, colorFloor());

      bool touchesGoal = !(x1 <= KLOTSKI_GOAL_COL || x0 >= KLOTSKI_GOAL_COL + 2 ||
                           y1 <= KLOTSKI_GOAL_ROW || y0 >= KLOTSKI_GOAL_ROW + 2);
      if (touchesGoal) {
        drawGoalHint();
        if (KLOTSKI_GOAL_COL < x0) x0 = KLOTSKI_GOAL_COL;
        if (KLOTSKI_GOAL_ROW < y0) y0 = KLOTSKI_GOAL_ROW;
        if (KLOTSKI_GOAL_COL + 2 > x1) x1 = KLOTSKI_GOAL_COL + 2;
        if (KLOTSKI_GOAL_ROW + 2 > y1) y1 = KLOTSKI_GOAL_ROW + 2;
      }

      for (int i = 0; i < pieceCount; i++) {
        const KlotskiPiece &p = pieces[i];
        if (p.x < x1 && p.x + p.w > x0 && p.y < y1 && p.y + p.h > y0) {
          drawPiece(i);
        }
      }
    }

    uint16_t pieceColor(const KlotskiPiece &p) const {
      if (p.w == 2 && p.h == 2) return colorTotoro();
      if (p.w == 2) return colorWoodMid();
      if (p.h == 2) return colorWoodLight();
      return colorWoodTan();
    }

    void drawPiece(int index) {
      const KlotskiPiece &p = pieces[index];
      int16_t x = cellX(p.x) + KLOTSKI_INSET;
      int16_t y = cellY(p.y) + KLOTSKI_INSET;
      int16_t w = (int16_t)p.w * KLOTSKI_CELL - KLOTSKI_INSET * 2;
      int16_t h = (int16_t)p.h * KLOTSKI_CELL - KLOTSKI_INSET * 2;
      uint16_t body = pieceColor(p);

      _tft->fillRoundRect(x, y, w, h, 6, body);
      _tft->drawRoundRect(x, y, w, h, 6, colorEdgeDark());
      // Bevel: a light edge along the top and left reads as a raised block.
      _tft->drawFastHLine(x + 4, y + 2, w - 8, colorEdgeLight());
      _tft->drawFastVLine(x + 2, y + 4, h - 8, colorEdgeLight());

      if (index == bigPiece) {
        drawTotoro(x, y, w, h);
      }
    }

    // The block the player is trying to free, drawn as Totoro so it is obvious at
    // a glance which piece the puzzle is about.
    void drawTotoro(int16_t x, int16_t y, int16_t w, int16_t h) {
      int16_t cx = x + w / 2;
      _tft->fillTriangle(cx - 26, y + 12, cx - 15, y + 1, cx - 8, y + 14, colorTotoroEar());
      _tft->fillTriangle(cx + 26, y + 12, cx + 15, y + 1, cx + 8, y + 14, colorTotoroEar());

      _tft->fillEllipse(cx, y + h - 28, w / 3, h / 4, colorBelly());

      int16_t eyeY = y + h / 3;
      for (int8_t side = -1; side <= 1; side += 2) {
        int16_t ex = cx + side * 15;
        _tft->fillCircle(ex, eyeY, 7, colorBelly());
        _tft->fillCircle(ex, eyeY, 3, rgb565(30, 28, 26));
      }
      _tft->fillTriangle(cx - 4, eyeY + 11, cx + 4, eyeY + 11, cx, eyeY + 16,
                         rgb565(40, 36, 32));
    }

    // ---- Header --------------------------------------------------------

    void drawHeader() {
      _tft->fillRect(0, 0, SCREENWIDTH, KLOTSKI_HDR_H, colorHeaderBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorDim(), colorHeaderBg());
      _tft->drawString("TIME", KLOTSKI_CLOCK_CX, 13, 2);
      _tft->drawString(tierName(tier), KLOTSKI_STATUS_CX, 13, 2);

      _tft->fillRoundRect(KLOTSKI_NEW_BTN_X, KLOTSKI_NEW_BTN_Y,
                          KLOTSKI_NEW_BTN_W, KLOTSKI_NEW_BTN_H, 8, colorWoodTan());
      _tft->drawRoundRect(KLOTSKI_NEW_BTN_X, KLOTSKI_NEW_BTN_Y,
                          KLOTSKI_NEW_BTN_W, KLOTSKI_NEW_BTN_H, 8, colorCream());
      _tft->setTextColor(rgb565(48, 30, 14), colorWoodTan());
      _tft->drawString("RESET", KLOTSKI_NEW_BTN_X + KLOTSKI_NEW_BTN_W / 2,
                       KLOTSKI_NEW_BTN_Y + KLOTSKI_NEW_BTN_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // Counts up; parked at 59:59 because "60:00" is wider than its erase field.
    int clockValue(unsigned long now) const {
      unsigned long seconds = (now - startMs) / 1000;
      return (int)(seconds > 3599 ? 3599 : seconds);
    }

    void drawClock(bool force) {
      int value = clockValue(millis());
      if (!force && value == lastClockValue) {
        return;
      }
      lastClockValue = value;
      char buf[8];
      snprintf(buf, sizeof(buf), "%d:%02d", value / 60, value % 60);
      _tft->fillRect(KLOTSKI_CLOCK_CX - 32, 22, 64, 26, colorHeaderBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorHeaderBg());
      _tft->drawString(buf, KLOTSKI_CLOCK_CX, 35, 4);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawSteps(bool force) {
      if (!force && stepCount == lastStepsValue) {
        return;
      }
      lastStepsValue = stepCount;
      char buf[20];
      // Clamped because a 4-digit count would outgrow the field it is erased from,
      // and by then the exact number has stopped meaning anything anyway.
      snprintf(buf, sizeof(buf), "%d / %d steps",
               stepCount > 999 ? 999 : stepCount, parFor(tier));
      _tft->fillRect(KLOTSKI_STATUS_CX - 54, 27, 108, 20, colorHeaderBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(stepCount > parBonusLimit() ? colorDim() : colorGold(),
                         colorHeaderBg());
      _tft->drawString(buf, KLOTSKI_STATUS_CX, 37, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    int parBonusLimit() const { return parFor(tier) + parFor(tier) / 2; }

    void drawVerdict(uint16_t accent) {
      _tft->fillRect(0, 0, SCREENWIDTH, KLOTSKI_HDR_H, colorHeaderBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(accent, colorHeaderBg());
      _tft->drawString(verdict1, SCREENWIDTH / 2, 16, 2);
      _tft->setTextColor(colorCream(), colorHeaderBg());
      _tft->drawString(verdict2, SCREENWIDTH / 2, 38, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void finishWin(unsigned long now) {
      state = KLOTSKI_STATE_WIN;
      grabbed = -1;
      // The win usually lands mid-drag, so hold off the tap that dismisses the
      // verdict long enough for it to be read.
      suppressTouchUntilMs = now + 700;
      int seconds = (int)((now - startMs) / 1000);
      bool underPar = stepCount <= parBonusLimit();
      int coins = coinsFor(tier) + (underPar ? KLOTSKI_PAR_BONUS_COINS : 0);

      // Ring the freed block so the finish reads at a glance.
      const KlotskiPiece &big = pieces[bigPiece];
      for (int8_t i = 0; i < 2; i++) {
        _tft->drawRoundRect(cellX(big.x) + i, cellY(big.y) + i,
                            (int16_t)big.w * KLOTSKI_CELL - i * 2,
                            (int16_t)big.h * KLOTSKI_CELL - i * 2, 8, colorGold());
      }

      snprintf(verdict1, sizeof(verdict1), "OUT IN %d steps, %d:%02d",
               stepCount, seconds / 60, seconds % 60);
      if (underPar) {
        snprintf(verdict2, sizeof(verdict2), "+%d coins, under par!", coins);
      } else {
        snprintf(verdict2, sizeof(verdict2), "+%d coins - tap to replay", coins);
      }
      drawVerdict(colorGold());

      GameResult::report(GAME_RESULT_WIN, coins);
      mlLogGameEnd(SCENE_KLOTSKI, GAME_RESULT_WIN, stepCount, (int)tier, (uint16_t)seconds);
      addSound(NOTE_E5, noteDurationMs(10, 900));
      addSound(NOTE_G5, noteDurationMs(10, 900));
      addSound(NOTE_C6, noteDurationMs(10, 900));
    }
};

#endif
