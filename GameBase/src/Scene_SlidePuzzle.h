#ifndef _SCENE_SLIDEPUZZLE_H_
#define _SCENE_SLIDEPUZZLE_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "GameResult.h"
#include "Input.h"
#include "TouchInput.h"
#include "ml/MLGameHooks.h"
#include "image_puzzle_totoro.h"

// Sliding-tile picture puzzle over the My Neighbor Totoro poster. The poster is
// exactly 2:3, so it cuts into a 3x4 grid of 56x63 tiles with no distortion:
// eleven tiles slide around one hole, and the board is solved when every tile is
// back on its own cell.
//
// Tiles are blitted straight out of the 8-bit indexed asset instead of being
// Avatars. The board is a static grid where a move only ever changes two cells,
// so pushing those two cells beats handing the dirty-rect renderer twelve
// tightly packed sprites (whose old footprints would have to be repainted from a
// background that does not exist here). render() is therefore a no-op and every
// repaint is driven from update(), like Scene_Settings.
//
// Two modes, both measured against the same 20 s bar:
//   Time Attack - 20 s hard limit, solving in time pays PUZZLE_FAST_WIN_COINS.
//   Normal      - no limit, but beating 20 s still pays the full purse; a slower
//                 solve is still a win and pays PUZZLE_SLOW_WIN_COINS.
// The scramble is a random walk of legal slides, which both guarantees the board
// is solvable (a random permutation of a sliding puzzle is unsolvable half the
// time) and lets each mode pick its own depth.

#define PUZZLE_COLS 3
#define PUZZLE_ROWS 4
#define PUZZLE_CELLS (PUZZLE_COLS * PUZZLE_ROWS)
// Tile id of the hole. Its home is the last cell, so "solved" is tiles[i] == i.
#define PUZZLE_HOLE_TILE (PUZZLE_CELLS - 1)

#define PUZZLE_TILE_W 56
#define PUZZLE_TILE_H 63
#define PUZZLE_BOARD_W (PUZZLE_COLS * PUZZLE_TILE_W)
#define PUZZLE_BOARD_H (PUZZLE_ROWS * PUZZLE_TILE_H)
#define PUZZLE_BOARD_X ((SCREENWIDTH - PUZZLE_BOARD_W) / 2)
#define PUZZLE_BOARD_Y 60

// The header doubles as the status area: while playing it carries the clock and
// the move count, and on a result it carries the verdict, so the finished
// picture is never covered up.
#define PUZZLE_HDR_H 56
#define PUZZLE_CLOCK_CX 34
#define PUZZLE_STATUS_CX 122
#define PUZZLE_NEW_BTN_X 180
#define PUZZLE_NEW_BTN_Y 10
#define PUZZLE_NEW_BTN_W 52
#define PUZZLE_NEW_BTN_H 36

// Mode select: title, a half-scale preview of the finished picture, two buttons.
#define PUZZLE_MENU_BTN_W 184
#define PUZZLE_MENU_BTN_X ((SCREENWIDTH - PUZZLE_MENU_BTN_W) / 2)
#define PUZZLE_MENU_BTN_H 46
#define PUZZLE_MENU_BTN1_Y 216
#define PUZZLE_MENU_BTN2_Y 266
#define PUZZLE_PREVIEW_SCALE 2
#define PUZZLE_PREVIEW_W (PUZZLE_TOTORO_SHEET_WIDTH / PUZZLE_PREVIEW_SCALE)
#define PUZZLE_PREVIEW_H (PUZZLE_TOTORO_SHEET_HEIGHT / PUZZLE_PREVIEW_SCALE)
#define PUZZLE_PREVIEW_X ((SCREENWIDTH - PUZZLE_PREVIEW_W) / 2)
#define PUZZLE_PREVIEW_Y 82

// The 20 s bar: the hard limit in Time Attack, the bonus line in Normal.
#define PUZZLE_TARGET_MS 20000UL
#define PUZZLE_FAST_WIN_COINS 20
#define PUZZLE_SLOW_WIN_COINS 8

// Scramble depth in legal slides. Because the walk never doubles back, these are
// very nearly the optimal solution lengths too: tools/check_slide_puzzle.py
// solves sampled scrambles with IDA* and reports a median of ~12 slides for
// Attack and ~21 for Normal. That puts a 20 s Attack win inside reach of a
// player taking about twice the optimal number of taps, and makes the 20 s bonus
// in Normal a genuine stretch rather than a formality.
#define PUZZLE_ATTACK_SHUFFLE_MOVES 12
#define PUZZLE_NORMAL_SHUFFLE_MOVES 24

