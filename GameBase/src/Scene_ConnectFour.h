#ifndef _SCENE_CONNECTFOUR_H_
#define _SCENE_CONNECTFOUR_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "GameResult.h"
#include "Input.h"
#include "TouchInput.h"
#include "ml/MLGameHooks.h"

// Four in a Row on the classic 7x6 grid: tap a column, a piece drops down it, and
// four in a line wins. The pieces are a white Totoro and a soot sprite, and the
// board is the lime green of the Ghibli set - all drawn with primitives, so like
// Scene_Klotski the whole game costs nothing in flash for art.
//
// Rendering is paint-on-change (see AGENTS.md): render() is a no-op and every
// repaint is a single cell, driven from update(). Pieces are inset inside their
// cell, so repainting one cell can never clip its neighbour.
//
// Difficulty is search depth and nothing else. An early version borrowed
// Tic-Tac-Toe's trick of randomly ignoring its own rules, and it does not
// transfer: Four in a Row punishes a single slip much harder than Tic-Tac-Toe,
// where a blunder usually only costs the draw. Measured over 60 games against a
// steady reference player, one blunder in five dropped the CPU from 25 wins to 7,
// and one in three to a single win - the difference between a thoughtful opponent
// and a broken one. So the CPU here never blunders on purpose; the easy setting
// simply cannot see very far. See tools/check_connect4.py.

#define CONNECT4_COLS 7
#define CONNECT4_ROWS 6
#define CONNECT4_CELLS (CONNECT4_COLS * CONNECT4_ROWS)
#define CONNECT4_NEED 4

#define CONNECT4_EMPTY 0
#define CONNECT4_TOTORO 1   // player 1, and the human in a 1P game
#define CONNECT4_SOOT 2     // player 2, and the CPU in a 1P game

#define CONNECT4_CELL 32
#define CONNECT4_BOARD_W (CONNECT4_COLS * CONNECT4_CELL)
#define CONNECT4_BOARD_H (CONNECT4_ROWS * CONNECT4_CELL)
#define CONNECT4_BOARD_X ((SCREENWIDTH - CONNECT4_BOARD_W) / 2)
#define CONNECT4_BOARD_Y 62
#define CONNECT4_PIECE_R 13
#define CONNECT4_HOLE_R 14

// A column is tappable a little above and below the board itself, so aiming at
// the column matters but aiming at a row does not. Kept short enough that the
// pad stops below the turn banner and above the status pill.
#define CONNECT4_TAP_PAD 12

#define CONNECT4_TURN_CY 32
#define CONNECT4_TURN_H 30
#define CONNECT4_STATUS_CY 282
#define CONNECT4_STATUS_H 26
#define CONNECT4_RESULT_Y 266
#define CONNECT4_RESULT_H 44

#define CONNECT4_BTN_W 184
#define CONNECT4_BTN_H 48
#define CONNECT4_BTN_X ((SCREENWIDTH - CONNECT4_BTN_W) / 2)
#define CONNECT4_BTN1_Y 132
#define CONNECT4_BTN2_Y 190
#define CONNECT4_BTN3_Y 248

// Search depth per setting, in plies. Both are even on purpose: this evaluation
// is taken after the side to move has played, so an odd depth scores every leaf
// straight after the CPU's own move and reads as optimistic. Depth 2 still always
// takes a win and blocks an immediate one - it just sees nothing beyond that -
// while depth 6 beat that depth-2 opponent 19-1 over 20 games.
#define CONNECT4_DEPTH_EASY 2
#define CONNECT4_DEPTH_HARD 6

// Worst case measured off-device: ~13.8k leaf evaluations for one depth-6 move,
// which is a fraction of a second and happens only on the CPU's turn, with the
// "thinking" status already on screen.
#define CONNECT4_WIN_SCORE 100000

// Weight of a line of four holding 0..3 of one colour and nothing of the other.
static const int CONNECT4_WINDOW_WEIGHT[4] = { 0, 1, 8, 40 };
#define CONNECT4_CENTER_WEIGHT 3

