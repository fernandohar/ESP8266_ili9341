#ifndef _SCENE_TICTACTOE_H_
#define _SCENE_TICTACTOE_H_

#include <Arduino.h>
#include "GameScene.h"
#include "Input.h"
#include "TouchInput.h"

// Placeholder tic-tac-toe scene. Swap drawBoard()/drawMark() for sprite assets
// when custom graphics are ready (see AGENTS.md asset pipeline).

#define TTT_CELL_SIZE 70
#define TTT_CELL_GAP 6
#define TTT_BOARD_W (TTT_CELL_SIZE * 3 + TTT_CELL_GAP * 2)
#define TTT_BOARD_X ((SCREENWIDTH - TTT_BOARD_W) / 2)
#define TTT_BOARD_Y 72

#define TTT_EMPTY 0
#define TTT_X 1
#define TTT_O 2

enum TicTacToeState {
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

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = 0;
        return;
      }

      if (state == TTT_STATE_WIN || state == TTT_STATE_DRAW) {
        if (isTouching && !wasTouching) {
          resetGame();
        }
        wasTouching = isTouching;
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          int cell = cellAt(touchX, touchY);
          if (cell >= 0 && board[cell] == TTT_EMPTY) {
            placeMove(cell, TTT_X);
            addSound(NOTE_C5, noteDurationMs(16, 800));

            int winner = checkWinner();
            if (winner != TTT_EMPTY) {
              endGame(winner);
            } else if (isBoardFull()) {
              endDraw();
            } else {
              int aiCell = chooseAiMove();
              if (aiCell >= 0) {
                placeMove(aiCell, TTT_O);
                addSound(NOTE_E4, noteDurationMs(16, 800));

                winner = checkWinner();
                if (winner != TTT_EMPTY) {
                  endGame(winner);
                } else if (isBoardFull()) {
                  endDraw();
                }
              }
            }
          }
        }
      }

      wasTouching = isTouching;
    }

    void render() {}

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 400;
      resetGame();
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    int8_t board[9];
    TicTacToeState state = TTT_STATE_PLAYING;
    int8_t winningLine[3];
    int winningLineCount = 0;
    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;

    uint16_t colorBg() const { return rgb565(24, 28, 38); }
    uint16_t colorGrid() const { return rgb565(90, 100, 120); }
    uint16_t colorCell() const { return rgb565(40, 46, 58); }
    uint16_t colorX() const { return rgb565(80, 180, 255); }
    uint16_t colorO() const { return rgb565(255, 140, 60); }
    uint16_t colorDim() const { return rgb565(150, 160, 175); }

    void resetGame() {
      for (int i = 0; i < 9; i++) {
        board[i] = TTT_EMPTY;
      }
      state = TTT_STATE_PLAYING;
      winningLineCount = 0;
      drawScreen();
    }

    int16_t cellOriginX(int cell) const {
      int col = cell % 3;
      return TTT_BOARD_X + col * (TTT_CELL_SIZE + TTT_CELL_GAP);
    }

    int16_t cellOriginY(int cell) const {
      int row = cell / 3;
      return TTT_BOARD_Y + row * (TTT_CELL_SIZE + TTT_CELL_GAP);
    }

    int cellAt(uint16_t x, uint16_t y) const {
      for (int i = 0; i < 9; i++) {
        int16_t cx = cellOriginX(i);
        int16_t cy = cellOriginY(i);
        if (x >= cx && x < cx + TTT_CELL_SIZE && y >= cy && y < cy + TTT_CELL_SIZE) {
          return i;
        }
      }
      return -1;
    }

    void placeMove(int cell, int8_t mark) {
      board[cell] = mark;
      drawCell(cell);
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
      for (int i = 0; i < 9; i++) {
        if (board[i] != TTT_EMPTY) {
          continue;
        }
        board[i] = TTT_O;
        if (checkWinner() == TTT_O) {
          board[i] = TTT_EMPTY;
          return i;
        }
        board[i] = TTT_EMPTY;
      }

      for (int i = 0; i < 9; i++) {
        if (board[i] != TTT_EMPTY) {
          continue;
        }
        board[i] = TTT_X;
        if (checkWinner() == TTT_X) {
          board[i] = TTT_EMPTY;
          return i;
        }
        board[i] = TTT_EMPTY;
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

    void endGame(int winner) {
      state = TTT_STATE_WIN;
      drawWinningLine();
      drawStatus(winner == TTT_X ? "YOU WIN!" : "YOU LOSE");
      drawFooter("Home = Back");
      addSound(NOTE_G5, noteDurationMs(8, 800));
      addSound(NOTE_C6, noteDurationMs(8, 800));
    }

    void endDraw() {
      state = TTT_STATE_DRAW;
      drawStatus("DRAW");
      drawFooter("Home = Back");
      addSound(NOTE_E4, noteDurationMs(8, 700));
      addSound(NOTE_E4, noteDurationMs(8, 700));
    }

    void drawScreen() {
      uint16_t bg = colorBg();
      setBackgroundColor(bg);
      _tft->fillScreen(bg);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, bg);
      _tft->drawString("Tic Tac Toe", SCREENWIDTH / 2, 28, 4);

      _tft->setTextColor(colorDim(), bg);
      _tft->drawString("You: X    CPU: O", SCREENWIDTH / 2, 52, 2);

      drawBoard();
      drawStatus("YOUR TURN");
      drawFooter("Home = Back");

      _tft->setTextDatum(TL_DATUM);
    }

    void drawBoard() {
      for (int i = 0; i < 9; i++) {
        drawCell(i);
      }
    }

    void drawCell(int cell) {
      int16_t x = cellOriginX(cell);
      int16_t y = cellOriginY(cell);
      _tft->fillRoundRect(x, y, TTT_CELL_SIZE, TTT_CELL_SIZE, 8, colorCell());
      _tft->drawRoundRect(x, y, TTT_CELL_SIZE, TTT_CELL_SIZE, 8, colorGrid());

      if (board[cell] == TTT_X) {
        drawMarkX(x, y);
      } else if (board[cell] == TTT_O) {
        drawMarkO(x, y);
      }
    }

    void drawMarkX(int16_t x, int16_t y) {
      int16_t pad = 14;
      _tft->drawLine(x + pad, y + pad, x + TTT_CELL_SIZE - pad, y + TTT_CELL_SIZE - pad, colorX());
      _tft->drawLine(x + TTT_CELL_SIZE - pad, y + pad, x + pad, y + TTT_CELL_SIZE - pad, colorX());
      _tft->drawLine(x + pad + 1, y + pad, x + TTT_CELL_SIZE - pad + 1, y + TTT_CELL_SIZE - pad, colorX());
      _tft->drawLine(x + TTT_CELL_SIZE - pad + 1, y + pad, x + pad + 1, y + TTT_CELL_SIZE - pad, colorX());
    }

    void drawMarkO(int16_t x, int16_t y) {
      int16_t cx = x + TTT_CELL_SIZE / 2;
      int16_t cy = y + TTT_CELL_SIZE / 2;
      _tft->drawCircle(cx, cy, 22, colorO());
      _tft->drawCircle(cx, cy, 23, colorO());
    }

    void drawWinningLine() {
      if (winningLineCount != 3) {
        return;
      }

      int16_t x0 = cellOriginX(winningLine[0]) + TTT_CELL_SIZE / 2;
      int16_t y0 = cellOriginY(winningLine[0]) + TTT_CELL_SIZE / 2;
      int16_t x1 = cellOriginX(winningLine[2]) + TTT_CELL_SIZE / 2;
      int16_t y1 = cellOriginY(winningLine[2]) + TTT_CELL_SIZE / 2;
      uint16_t highlight = rgb565(255, 230, 80);

      for (int d = -1; d <= 1; d++) {
        _tft->drawLine(x0 + d, y0 + d, x1 + d, y1 + d, highlight);
      }
    }

    void drawStatus(const char *text) {
      uint16_t bg = colorBg();
      _tft->fillRect(0, TTT_BOARD_Y + TTT_BOARD_W + 10, SCREENWIDTH, 28, bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, bg);
      _tft->drawString(text, SCREENWIDTH / 2, TTT_BOARD_Y + TTT_BOARD_W + 24, 2);
    }

    void drawFooter(const char *text) {
      uint16_t bg = colorBg();
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorDim(), bg);
      _tft->drawString(text, SCREENWIDTH / 2, SCREENHEIGHT - 24, 2);
      if (state == TTT_STATE_WIN || state == TTT_STATE_DRAW) {
        _tft->drawString("Tap to play again", SCREENWIDTH / 2, SCREENHEIGHT - 44, 2);
      }
    }
};

#endif