enum SlidePuzzleMode {
  PUZZLE_MODE_ATTACK = 0,
  PUZZLE_MODE_NORMAL = 1
};

enum SlidePuzzleState {
  PUZZLE_STATE_MODE_SELECT,
  PUZZLE_STATE_PLAYING,
  PUZZLE_STATE_WIN,
  PUZZLE_STATE_LOSE
};

class Scene_SlidePuzzle : public GameScene {
  public:
    Scene_SlidePuzzle(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      // Home ends the visit and collects the payout for every round solved.
      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = gameExitSceneIndex();
        return;
      }

      bool touchDown = isTouching && !wasTouching && now > suppressTouchUntilMs;
      uint16_t tx = 0;
      uint16_t ty = 0;
      bool tapped = touchDown && getTouchPoint(_tft, &tx, &ty);

      if (state == PUZZLE_STATE_MODE_SELECT) {
        if (tapped) {
          handleModeSelectTouch(tx, ty);
        }
        wasTouching = isTouching;
        return;
      }

      if (state == PUZZLE_STATE_WIN || state == PUZZLE_STATE_LOSE) {
        if (touchDown) {
          state = PUZZLE_STATE_MODE_SELECT;
          // The tap that dismissed the verdict must not also pick a mode.
          suppressTouchUntilMs = now + 250;
          drawModeSelect();
        }
        wasTouching = isTouching;
        return;
      }

      if (tapped) {
        if (inRect(tx, ty, PUZZLE_NEW_BTN_X, PUZZLE_NEW_BTN_Y,
                   PUZZLE_NEW_BTN_W, PUZZLE_NEW_BTN_H)) {
          // Re-mix without reporting anything: a stuck board costs the player
          // their time, not a loss.
          startGame(mode);
          wasTouching = isTouching;
          return;
        }
        int cell = cellAt(tx, ty);
        if (cell >= 0 && slideTile(cell)) {
          moveCount++;
          drawMoves(false);
          addSound(NOTE_E5, noteDurationMs(32, 900));
          if (isSolved()) {
            finishWin(now);
            wasTouching = isTouching;
            return;
          }
        }
      }

      drawClock(false);
      if (mode == PUZZLE_MODE_ATTACK && (now - startMs) >= PUZZLE_TARGET_MS) {
        finishTimeout(now);
      }