// One decisive round pays this. A little above Tic-Tac-Toe's 6, since a round
// lasts longer and the CPU actually searches.
#define CONNECT4_WIN_COINS 8

// Frames between steps of the falling piece, in scene ticks (50ms each).
#define CONNECT4_FALL_TICKS 1

enum ConnectFourState {
  C4_STATE_MODE_SELECT,
  C4_STATE_PLAYING,
  C4_STATE_WIN,
  C4_STATE_DRAW
};

class Scene_ConnectFour : public GameScene {
  public:
    Scene_ConnectFour(TFT_eSPI *tft) {
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

      if (state == C4_STATE_MODE_SELECT) {
        if (touchDown) {
          handleModeSelectTouch();
        }
        wasTouching = isTouching;
        return;
      }

      if (state == C4_STATE_WIN || state == C4_STATE_DRAW) {
        if (touchDown) {
          state = C4_STATE_MODE_SELECT;
          suppressTouchUntilMs = now + 250;
          drawModeSelect();
        }
        wasTouching = isTouching;
        return;
      }

      if (dropping) {
        advanceDrop();
        wasTouching = isTouching;
        return;
      }

      // The CPU thinks one tick after the status says so, so the message is on
      // screen before the search stalls the loop.
      if (aiPending) {
        aiPending = false;
        int col = aiChooseColumn(CONNECT4_SOOT, aiDepth);
        if (col >= 0) {
          startDrop(col, CONNECT4_SOOT);
        }
        wasTouching = isTouching;
        return;
      }

      if (touchDown) {
        uint16_t tx = 0;
        uint16_t ty = 0;
        if (getTouchPoint(_tft, &tx, &ty)) {
          handlePlayTouch(tx, ty);
        }
      }
      wasTouching = isTouching;
    }

    void render() {}

    void initScene() {
      buildWindows();
      wasTouching = false;
      suppressTouchUntilMs = millis() + 400;
      numPlayers = 1;
      aiDepth = CONNECT4_DEPTH_EASY;
      state = C4_STATE_MODE_SELECT;
      setBackgroundColor(colorBg());
      drawModeSelect();
    }

    void destroyScene() {
      wasTouching = false;
      dropping = false;
      aiPending = false;
      GameScene::destroyScene();
    }

  private:
    ConnectFourState state = C4_STATE_MODE_SELECT;
    int8_t cells[CONNECT4_CELLS];   // row 0 is the top row, as drawn
    int8_t heights[CONNECT4_COLS];  // pieces stacked in each column
    int8_t turn = CONNECT4_TOTORO;
    int numPlayers = 1;
    int aiDepth = CONNECT4_DEPTH_EASY;

    // Every line of four on the board, as cell indices. Built once at load rather
    // than written out, which keeps it honest about the board dimensions.
    uint8_t windows[70][CONNECT4_NEED];
    int windowCount = 0;

    bool dropping = false;
    int8_t dropCol = 0;
    int8_t dropRow = 0;
    int8_t dropTargetRow = 0;
    int8_t dropPlayer = CONNECT4_EMPTY;
    int8_t fallTick = 0;

    bool aiPending = false;
    int8_t lastCol = -1;
    int8_t lastRow = -1;
    int8_t winCells[CONNECT4_NEED];
    int winCellCount = 0;

    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;
    unsigned long gameStartMs = 0;
    int moveCount = 0;

    uint16_t colorBg() const { return rgb565(38, 46, 30); }
    uint16_t colorBoard() const { return rgb565(150, 200, 62); }
    uint16_t colorBoardRim() const { return rgb565(96, 140, 36); }
    uint16_t colorHole() const { return rgb565(86, 126, 34); }
    uint16_t colorCream() const { return rgb565(244, 240, 226); }
    uint16_t colorTotoroLine() const { return rgb565(90, 96, 88); }
    uint16_t colorSoot() const { return rgb565(32, 30, 34); }
    uint16_t colorSootFuzz() const { return rgb565(58, 54, 60); }
    uint16_t colorEye() const { return rgb565(250, 250, 248); }
    uint16_t colorPupil() const { return rgb565(26, 24, 26); }
    uint16_t colorPanel() const { return rgb565(28, 40, 24); }
    uint16_t colorPanelLight() const { return rgb565(58, 82, 40); }
    uint16_t colorGold() const { return rgb565(255, 216, 74); }

