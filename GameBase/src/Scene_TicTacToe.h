#ifndef _SCENE_TICTACTOE_H_
#define _SCENE_TICTACTOE_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "GameResult.h"
#include "Input.h"
#include "ml/MLGameHooks.h"
#include "SpriteSheet.h"
#include "TouchInput.h"
#include "image_grass_tile.h"
#include "sprite_ttt_grid.h"

// Coins awarded for a decisive Tic-Tac-Toe result (player win in 1P, or any
// winner in 2P). A draw is a consolation loss.
#define TTT_WIN_COINS 6
#include "sprite_ttt_tokens.h"

// Grass board rendered from a tiny 50x50 grass tile repeated across the whole
// screen (setBackgroundTile) - this replaces the old ~150 KB full-screen
// background. The wooden "#" grid is a single transparent overlay Avatar, and
// the Studio-Ghibli markers (Cat Bus head = X, Mei head = O) are token Avatars
// composited into the grid cells by renderScene() (see AGENTS.md). A
// mode-select screen on entry picks 1P (vs CPU) or 2P (hot-seat).

#define TTT_EMPTY 0
#define TTT_X 1   // Cat Bus  -> token region 1
#define TTT_O 2   // Mei      -> token region 0

// Per-move chance (%) the AI "fumbles" a key rule: independently rolled for
// taking its own win and for blocking the player, so a sharp player can now win
// instead of only ever drawing. 0 = perfect heuristic, 100 = ignores win/block.
#define TTT_AI_MISTAKE_CHANCE 35

#define TTT_TOKEN_REGION_MEI 0
#define TTT_TOKEN_REGION_CATBUS 1
#define TTT_TOKEN_SIZE 46

// Board square: the grid overlay is drawn at (X,Y) sized SIZE x SIZE.
#define TTT_BOARD_X 15
#define TTT_BOARD_Y 70
#define TTT_BOARD_SIZE 210

// Cell centers and internal stick lines, measured from the grid art so tokens
// land inside the (slightly uneven) hand-drawn stick cells.
static const int16_t TTT_COL_CX[3] = { 47, 119, 192 };
static const int16_t TTT_ROW_CY[3] = { 101, 168, 242 };
#define TTT_VLINE_A 79
#define TTT_VLINE_B 159
#define TTT_HLINE_A 133
#define TTT_HLINE_B 204
#define TTT_BOARD_MIN_X TTT_BOARD_X
#define TTT_BOARD_MAX_X (TTT_BOARD_X + TTT_BOARD_SIZE)
#define TTT_BOARD_MIN_Y TTT_BOARD_Y
#define TTT_BOARD_MAX_Y (TTT_BOARD_Y + TTT_BOARD_SIZE)

// Status pill sits in the grass strip below the board.
#define TTT_STATUS_CY 300
#define TTT_STATUS_H 26
// The result pill carries a second line, so it reaches further up the strip.
#define TTT_RESULT_Y 276
#define TTT_RESULT_H 42

// Mode-select buttons.
#define TTT_BTN_W 184
#define TTT_BTN_H 50
#define TTT_BTN_X ((SCREENWIDTH - TTT_BTN_W) / 2)
#define TTT_BTN1_Y 150
#define TTT_BTN2_Y 214

enum TicTacToeState {
  TTT_STATE_MODE_SELECT,
  TTT_STATE_PLAYING,
  TTT_STATE_WIN,
  TTT_STATE_DRAW
};

class Scene_TicTacToe : public GameScene {
  public:
    Scene_TicTacToe(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();

      // Home ends the visit and collects the payout for every round played; a
      // tap after a result goes back to the mode select for another game.
      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = gameExitSceneIndex();
        return;
      }

      bool touchDown = isTouching && !wasTouching && millis() > suppressTouchUntilMs;

      if (state == TTT_STATE_MODE_SELECT) {
        if (touchDown) {
          handleModeSelectTouch();
        }
        wasTouching = isTouching;
        return;
      }

      if (state == TTT_STATE_WIN || state == TTT_STATE_DRAW) {
        if (touchDown) {
          state = TTT_STATE_MODE_SELECT;
          drawModeSelect();
        }
        wasTouching = isTouching;
        return;
      }