      wasTouching = isTouching;
    }

    // Every pixel is pushed the moment it changes, so there is nothing to do per
    // frame (see the note at the top of this file).
    void render() {}

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 400;
      state = PUZZLE_STATE_MODE_SELECT;
      setBackgroundColor(colorBg());
      drawModeSelect();
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    SlidePuzzleState state = PUZZLE_STATE_MODE_SELECT;
    SlidePuzzleMode mode = PUZZLE_MODE_ATTACK;
    int8_t tiles[PUZZLE_CELLS];
    int8_t holeCell = PUZZLE_CELLS - 1;
    int moveCount = 0;
    unsigned long startMs = 0;
    int lastClockValue = -1;
    int lastMovesValue = -1;
    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;
    char verdict1[28];
    char verdict2[32];

    uint16_t colorBg() const { return rgb565(24, 30, 26); }
    uint16_t colorHeaderBg() const { return rgb565(40, 34, 24); }
    uint16_t colorCream() const { return rgb565(238, 226, 196); }
    uint16_t colorDim() const { return rgb565(150, 145, 125); }
    uint16_t colorSeam() const { return rgb565(60, 52, 40); }
    uint16_t colorHole() const { return rgb565(18, 22, 20); }
    uint16_t colorWood() const { return rgb565(150, 105, 55); }
    uint16_t colorWoodDark() const { return rgb565(92, 62, 30); }
    uint16_t colorUrgent() const { return rgb565(240, 110, 80); }
    uint16_t colorGold() const { return rgb565(255, 220, 110); }

    static bool inRect(uint16_t tx, uint16_t ty, int16_t x, int16_t y, int16_t w, int16_t h) {
      return (tx >= x && tx < x + w && ty >= y && ty < y + h);
    }

    // ---- Mode select ---------------------------------------------------

    void drawModeSelect() {
      _tft->fillScreen(colorBg());

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorBg());
      _tft->drawString("SLIDE PUZZLE", SCREENWIDTH / 2, 38, 4);
      _tft->setTextColor(colorDim(), colorBg());
      _tft->drawString("Under 20s = 20 coins", SCREENWIDTH / 2, 64, 2);

      drawPreview();

      drawMenuButton(PUZZLE_MENU_BTN1_Y, "TIME ATTACK", "20 second limit");
      drawMenuButton(PUZZLE_MENU_BTN2_Y, "NORMAL", "no time limit");

      _tft->setTextDatum(TL_DATUM);
    }

    // Half-scale nearest-neighbour view of the finished picture, so the player
    // knows what they are building before they pick a mode.
    void drawPreview() {
      static uint16_t line[PUZZLE_PREVIEW_W];
      for (int16_t y = 0; y < PUZZLE_PREVIEW_H; y++) {
        for (int16_t x = 0; x < PUZZLE_PREVIEW_W; x++) {
          line[x] = spriteAssetPixelRgb565(&puzzle_totoro,
                                           (uint16_t)(x * PUZZLE_PREVIEW_SCALE),
                                           (uint16_t)(y * PUZZLE_PREVIEW_SCALE));
        }
        _tft->pushImage(PUZZLE_PREVIEW_X, PUZZLE_PREVIEW_Y + y, PUZZLE_PREVIEW_W, 1, line);
      }
      _tft->drawRect(PUZZLE_PREVIEW_X - 1, PUZZLE_PREVIEW_Y - 1,
                     PUZZLE_PREVIEW_W + 2, PUZZLE_PREVIEW_H + 2, colorWoodDark());
    }

    void drawMenuButton(int16_t y, const char *title, const char *subtitle) {
      _tft->fillRoundRect(PUZZLE_MENU_BTN_X, y, PUZZLE_MENU_BTN_W, PUZZLE_MENU_BTN_H, 10, colorWood());
      _tft->drawRoundRect(PUZZLE_MENU_BTN_X, y, PUZZLE_MENU_BTN_W, PUZZLE_MENU_BTN_H, 10, colorCream());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorWood());
      _tft->drawString(title, SCREENWIDTH / 2, y + 16, 4);
      _tft->drawString(subtitle, SCREENWIDTH / 2, y + 35, 2);
    }

    void handleModeSelectTouch(uint16_t x, uint16_t y) {
      if (x < PUZZLE_MENU_BTN_X || x > PUZZLE_MENU_BTN_X + PUZZLE_MENU_BTN_W) {
        return;
      }
      if (y >= PUZZLE_MENU_BTN1_Y && y <= PUZZLE_MENU_BTN1_Y + PUZZLE_MENU_BTN_H) {
        startGame(PUZZLE_MODE_ATTACK);
      } else if (y >= PUZZLE_MENU_BTN2_Y && y <= PUZZLE_MENU_BTN2_Y + PUZZLE_MENU_BTN_H) {
        startGame(PUZZLE_MODE_NORMAL);
      }
    }

    // ---- Board ---------------------------------------------------------

    void startGame(SlidePuzzleMode chosen) {
      mode = chosen;
      moveCount = 0;
      state = PUZZLE_STATE_PLAYING;
      shuffleBoard(chosen == PUZZLE_MODE_ATTACK ? PUZZLE_ATTACK_SHUFFLE_MOVES
                                                : PUZZLE_NORMAL_SHUFFLE_MOVES);

      _tft->fillScreen(colorBg());
      drawHeader();
      drawBoard(false);

      // Start the clock only once the board is on screen, so the first repaint
      // is not charged to the player.
      startMs = millis();
      lastClockValue = -1;
      lastMovesValue = -1;
      drawClock(true);
      drawMoves(true);
      addSound(NOTE_C5, noteDurationMs(16, 800));
      addSound(NOTE_G5, noteDurationMs(16, 800));
    }

    void shuffleBoard(int walkLength) {
      // A walk this long practically never lands back on the solved board, but
      // the guard makes a free win impossible rather than merely unlikely.
      do {
        for (int i = 0; i < PUZZLE_CELLS; i++) {
          tiles[i] = (int8_t)i;
        }
        holeCell = PUZZLE_CELLS - 1;
        randomWalk(walkLength);
      } while (isSolved());
    }

    void randomWalk(int steps) {
      int8_t cameFrom = -1;
      for (int i = 0; i < steps; i++) {
        int8_t options[4];
        int count = 0;
        int col = holeCell % PUZZLE_COLS;
        int row = holeCell / PUZZLE_COLS;
        if (col > 0) options[count++] = (int8_t)(holeCell - 1);
        if (col < PUZZLE_COLS - 1) options[count++] = (int8_t)(holeCell + 1);
        if (row > 0) options[count++] = (int8_t)(holeCell - PUZZLE_COLS);
        if (row < PUZZLE_ROWS - 1) options[count++] = (int8_t)(holeCell + PUZZLE_COLS);

        // Refuse to immediately undo the previous slide: an unconstrained walk
        // folds back on itself and can leave a long shuffle nearly solved.
        int8_t pick = options[random(count)];
        for (int tries = 0; tries < 6 && pick == cameFrom && count > 1; tries++) {
          pick = options[random(count)];
        }
        cameFrom = holeCell;
        moveIntoHole(pick);
      }
    }

    void moveIntoHole(int8_t cell) {
      tiles[holeCell] = tiles[cell];
      tiles[cell] = PUZZLE_HOLE_TILE;
      holeCell = cell;
    }

    // Slides the tapped tile if it borders the hole. Repaints just the two cells
    // that changed.
    bool slideTile(int cell) {
      int dc = (cell % PUZZLE_COLS) - (holeCell % PUZZLE_COLS);
      int dr = (cell / PUZZLE_COLS) - (holeCell / PUZZLE_COLS);
      if (abs(dc) + abs(dr) != 1) {
        return false;
      }
      int vacated = holeCell;
      moveIntoHole((int8_t)cell);
      drawCell(vacated, false);
      drawCell(cell, false);
      return true;
    }

    bool isSolved() const {
      for (int i = 0; i < PUZZLE_CELLS; i++) {
        if (tiles[i] != (int8_t)i) {
          return false;
        }
      }
      return true;
    }

    int cellAt(uint16_t x, uint16_t y) const {
      if (x < PUZZLE_BOARD_X || y < PUZZLE_BOARD_Y) {
        return -1;
      }
      int col = (x - PUZZLE_BOARD_X) / PUZZLE_TILE_W;
      int row = (y - PUZZLE_BOARD_Y) / PUZZLE_TILE_H;
      if (col >= PUZZLE_COLS || row >= PUZZLE_ROWS) {
        return -1;
      }
      return row * PUZZLE_COLS + col;
    }

    void drawBoard(bool complete) {
      for (int cell = 0; cell < PUZZLE_CELLS; cell++) {
        drawCell(cell, complete);
      }
    }

    // complete = draw the piece that *belongs* on this cell with no seam. On a
    // solved board that is the same picture plus the missing corner, so the
    // photo closes up as the reward for finishing.
    void drawCell(int cell, bool complete) {
      int16_t dx = PUZZLE_BOARD_X + (int16_t)(cell % PUZZLE_COLS) * PUZZLE_TILE_W;
      int16_t dy = PUZZLE_BOARD_Y + (int16_t)(cell / PUZZLE_COLS) * PUZZLE_TILE_H;

      if (!complete && tiles[cell] == PUZZLE_HOLE_TILE) {
        _tft->fillRect(dx, dy, PUZZLE_TILE_W, PUZZLE_TILE_H, colorHole());
        _tft->drawRect(dx, dy, PUZZLE_TILE_W, PUZZLE_TILE_H, colorSeam());
        return;
      }

      int src = complete ? cell : tiles[cell];
      uint16_t sx = (uint16_t)(src % PUZZLE_COLS) * PUZZLE_TILE_W;
      uint16_t sy = (uint16_t)(src / PUZZLE_COLS) * PUZZLE_TILE_H;

      static uint16_t line[PUZZLE_TILE_W];
      for (uint16_t r = 0; r < PUZZLE_TILE_H; r++) {
        for (uint16_t c = 0; c < PUZZLE_TILE_W; c++) {
          line[c] = spriteAssetPixelRgb565(&puzzle_totoro, sx + c, sy + r);
        }
        _tft->pushImage(dx, dy + r, PUZZLE_TILE_W, 1, line);
      }
      if (!complete) {
        _tft->drawRect(dx, dy, PUZZLE_TILE_W, PUZZLE_TILE_H, colorSeam());
      }
    }

    // ---- Header --------------------------------------------------------

    void drawHeader() {
      _tft->fillRect(0, 0, SCREENWIDTH, PUZZLE_HDR_H, colorHeaderBg());

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorDim(), colorHeaderBg());
      _tft->drawString("TIME", PUZZLE_CLOCK_CX, 14, 2);
      _tft->drawString(mode == PUZZLE_MODE_ATTACK ? "ATTACK" : "NORMAL",
                       PUZZLE_STATUS_CX, 14, 2);

      _tft->fillRoundRect(PUZZLE_NEW_BTN_X, PUZZLE_NEW_BTN_Y,
                          PUZZLE_NEW_BTN_W, PUZZLE_NEW_BTN_H, 8, colorWood());
      _tft->drawRoundRect(PUZZLE_NEW_BTN_X, PUZZLE_NEW_BTN_Y,
                          PUZZLE_NEW_BTN_W, PUZZLE_NEW_BTN_H, 8, colorCream());
      _tft->setTextColor(colorCream(), colorWood());
      _tft->drawString("NEW", PUZZLE_NEW_BTN_X + PUZZLE_NEW_BTN_W / 2,
                       PUZZLE_NEW_BTN_Y + PUZZLE_NEW_BTN_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // Time Attack counts the remaining seconds down (rounded up, so it shows the
    // full 20 on the first frame and 0 exactly at expiry); Normal counts up, and
    // parks at 59:59 because "60:00" is wider than the field it is erased from.
    int clockValue(unsigned long now) const {
      unsigned long elapsed = now - startMs;
      if (mode == PUZZLE_MODE_ATTACK) {
        if (elapsed >= PUZZLE_TARGET_MS) {
          return 0;
        }
        return (int)((PUZZLE_TARGET_MS - elapsed + 999) / 1000);
      }
      unsigned long seconds = elapsed / 1000;
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
      bool urgent = (mode == PUZZLE_MODE_ATTACK && value <= 5);

      _tft->fillRect(PUZZLE_CLOCK_CX - 32, 23, 64, 26, colorHeaderBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(urgent ? colorUrgent() : colorCream(), colorHeaderBg());
      _tft->drawString(buf, PUZZLE_CLOCK_CX, 36, 4);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawMoves(bool force) {
      if (!force && moveCount == lastMovesValue) {
        return;
      }
      lastMovesValue = moveCount;

      char buf[16];
      snprintf(buf, sizeof(buf), "%d moves", moveCount);
      _tft->fillRect(PUZZLE_STATUS_CX - 52, 28, 104, 20, colorHeaderBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorDim(), colorHeaderBg());
      _tft->drawString(buf, PUZZLE_STATUS_CX, 38, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // The verdict takes over the whole header, which keeps the finished picture
    // uncovered. From here a tap returns to the mode select and Home leaves.
    void drawVerdict(uint16_t accent) {
      _tft->fillRect(0, 0, SCREENWIDTH, PUZZLE_HDR_H, colorHeaderBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(accent, colorHeaderBg());
      _tft->drawString(verdict1, SCREENWIDTH / 2, 17, 2);
      _tft->setTextColor(colorCream(), colorHeaderBg());
      _tft->drawString(verdict2, SCREENWIDTH / 2, 39, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // ---- Results -------------------------------------------------------

    void finishWin(unsigned long now) {
      state = PUZZLE_STATE_WIN;
      unsigned long elapsed = now - startMs;
      int seconds = (int)(elapsed / 1000);
      // In Time Attack an over-target solve is impossible (the round ends at the
      // limit), so this only ever splits the two payouts in Normal.
      int coins = (elapsed < PUZZLE_TARGET_MS) ? PUZZLE_FAST_WIN_COINS : PUZZLE_SLOW_WIN_COINS;

      drawBoard(true);
      snprintf(verdict1, sizeof(verdict1), "SOLVED IN %ds - %d moves", seconds, moveCount);
      snprintf(verdict2, sizeof(verdict2), "+%d coins - tap to replay", coins);
      drawVerdict(colorGold());

      GameResult::report(GAME_RESULT_WIN, coins);
      mlLogGameEnd(SCENE_SLIDE_PUZZLE, GAME_RESULT_WIN, moveCount, (int)mode, (uint16_t)seconds);
      addSound(NOTE_E5, noteDurationMs(10, 900));
      addSound(NOTE_G5, noteDurationMs(10, 900));
      addSound(NOTE_C6, noteDurationMs(10, 900));
    }

    void finishTimeout(unsigned long now) {
      state = PUZZLE_STATE_LOSE;
      int seconds = (int)((now - startMs) / 1000);

      snprintf(verdict1, sizeof(verdict1), "TIME UP - %d moves", moveCount);
      snprintf(verdict2, sizeof(verdict2), "Tap to try again");
      drawVerdict(colorUrgent());

      GameResult::report(GAME_RESULT_LOSS, 0);
      mlLogGameEnd(SCENE_SLIDE_PUZZLE, GAME_RESULT_LOSS, moveCount, (int)mode, (uint16_t)seconds);
      addSound(NOTE_E4, noteDurationMs(8, 700));
      addSound(NOTE_C4, noteDurationMs(8, 700));
    }
};

#endif