    static int16_t cellCX(int col) {
      return CONNECT4_BOARD_X + (int16_t)col * CONNECT4_CELL + CONNECT4_CELL / 2;
    }
    static int16_t cellCY(int row) {
      return CONNECT4_BOARD_Y + (int16_t)row * CONNECT4_CELL + CONNECT4_CELL / 2;
    }

    // ---- Board model ---------------------------------------------------

    void buildWindows() {
      windowCount = 0;
      static const int8_t DIRS[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
      for (int row = 0; row < CONNECT4_ROWS; row++) {
        for (int col = 0; col < CONNECT4_COLS; col++) {
          for (int d = 0; d < 4; d++) {
            int endCol = col + DIRS[d][0] * (CONNECT4_NEED - 1);
            int endRow = row + DIRS[d][1] * (CONNECT4_NEED - 1);
            if (endCol < 0 || endCol >= CONNECT4_COLS ||
                endRow < 0 || endRow >= CONNECT4_ROWS) {
              continue;
            }
            for (int i = 0; i < CONNECT4_NEED; i++) {
              int c = col + DIRS[d][0] * i;
              int r = row + DIRS[d][1] * i;
              windows[windowCount][i] = (uint8_t)(r * CONNECT4_COLS + c);
            }
            windowCount++;
          }
        }
      }
    }

    static int8_t opponentOf(int8_t player) {
      return (player == CONNECT4_TOTORO) ? CONNECT4_SOOT : CONNECT4_TOTORO;
    }

    bool columnOpen(int col) const { return heights[col] < CONNECT4_ROWS; }

    int landingRow(int col) const { return CONNECT4_ROWS - 1 - heights[col]; }

    int dropInto(int col, int8_t player) {
      int row = landingRow(col);
      cells[row * CONNECT4_COLS + col] = player;
      heights[col]++;
      return row;
    }

    void undoDrop(int col) {
      heights[col]--;
      cells[landingRow(col) * CONNECT4_COLS + col] = CONNECT4_EMPTY;
    }

    // Four in a line through (col,row), which is the only place a new one can be.
    bool makesFour(int col, int row, int8_t player) const {
      static const int8_t DIRS[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
      for (int d = 0; d < 4; d++) {
        int run = 1;
        for (int sign = -1; sign <= 1; sign += 2) {
          int c = col + DIRS[d][0] * sign;
          int r = row + DIRS[d][1] * sign;
          while (c >= 0 && c < CONNECT4_COLS && r >= 0 && r < CONNECT4_ROWS &&
                 cells[r * CONNECT4_COLS + c] == player) {
            run++;
            c += DIRS[d][0] * sign;
            r += DIRS[d][1] * sign;
          }
        }
        if (run >= CONNECT4_NEED) {
          return true;
        }
      }
      return false;
    }

    bool boardFull() const {
      for (int col = 0; col < CONNECT4_COLS; col++) {
        if (columnOpen(col)) {
          return false;
        }
      }
      return true;
    }

    // Records the four cells of a completed line so the win can be ringed.
    void captureWinCells(int8_t player) {
      winCellCount = 0;
      for (int w = 0; w < windowCount; w++) {
        int same = 0;
        for (int i = 0; i < CONNECT4_NEED; i++) {
          if (cells[windows[w][i]] == player) {
            same++;
          }
        }
        if (same == CONNECT4_NEED) {
          for (int i = 0; i < CONNECT4_NEED; i++) {
            winCells[i] = (int8_t)windows[w][i];
          }
          winCellCount = CONNECT4_NEED;
          return;
        }
      }
    }

    // ---- CPU -----------------------------------------------------------

    // Heuristic from `player`'s point of view: every line of four that only one
    // colour occupies is worth more the fuller it is, and the centre file is worth
    // a little on its own because more lines pass through it.
    int evaluateFor(int8_t player) const {
      int8_t foe = opponentOf(player);
      int total = 0;
      for (int w = 0; w < windowCount; w++) {
        int mine = 0;
        int theirs = 0;
        for (int i = 0; i < CONNECT4_NEED; i++) {
          int8_t v = cells[windows[w][i]];
          if (v == player) {
            mine++;
          } else if (v == foe) {
            theirs++;
          }
        }
        if (theirs == 0) {
          total += CONNECT4_WINDOW_WEIGHT[mine < 4 ? mine : 3];
        } else if (mine == 0) {
          total -= CONNECT4_WINDOW_WEIGHT[theirs < 4 ? theirs : 3];
        }
      }
      int mid = CONNECT4_COLS / 2;
      for (int row = 0; row < CONNECT4_ROWS; row++) {
        int8_t v = cells[row * CONNECT4_COLS + mid];
        if (v == player) {
          total += CONNECT4_CENTER_WEIGHT;
        } else if (v == foe) {
          total -= CONNECT4_CENTER_WEIGHT;
        }
      }
      return total;
    }

    // Centre-out ordering, which makes alpha-beta cut far more of the tree.
    static int orderedColumn(int i) {
      static const int8_t ORDER[CONNECT4_COLS] = { 3, 2, 4, 1, 5, 0, 6 };
      return ORDER[i];
    }

    int negamax(int depth, int alpha, int beta, int8_t player) {
      bool anyMove = false;
      int best = -CONNECT4_WIN_SCORE * 2;
      for (int i = 0; i < CONNECT4_COLS; i++) {
        int col = orderedColumn(i);
        if (!columnOpen(col)) {
          continue;
        }
        anyMove = true;
        int row = dropInto(col, player);
        if (makesFour(col, row, player)) {
          undoDrop(col);
          // Nothing beats winning, and a sooner win scores higher.
          return CONNECT4_WIN_SCORE + depth;
        }
        int value = (depth <= 1) ? evaluateFor(player)
                                 : -negamax(depth - 1, -beta, -alpha, opponentOf(player));
        undoDrop(col);
        if (value > best) {
          best = value;
        }
        if (best > alpha) {
          alpha = best;
        }
        if (alpha >= beta) {
          break;
        }
      }
      return anyMove ? best : 0;  // full board is a draw
    }

    int aiChooseColumn(int8_t player, int depth) {
      int scores[CONNECT4_COLS];
      int bestScore = -CONNECT4_WIN_SCORE * 4;
      for (int col = 0; col < CONNECT4_COLS; col++) {
        scores[col] = -CONNECT4_WIN_SCORE * 4;
      }
      for (int i = 0; i < CONNECT4_COLS; i++) {
        int col = orderedColumn(i);
        if (!columnOpen(col)) {
          continue;
        }
        int row = dropInto(col, player);
        int value;
        if (makesFour(col, row, player)) {
          value = CONNECT4_WIN_SCORE + depth;
        } else if (depth <= 1) {
          value = evaluateFor(player);
        } else {
          value = -negamax(depth - 1, -CONNECT4_WIN_SCORE * 2,
                           CONNECT4_WIN_SCORE * 2, opponentOf(player));
        }
        undoDrop(col);
        scores[col] = value;
        if (value > bestScore) {
          bestScore = value;
        }
      }
      // Pick at random between equally good columns, so the CPU does not replay
      // the same game every time.
      int tied[CONNECT4_COLS];
      int tiedCount = 0;
      for (int col = 0; col < CONNECT4_COLS; col++) {
        if (columnOpen(col) && scores[col] == bestScore) {
          tied[tiedCount++] = col;
        }
      }
      if (tiedCount == 0) {
        return -1;
      }
      return tied[random(0, tiedCount)];
    }

    // ---- Mode select ---------------------------------------------------

    void drawModeSelect() {
      _tft->fillScreen(colorBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorBg());
      _tft->drawString("FOUR IN A ROW", SCREENWIDTH / 2, 30, 4);
      _tft->setTextColor(colorBoard(), colorBg());
      _tft->drawString("Line up four to win", SCREENWIDTH / 2, 58, 2);

      // Show what the two sides look like, on a scrap of the green board.
      int16_t stripY = 76;
      _tft->fillRoundRect(28, stripY, SCREENWIDTH - 56, 40, 8, colorBoard());
      drawTotoroPiece(96, stripY + 20);
      drawSootPiece(144, stripY + 20);

      drawModeButton(CONNECT4_BTN1_Y, "1 PLAYER", "vs Soot - easy");
      drawModeButton(CONNECT4_BTN2_Y, "1 PLAYER", "vs Soot - hard");
      drawModeButton(CONNECT4_BTN3_Y, "2 PLAYERS", "hot seat");
      _tft->setTextDatum(TL_DATUM);
    }

    void drawModeButton(int16_t y, const char *title, const char *subtitle) {
      _tft->fillRoundRect(CONNECT4_BTN_X, y, CONNECT4_BTN_W, CONNECT4_BTN_H, 10,
                          colorPanelLight());
      _tft->drawRoundRect(CONNECT4_BTN_X, y, CONNECT4_BTN_W, CONNECT4_BTN_H, 10,
                          colorCream());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorPanelLight());
      _tft->drawString(title, SCREENWIDTH / 2, y + 16, 4);
      _tft->drawString(subtitle, SCREENWIDTH / 2, y + 35, 2);
    }

    void handleModeSelectTouch() {
      uint16_t x = 0;
      uint16_t y = 0;
      if (!getTouchPoint(_tft, &x, &y)) {
        return;
      }
      if (x < CONNECT4_BTN_X || x > CONNECT4_BTN_X + CONNECT4_BTN_W) {
        return;
      }
      if (y >= CONNECT4_BTN1_Y && y <= CONNECT4_BTN1_Y + CONNECT4_BTN_H) {
        numPlayers = 1;
        aiDepth = CONNECT4_DEPTH_EASY;
        startGame();
      } else if (y >= CONNECT4_BTN2_Y && y <= CONNECT4_BTN2_Y + CONNECT4_BTN_H) {
        numPlayers = 1;
        aiDepth = CONNECT4_DEPTH_HARD;
        startGame();
      } else if (y >= CONNECT4_BTN3_Y && y <= CONNECT4_BTN3_Y + CONNECT4_BTN_H) {
        numPlayers = 2;
        startGame();
      }
    }

    // ---- Gameplay ------------------------------------------------------

    void startGame() {
      for (int i = 0; i < CONNECT4_CELLS; i++) {
        cells[i] = CONNECT4_EMPTY;
      }
      for (int col = 0; col < CONNECT4_COLS; col++) {
        heights[col] = 0;
      }
      turn = CONNECT4_TOTORO;
      dropping = false;
      aiPending = false;
      lastCol = -1;
      lastRow = -1;
      winCellCount = 0;
      moveCount = 0;
      state = C4_STATE_PLAYING;
      gameStartMs = millis();

      _tft->fillScreen(colorBg());
      drawBoard();
      drawTurnBanner();
      drawStatusPill("Tap a column to drop");
      addSound(NOTE_C5, noteDurationMs(16, 800));
    }

    int columnAt(uint16_t x, uint16_t y) const {
      if (y + CONNECT4_TAP_PAD < CONNECT4_BOARD_Y ||
          y > CONNECT4_BOARD_Y + CONNECT4_BOARD_H + CONNECT4_TAP_PAD) {
        return -1;
      }
      if (x < CONNECT4_BOARD_X || x >= CONNECT4_BOARD_X + CONNECT4_BOARD_W) {
        return -1;
      }
      return (x - CONNECT4_BOARD_X) / CONNECT4_CELL;
    }

    void handlePlayTouch(uint16_t x, uint16_t y) {
      // The turn order already keeps this from being reached on the CPU's turn,
      // stated here so a reordering of update() cannot quietly let a tap through.
      if (numPlayers == 1 && turn != CONNECT4_TOTORO) {
        return;
      }
      int col = columnAt(x, y);
      if (col < 0) {
        return;
      }
      if (!columnOpen(col)) {
        addSound(NOTE_E4, noteDurationMs(32, 600));
        return;
      }
      startDrop(col, turn);
    }

    // ---- The falling piece ---------------------------------------------

    void startDrop(int col, int8_t player) {
      dropCol = (int8_t)col;
      dropPlayer = player;
      dropTargetRow = (int8_t)landingRow(col);
      dropRow = 0;
      fallTick = 0;
      dropping = true;
      drawPieceAt(dropCol, dropRow, dropPlayer, false);
      addSound(NOTE_G5, noteDurationMs(32, 500));
      if (dropRow >= dropTargetRow) {
        commitDrop();
      }
    }

    void advanceDrop() {
      if (++fallTick < CONNECT4_FALL_TICKS) {
        return;
      }
      fallTick = 0;
      drawEmptyCell(dropCol, dropRow);
      dropRow++;
      if (dropRow >= dropTargetRow) {
        commitDrop();
        return;
      }
      drawPieceAt(dropCol, dropRow, dropPlayer, false);
    }

    void commitDrop() {
      dropping = false;
      int row = dropInto(dropCol, dropPlayer);
      moveCount++;

      // Move the "latest move" ring off the previous piece and onto this one. The
      // new position has to be recorded first, because drawCell() decides whether
      // to ring a piece by comparing against it.
      int8_t prevCol = lastCol;
      int8_t prevRow = lastRow;
      lastCol = dropCol;
      lastRow = (int8_t)row;
      if (prevCol >= 0) {
        drawCell(prevCol, prevRow);
      }
      drawCell(dropCol, row);
      addSound(NOTE_C4, noteDurationMs(24, 700));

      if (makesFour(dropCol, row, dropPlayer)) {
        endGame(dropPlayer);
        return;
      }
      if (boardFull()) {
        endDraw();
        return;
      }

      turn = opponentOf(dropPlayer);
      drawTurnBanner();
      if (numPlayers == 1 && turn == CONNECT4_SOOT) {
        aiPending = true;
      }
    }

    // ---- Drawing -------------------------------------------------------

    void drawBoard() {
      _tft->fillRoundRect(CONNECT4_BOARD_X - 5, CONNECT4_BOARD_Y - 5,
                          CONNECT4_BOARD_W + 10, CONNECT4_BOARD_H + 10, 12,
                          colorBoardRim());
      _tft->fillRoundRect(CONNECT4_BOARD_X, CONNECT4_BOARD_Y,
                          CONNECT4_BOARD_W, CONNECT4_BOARD_H, 8, colorBoard());
      for (int row = 0; row < CONNECT4_ROWS; row++) {
        for (int col = 0; col < CONNECT4_COLS; col++) {
          drawCell(col, row);
        }
      }
    }

    void drawEmptyCell(int col, int row) {
      _tft->fillRect(CONNECT4_BOARD_X + col * CONNECT4_CELL,
                     CONNECT4_BOARD_Y + row * CONNECT4_CELL,
                     CONNECT4_CELL, CONNECT4_CELL, colorBoard());
      _tft->fillCircle(cellCX(col), cellCY(row), CONNECT4_HOLE_R, colorHole());
    }

    void drawCell(int col, int row) {
      int8_t who = cells[row * CONNECT4_COLS + col];
      if (who == CONNECT4_EMPTY) {
        drawEmptyCell(col, row);
        return;
      }
      drawPieceAt(col, row, who, col == lastCol && row == lastRow);
    }

    void drawPieceAt(int col, int row, int8_t who, bool marked) {
      drawEmptyCell(col, row);
      int16_t cx = cellCX(col);
      int16_t cy = cellCY(row);
      if (who == CONNECT4_TOTORO) {
        drawTotoroPiece(cx, cy);
      } else {
        drawSootPiece(cx, cy);
      }
      if (marked) {
        // The set's little gold seat ring, doubling as "this was the last move".
        _tft->drawCircle(cx, cy, CONNECT4_HOLE_R, colorGold());
      }
    }

    // Small white Totoro: round body, two upright ears, eyes and a little smile.
    // At 26px across the chest chevrons have to go - tried as three ticks, they
    // read as teeth - so the ears carry the silhouette and the smile the charm.
    // The ear tips sit just outside the hole circle, which is how the figures in
    // the real set stand proud of the board.
    void drawTotoroPiece(int16_t cx, int16_t cy) {
      _tft->fillTriangle(cx - 8, cy - 8, cx - 7, cy - 15, cx - 2, cy - 7, colorCream());
      _tft->fillTriangle(cx + 8, cy - 8, cx + 7, cy - 15, cx + 2, cy - 7, colorCream());
      _tft->fillCircle(cx, cy + 1, CONNECT4_PIECE_R - 1, colorCream());
      _tft->drawCircle(cx, cy + 1, CONNECT4_PIECE_R - 1, colorTotoroLine());
      _tft->fillCircle(cx - 4, cy - 2, 2, colorPupil());
      _tft->fillCircle(cx + 4, cy - 2, 2, colorPupil());
      _tft->drawLine(cx - 3, cy + 3, cx, cy + 5, colorTotoroLine());
      _tft->drawLine(cx, cy + 5, cx + 3, cy + 3, colorTotoroLine());
    }

    // Soot sprite: black fuzzball with big eyes. The fuzz is eight short spokes,
    // which at this size reads better than trying to draw a ragged outline.
    void drawSootPiece(int16_t cx, int16_t cy) {
      static const int8_t SPOKE[8][2] = {
        { 10, 0 }, { 7, 7 }, { 0, 10 }, { -7, 7 },
        { -10, 0 }, { -7, -7 }, { 0, -10 }, { 7, -7 }
      };
      for (int i = 0; i < 8; i++) {
        int16_t x0 = cx + SPOKE[i][0];
        int16_t y0 = cy + SPOKE[i][1];
        int16_t x1 = cx + SPOKE[i][0] * 14 / 10;
        int16_t y1 = cy + SPOKE[i][1] * 14 / 10;
        _tft->drawLine(x0, y0, x1, y1, colorSootFuzz());
      }
      _tft->fillCircle(cx, cy, CONNECT4_PIECE_R - 2, colorSoot());
      _tft->fillCircle(cx - 4, cy - 1, 3, colorEye());
      _tft->fillCircle(cx + 4, cy - 1, 3, colorEye());
      _tft->drawPixel(cx - 4, cy - 1, colorPupil());
      _tft->drawPixel(cx + 4, cy - 1, colorPupil());
    }

    // ---- Banners -------------------------------------------------------

    // Whose turn, with that side's piece drawn beside the words.
    void drawTurnBanner() {
      int16_t y = CONNECT4_TURN_CY - CONNECT4_TURN_H / 2;
      _tft->fillRoundRect(12, y, SCREENWIDTH - 24, CONNECT4_TURN_H, 8, colorPanel());
      const char *text;
      if (numPlayers == 1) {
        text = (turn == CONNECT4_TOTORO) ? "Your turn" : "Soot is thinking...";
      } else {
        text = (turn == CONNECT4_TOTORO) ? "Totoro's turn" : "Soot's turn";
      }
      _tft->setTextDatum(ML_DATUM);
      _tft->setTextColor(colorCream(), colorPanel());
      _tft->drawString(text, 52, CONNECT4_TURN_CY, 2);
      if (turn == CONNECT4_TOTORO) {
        drawTotoroPiece(32, CONNECT4_TURN_CY);
      } else {
        drawSootPiece(32, CONNECT4_TURN_CY);
      }
      _tft->setTextDatum(TL_DATUM);
    }

    void drawStatusPill(const char *text) {
      _tft->fillRoundRect(12, CONNECT4_STATUS_CY - CONNECT4_STATUS_H / 2,
                          SCREENWIDTH - 24, CONNECT4_STATUS_H, 8, colorPanel());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorBoard(), colorPanel());
      _tft->drawString(text, SCREENWIDTH / 2, CONNECT4_STATUS_CY, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawResult(const char *msg) {
      _tft->fillRoundRect(12, CONNECT4_RESULT_Y, SCREENWIDTH - 24,
                          CONNECT4_RESULT_H, 8, colorPanel());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorGold(), colorPanel());
      _tft->drawString(msg, SCREENWIDTH / 2, CONNECT4_RESULT_Y + 14, 2);
      _tft->setTextColor(colorCream(), colorPanel());
      _tft->drawString("Tap to play again", SCREENWIDTH / 2,
                       CONNECT4_RESULT_Y + 31, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void ringWinningLine() {
      for (int i = 0; i < winCellCount; i++) {
        int col = winCells[i] % CONNECT4_COLS;
        int row = winCells[i] / CONNECT4_COLS;
        _tft->drawCircle(cellCX(col), cellCY(row), CONNECT4_HOLE_R, colorGold());
        _tft->drawCircle(cellCX(col), cellCY(row), CONNECT4_HOLE_R - 1, colorGold());
      }
    }

    uint16_t sessionSeconds() const {
      if (gameStartMs == 0) {
        return 0;
      }
      return (uint16_t)((millis() - gameStartMs) / 1000);
    }

    void endGame(int8_t winner) {
      state = C4_STATE_WIN;
      aiPending = false;
      suppressTouchUntilMs = millis() + 700;
      captureWinCells(winner);
      ringWinningLine();

      const char *msg;
      if (numPlayers == 1) {
        bool playerWon = (winner == CONNECT4_TOTORO);
        msg = playerWon ? "FOUR IN A ROW - YOU WIN!" : "SOOT GOT FOUR";
        GameOutcome outcome = playerWon ? GAME_RESULT_WIN : GAME_RESULT_LOSS;
        GameResult::report(outcome, playerWon ? CONNECT4_WIN_COINS : 0);
        mlLogGameEnd(SCENE_CONNECT_FOUR, outcome, moveCount, aiDepth, sessionSeconds());
      } else {
        msg = (winner == CONNECT4_TOTORO) ? "TOTORO WINS!" : "SOOT WINS!";
        GameResult::report(GAME_RESULT_NEUTRAL, 0);
        mlLogGameEnd(SCENE_CONNECT_FOUR, GAME_RESULT_NEUTRAL, moveCount, 0,
                     sessionSeconds());
      }
      drawTurnBannerCleared();
      drawResult(msg);
      addSound(NOTE_E5, noteDurationMs(10, 900));
      addSound(NOTE_G5, noteDurationMs(10, 900));
      addSound(NOTE_C6, noteDurationMs(10, 900));
    }

    void endDraw() {
      state = C4_STATE_DRAW;
      aiPending = false;
      suppressTouchUntilMs = millis() + 700;
      if (numPlayers == 1) {
        GameResult::report(GAME_RESULT_LOSS, 0);
        mlLogGameEnd(SCENE_CONNECT_FOUR, GAME_RESULT_LOSS, moveCount, aiDepth,
                     sessionSeconds());
      } else {
        GameResult::report(GAME_RESULT_NEUTRAL, 0);
        mlLogGameEnd(SCENE_CONNECT_FOUR, GAME_RESULT_NEUTRAL, moveCount, 0,
                     sessionSeconds());
      }
      drawTurnBannerCleared();
      drawResult("BOARD FULL - DRAW");
      addSound(NOTE_E4, noteDurationMs(8, 700));
      addSound(NOTE_E4, noteDurationMs(8, 700));
    }

    // The turn banner is meaningless once the round is over, and leaving
    // "Soot is thinking..." on screen under a verdict reads as a hang.
    void drawTurnBannerCleared() {
      int16_t y = CONNECT4_TURN_CY - CONNECT4_TURN_H / 2;
      _tft->fillRoundRect(12, y, SCREENWIDTH - 24, CONNECT4_TURN_H, 8, colorPanel());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorPanel());
      _tft->drawString("FOUR IN A ROW", SCREENWIDTH / 2, CONNECT4_TURN_CY, 2);
      _tft->setTextDatum(TL_DATUM);
    }
};

#endif