      if (touchDown) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          handlePlayTouch(touchX, touchY);
        }
      }

      wasTouching = isTouching;
    }

    void render() {
      // Only the PLAYING state animates (tokens dropping in). In MODE_SELECT /
      // WIN / DRAW the screen is drawn directly (menu, winning line, result)
      // and must NOT be touched by renderScene(): the hidden token/grid Avatars
      // still carry their last on-screen positions, so a stray renderScene()
      // would repaint their old footprints with grass and wipe those overlays.
      if (state == TTT_STATE_PLAYING) {
        renderScene();
      }
    }

    void initScene() {
      setBackgroundTile(grass_tile, GRASS_TILE_WIDTH, GRASS_TILE_HEIGHT);

      grid = new Avatar(-300, -300, SPRITE_TTT_GRID_WIDTH, SPRITE_TTT_GRID_HEIGHT,
                        sprite_ttt_grid, sprite_ttt_gridMask);
      grid->setVelocity(0, 0);
      grid->updateInterval = 50;
      appendAvatar(grid);

      SpriteSheet sheet = tokenSheet();
      SpriteSheetRegion region = SpriteSheet::readRegion(sprite_ttt_tokensRegions, TTT_TOKEN_REGION_MEI);
      for (int i = 0; i < 9; i++) {
        tokens[i] = sheet.createAvatar(-100, -100, region);
        appendAvatar(tokens[i]);
      }

      wasTouching = false;
      suppressTouchUntilMs = millis() + 400;
      numPlayers = 1;
      state = TTT_STATE_MODE_SELECT;
      drawModeSelect();
    }

    void destroyScene() {
      grid = NULL;
      for (int i = 0; i < 9; i++) {
        tokens[i] = NULL;
      }
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    Avatar *grid = NULL;
    Avatar *tokens[9];
    int8_t board[9];
    TicTacToeState state = TTT_STATE_MODE_SELECT;
    int8_t winningLine[3];
    int winningLineCount = 0;
    int numPlayers = 1;
    int8_t currentMark = TTT_X;
    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;
    unsigned long gameStartMs = 0;

    uint16_t colorWood() const { return rgb565(150, 105, 55); }
    uint16_t colorWoodDark() const { return rgb565(92, 62, 30); }
    uint16_t colorCream() const { return rgb565(235, 220, 185); }
    uint16_t colorPanel() const { return rgb565(28, 40, 24); }

    SpriteSheet tokenSheet() const {
      return SpriteSheet(sprite_ttt_tokens, sprite_ttt_tokensMask,
                         SPRITE_TTT_TOKENS_WIDTH, SPRITE_TTT_TOKENS_HEIGHT);
    }

    // ---- Mode select ---------------------------------------------------

    void drawModeSelect() {
      grid->setPos(-300, -300);
      hideAllTokens();
      renderFullScreen();

      drawPillText("TIC TAC TOE", SCREENWIDTH / 2, 40, 150, 4, colorWoodDark(), colorCream());
      drawPillText("Choose players", SCREENWIDTH / 2, 74, 120, 2, colorPanel(), colorCream());

      drawButton(TTT_BTN1_Y, "1 PLAYER", "vs Cat Bus CPU");
      drawButton(TTT_BTN2_Y, "2 PLAYERS", "hot seat");

      _tft->setTextDatum(TL_DATUM);
    }

    void drawButton(int16_t y, const char *title, const char *subtitle) {
      _tft->fillRoundRect(TTT_BTN_X, y, TTT_BTN_W, TTT_BTN_H, 10, colorWood());
      _tft->drawRoundRect(TTT_BTN_X, y, TTT_BTN_W, TTT_BTN_H, 10, colorCream());
      _tft->drawRoundRect(TTT_BTN_X + 1, y + 1, TTT_BTN_W - 2, TTT_BTN_H - 2, 9, colorWoodDark());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorCream(), colorWood());
      _tft->drawString(title, SCREENWIDTH / 2, y + 17, 4);
      _tft->drawString(subtitle, SCREENWIDTH / 2, y + 38, 2);
    }

    void drawPillText(const char *text, int16_t cx, int16_t cy, int16_t halfW,
                      int font, uint16_t bg, uint16_t fg) {
      _tft->fillRoundRect(cx - halfW, cy - 14, halfW * 2, 28, 8, bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(fg, bg);
      _tft->drawString(text, cx, cy, font);
    }

    void handleModeSelectTouch() {
      uint16_t x = 0;
      uint16_t y = 0;
      if (!getTouchPoint(_tft, &x, &y)) {
        return;
      }
      if (x < TTT_BTN_X || x > TTT_BTN_X + TTT_BTN_W) {
        return;
      }
      if (y >= TTT_BTN1_Y && y <= TTT_BTN1_Y + TTT_BTN_H) {
        numPlayers = 1;
        startGame();
      } else if (y >= TTT_BTN2_Y && y <= TTT_BTN2_Y + TTT_BTN_H) {
        numPlayers = 2;
        startGame();
      }
    }

    // ---- Gameplay ------------------------------------------------------

    void startGame() {
      for (int i = 0; i < 9; i++) {
        board[i] = TTT_EMPTY;
      }
      winningLineCount = 0;
      currentMark = TTT_X;
      state = TTT_STATE_PLAYING;
      gameStartMs = millis();

      grid->setPos(TTT_BOARD_X, TTT_BOARD_Y);
      grid->requestRedraw();
      hideAllTokens();
      renderFullScreen();
      drawTurnStatus();
      addSound(NOTE_C5, noteDurationMs(16, 800));
    }

    void handlePlayTouch(uint16_t x, uint16_t y) {
      int cell = cellAt(x, y);
      if (cell < 0 || board[cell] != TTT_EMPTY) {
        return;
      }

      placeMove(cell, currentMark);
      addSound(currentMark == TTT_X ? NOTE_C5 : NOTE_E4, noteDurationMs(16, 800));

      if (finishIfOver()) {
        return;
      }

      if (numPlayers == 1) {
        int aiCell = chooseAiMove();
        if (aiCell >= 0) {
          placeMove(aiCell, TTT_O);
          addSound(NOTE_E4, noteDurationMs(16, 800));
          if (finishIfOver()) {
            return;
          }
        }
      } else {
        currentMark = (currentMark == TTT_X) ? TTT_O : TTT_X;
      }

      drawTurnStatus();
    }

    bool finishIfOver() {
      int winner = checkWinner();
      if (winner != TTT_EMPTY) {
        endGame(winner);
        return true;
      }
      if (isBoardFull()) {
        endDraw();
        return true;
      }
      return false;
    }

    void hideAllTokens() {
      for (int i = 0; i < 9; i++) {
        tokens[i]->setPos(-100, -100);
      }
    }

    void placeMove(int cell, int8_t mark) {
      board[cell] = mark;
      int region = (mark == TTT_X) ? TTT_TOKEN_REGION_CATBUS : TTT_TOKEN_REGION_MEI;
      tokenSheet().applyRegion(tokens[cell], SpriteSheet::readRegion(sprite_ttt_tokensRegions, region));
      int col = cell % 3;
      int row = cell / 3;
      tokens[cell]->setPos(TTT_COL_CX[col] - TTT_TOKEN_SIZE / 2, TTT_ROW_CY[row] - TTT_TOKEN_SIZE / 2);
      tokens[cell]->requestRedraw();
      requestRender();
    }

    int cellAt(uint16_t x, uint16_t y) const {
      if (x < TTT_BOARD_MIN_X || x > TTT_BOARD_MAX_X ||
          y < TTT_BOARD_MIN_Y || y > TTT_BOARD_MAX_Y) {
        return -1;
      }
      int col = (x < TTT_VLINE_A) ? 0 : (x < TTT_VLINE_B ? 1 : 2);
      int row = (y < TTT_HLINE_A) ? 0 : (y < TTT_HLINE_B ? 1 : 2);
      return row * 3 + col;
    }

    bool isBoardFull() const {
      for (int i = 0; i < 9; i++) {
        if (board[i] == TTT_EMPTY) {
          return false;
        }
      }
      return true;
    }

    int checkWinner() {
      static const int lines[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
        {0, 4, 8}, {2, 4, 6}
      };
      for (int i = 0; i < 8; i++) {
        int a = lines[i][0];
        int b = lines[i][1];
        int c = lines[i][2];
        if (board[a] != TTT_EMPTY && board[a] == board[b] && board[b] == board[c]) {
          winningLineCount = 3;
          winningLine[0] = a;
          winningLine[1] = b;
          winningLine[2] = c;
          return board[a];
        }
      }
      winningLineCount = 0;
      return TTT_EMPTY;
    }

    int chooseAiMove() {
      // Independently decide whether to bother taking a win / making a block
      // this move. Skipping either is what lets a good player actually win.
      bool goForWin = (random(0, 100) >= TTT_AI_MISTAKE_CHANCE);
      bool doBlock = (random(0, 100) >= TTT_AI_MISTAKE_CHANCE);

      if (goForWin) {
        for (int i = 0; i < 9; i++) {
          if (board[i] != TTT_EMPTY) continue;
          board[i] = TTT_O;
          if (checkWinner() == TTT_O) { board[i] = TTT_EMPTY; return i; }
          board[i] = TTT_EMPTY;
        }
      }
      if (doBlock) {
        for (int i = 0; i < 9; i++) {
          if (board[i] != TTT_EMPTY) continue;
          board[i] = TTT_X;
          if (checkWinner() == TTT_X) { board[i] = TTT_EMPTY; return i; }
          board[i] = TTT_EMPTY;
        }
      }
      if (board[4] == TTT_EMPTY) {
        return 4;
      }
      int corners[4] = {0, 2, 6, 8};
      int openCorners[4];
      int openCornerCount = 0;
      for (int i = 0; i < 4; i++) {
        if (board[corners[i]] == TTT_EMPTY) {
          openCorners[openCornerCount++] = corners[i];
        }
      }
      if (openCornerCount > 0) {
        return openCorners[random(0, openCornerCount)];
      }
      int open[9];
      int openCount = 0;
      for (int i = 0; i < 9; i++) {
        if (board[i] == TTT_EMPTY) {
          open[openCount++] = i;
        }
      }
      if (openCount == 0) {
        return -1;
      }
      return open[random(0, openCount)];
    }

    int boardMoveCount() const {
      int count = 0;
      for (int i = 0; i < 9; i++) {
        if (board[i] != TTT_EMPTY) {
          count++;
        }
      }
      return count;
    }

    uint16_t gameSessionSeconds() const {
      if (gameStartMs == 0) {
        return 0;
      }
      return (uint16_t)((millis() - gameStartMs) / 1000);
    }

    void endGame(int winner) {
      // Flush the final placed token(s) before we stop calling renderScene().
      renderScene();
      state = TTT_STATE_WIN;
      drawWinningLine();
      const char *msg;
      if (numPlayers == 1) {
        msg = (winner == TTT_X) ? "YOU WIN!" : "CAT BUS LOSES";
      } else {
        msg = (winner == TTT_X) ? "CAT BUS WINS!" : "MEI WINS!";
      }
      // Reward the pet: in 1P the human is X (win iff X wins); in 2P any decisive
      // result counts as a win for the play session.
      bool playerWon = (numPlayers != 1) || (winner == TTT_X);
      GameOutcome outcome = playerWon ? GAME_RESULT_WIN : GAME_RESULT_LOSS;
      GameResult::report(outcome, playerWon ? TTT_WIN_COINS : 0);
      mlLogGameEnd(SCENE_TIC_TAC_TOE, outcome, boardMoveCount(), numPlayers, gameSessionSeconds());
      drawResult(msg);
      addSound(NOTE_G5, noteDurationMs(8, 800));
      addSound(NOTE_C6, noteDurationMs(8, 800));
    }

    void endDraw() {
      // Flush the final placed token before we stop calling renderScene().
      renderScene();
      state = TTT_STATE_DRAW;
      GameResult::report(GAME_RESULT_LOSS, 0);  // consolation reward for a draw
      mlLogGameEnd(SCENE_TIC_TAC_TOE, GAME_RESULT_LOSS, boardMoveCount(), numPlayers, gameSessionSeconds());
      drawResult("DRAW");
      addSound(NOTE_E4, noteDurationMs(8, 700));
      addSound(NOTE_E4, noteDurationMs(8, 700));
    }

    void drawWinningLine() {
      if (winningLineCount != 3) {
        return;
      }
      int a = winningLine[0];
      int c = winningLine[2];
      int16_t x0 = TTT_COL_CX[a % 3];
      int16_t y0 = TTT_ROW_CY[a / 3];
      int16_t x1 = TTT_COL_CX[c % 3];
      int16_t y1 = TTT_ROW_CY[c / 3];
      uint16_t highlight = rgb565(255, 230, 80);
      for (int d = -2; d <= 2; d++) {
        _tft->drawLine(x0 + d, y0, x1 + d, y1, highlight);
        _tft->drawLine(x0, y0 + d, x1, y1 + d, highlight);
      }
    }

    void drawTurnStatus() {
      const char *text;
      if (numPlayers == 1) {
        text = "Your turn  (Cat Bus)";
      } else {
        text = (currentMark == TTT_X) ? "Cat Bus's turn" : "Mei's turn";
      }
      drawStatusPill(text, colorWoodDark(), colorCream());
    }

    // Taller than the turn pill so the verdict can carry the replay hint: from
    // here a tap goes back to the mode select and Home finishes the visit.
    void drawResult(const char *msg) {
      uint16_t bg = rgb565(60, 40, 20);
      _tft->fillRoundRect(SCREENWIDTH / 2 - 108, TTT_RESULT_Y, 216, TTT_RESULT_H, 8, bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(255, 235, 120), bg);
      _tft->drawString(msg, SCREENWIDTH / 2, TTT_RESULT_Y + 13, 2);
      _tft->drawString("Tap to replay", SCREENWIDTH / 2, TTT_RESULT_Y + 30, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawStatusPill(const char *text, uint16_t bg, uint16_t fg) {
      _tft->fillRoundRect(SCREENWIDTH / 2 - 108, TTT_STATUS_CY - TTT_STATUS_H / 2,
                          216, TTT_STATUS_H, 8, bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(fg, bg);
      _tft->drawString(text, SCREENWIDTH / 2, TTT_STATUS_CY, 2);
      _tft->setTextDatum(TL_DATUM);
    }
};

#endif
