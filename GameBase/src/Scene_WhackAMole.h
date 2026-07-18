#ifndef _SCENE_WHACKAMOLE_H_
#define _SCENE_WHACKAMOLE_H_

#include <Arduino.h>
#include "GameScene.h"
#include "Input.h"
#include "TouchInput.h"

// Placeholder whack-a-mole scene. Replace drawHole()/drawMole() with sprite
// assets when custom graphics are ready (see AGENTS.md asset pipeline).

#define WAM_GRID_COLS 3
#define WAM_GRID_ROWS 3
#define WAM_HOLE_COUNT (WAM_GRID_COLS * WAM_GRID_ROWS)
#define WAM_HOLE_SIZE 64
#define WAM_HOLE_GAP 10
#define WAM_GRID_W (WAM_HOLE_SIZE * WAM_GRID_COLS + WAM_HOLE_GAP * (WAM_GRID_COLS - 1))
#define WAM_GRID_X ((SCREENWIDTH - WAM_GRID_W) / 2)
#define WAM_GRID_Y 78
#define WAM_ROUND_MS 30000
#define WAM_MOLE_VISIBLE_MIN_MS 600
#define WAM_MOLE_VISIBLE_MAX_MS 1400
#define WAM_SPAWN_MIN_MS 400
#define WAM_SPAWN_MAX_MS 900

enum WhackAMoleState {
  WAM_STATE_READY,
  WAM_STATE_PLAYING,
  WAM_STATE_ENDED
};

struct MoleHole {
  int16_t x;
  int16_t y;
  bool active;
  unsigned long hideAtMs;
};

class Scene_WhackAMole : public GameScene {
  public:
    Scene_WhackAMole(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = 0;
        return;
      }

      if (state == WAM_STATE_ENDED) {
        if (isTouching && !wasTouching) {
          resetGame();
        }
        wasTouching = isTouching;
        return;
      }

      if (state == WAM_STATE_READY) {
        if (now - stateStartMs > 800) {
          beginPlay(now);
        }
        wasTouching = isTouching;
        return;
      }

      updateMoles(now);
      trySpawnMole(now);

      if (now >= roundEndMs) {
        endRound();
        wasTouching = isTouching;
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          whackAt(touchX, touchY);
        }
      }

      int timeLeft = (roundEndMs > now) ? (int)((roundEndMs - now) / 1000) : 0;
      if (timeLeft != lastHudTimeLeft || score != lastHudScore) {
        drawHud(timeLeft, score);
        lastHudTimeLeft = timeLeft;
        lastHudScore = score;
      }

      wasTouching = isTouching;
    }

    void render() {}

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 400;
      initHoles();
      resetGame();
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    MoleHole holes[WAM_HOLE_COUNT];
    WhackAMoleState state = WAM_STATE_READY;
    unsigned long stateStartMs = 0;
    unsigned long roundEndMs = 0;
    unsigned long nextSpawnMs = 0;
    int score = 0;
    int lastHudTimeLeft = -1;
    int lastHudScore = -1;
    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;

    uint16_t colorBg() const { return rgb565(34, 52, 34); }
    uint16_t colorGrass() const { return rgb565(56, 110, 56); }
    uint16_t colorHole() const { return rgb565(45, 30, 18); }
    uint16_t colorMole() const { return rgb565(120, 80, 50); }
    uint16_t colorNose() const { return rgb565(220, 120, 100); }
    uint16_t colorDim() const { return rgb565(180, 200, 170); }

    void initHoles() {
      for (int row = 0; row < WAM_GRID_ROWS; row++) {
        for (int col = 0; col < WAM_GRID_COLS; col++) {
          int index = row * WAM_GRID_COLS + col;
          holes[index].x = WAM_GRID_X + col * (WAM_HOLE_SIZE + WAM_HOLE_GAP);
          holes[index].y = WAM_GRID_Y + row * (WAM_HOLE_SIZE + WAM_HOLE_GAP);
          holes[index].active = false;
          holes[index].hideAtMs = 0;
        }
      }
    }

    void resetGame() {
      score = 0;
      state = WAM_STATE_READY;
      stateStartMs = millis();
      roundEndMs = 0;
      nextSpawnMs = 0;
      lastHudTimeLeft = -1;
      lastHudScore = -1;

      for (int i = 0; i < WAM_HOLE_COUNT; i++) {
        holes[i].active = false;
        holes[i].hideAtMs = 0;
      }

      drawScreen();
    }

    void beginPlay(unsigned long now) {
      state = WAM_STATE_PLAYING;
      stateStartMs = now;
      roundEndMs = now + WAM_ROUND_MS;
      nextSpawnMs = now + 500;
      lastHudTimeLeft = -1;
      lastHudScore = -1;
      drawHud(WAM_ROUND_MS / 1000, 0);
      addSound(NOTE_C5, noteDurationMs(8, 800));
    }

    void endRound() {
      state = WAM_STATE_ENDED;

      for (int i = 0; i < WAM_HOLE_COUNT; i++) {
        if (holes[i].active) {
          holes[i].active = false;
          drawHole(i);
        }
      }

      drawStatus("TIME UP!");
      drawFooter("Home = Back");
      addSound(NOTE_G4, noteDurationMs(8, 700));
      addSound(NOTE_E4, noteDurationMs(8, 700));
    }

    void updateMoles(unsigned long now) {
      for (int i = 0; i < WAM_HOLE_COUNT; i++) {
        if (holes[i].active && now >= holes[i].hideAtMs) {
          holes[i].active = false;
          drawHole(i);
        }
      }
    }

    void trySpawnMole(unsigned long now) {
      if (now < nextSpawnMs) {
        return;
      }

      int open[WAM_HOLE_COUNT];
      int openCount = 0;
      for (int i = 0; i < WAM_HOLE_COUNT; i++) {
        if (!holes[i].active) {
          open[openCount++] = i;
        }
      }

      if (openCount > 0) {
        int index = open[random(0, openCount)];
        holes[index].active = true;
        holes[index].hideAtMs = now + random(WAM_MOLE_VISIBLE_MIN_MS, WAM_MOLE_VISIBLE_MAX_MS);
        drawMole(index);
      }

      nextSpawnMs = now + random(WAM_SPAWN_MIN_MS, WAM_SPAWN_MAX_MS);
    }

    int holeAt(uint16_t x, uint16_t y) const {
      for (int i = 0; i < WAM_HOLE_COUNT; i++) {
        const MoleHole &hole = holes[i];
        if (x >= hole.x && x < hole.x + WAM_HOLE_SIZE &&
            y >= hole.y && y < hole.y + WAM_HOLE_SIZE) {
          return i;
        }
      }
      return -1;
    }

    void whackAt(uint16_t x, uint16_t y) {
      int index = holeAt(x, y);
      if (index < 0) {
        return;
      }

      if (holes[index].active) {
        holes[index].active = false;
        score++;
        drawHole(index);
        drawWhackFlash(index);
        addSound(NOTE_E5, noteDurationMs(16, 900));
        addSound(NOTE_G5, noteDurationMs(32, 900));
      } else {
        addSound(NOTE_A3, noteDurationMs(32, 700));
      }
    }

    void drawScreen() {
      uint16_t bg = colorBg();
      setBackgroundColor(bg);
      _tft->fillScreen(bg);

      _tft->fillRect(0, WAM_GRID_Y - 16, SCREENWIDTH, WAM_GRID_H() + 32, colorGrass());

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, bg);
      _tft->drawString("Whack-a-Mole", SCREENWIDTH / 2, 24, 4);

      for (int i = 0; i < WAM_HOLE_COUNT; i++) {
        drawHole(i);
      }

      drawHud(WAM_ROUND_MS / 1000, 0);
      drawStatus("GET READY...");
      drawFooter("Home = Back");
      _tft->setTextDatum(TL_DATUM);
    }

    int16_t WAM_GRID_H() const {
      return WAM_HOLE_SIZE * WAM_GRID_ROWS + WAM_HOLE_GAP * (WAM_GRID_ROWS - 1);
    }

    void drawHole(int index) {
      const MoleHole &hole = holes[index];
      int16_t cx = hole.x + WAM_HOLE_SIZE / 2;
      int16_t cy = hole.y + WAM_HOLE_SIZE / 2 + 8;

      _tft->fillRect(hole.x, hole.y, WAM_HOLE_SIZE, WAM_HOLE_SIZE, colorGrass());
      _tft->fillEllipse(cx, cy, 26, 14, colorHole());
      _tft->drawEllipse(cx, cy, 26, 14, rgb565(25, 15, 8));
    }

    void drawMole(int index) {
      const MoleHole &hole = holes[index];
      int16_t cx = hole.x + WAM_HOLE_SIZE / 2;
      int16_t cy = hole.y + WAM_HOLE_SIZE / 2 - 4;

      drawHole(index);

      _tft->fillCircle(cx, cy, 22, colorMole());
      _tft->drawCircle(cx, cy, 22, rgb565(80, 50, 30));

      _tft->fillCircle(cx - 8, cy - 4, 4, TFT_WHITE);
      _tft->fillCircle(cx + 8, cy - 4, 4, TFT_WHITE);
      _tft->fillCircle(cx - 7, cy - 4, 2, TFT_BLACK);
      _tft->fillCircle(cx + 9, cy - 4, 2, TFT_BLACK);

      _tft->fillCircle(cx, cy + 6, 5, colorNose());
    }

    void drawWhackFlash(int index) {
      const MoleHole &hole = holes[index];
      int16_t cx = hole.x + WAM_HOLE_SIZE / 2;
      int16_t cy = hole.y + WAM_HOLE_SIZE / 2;
      uint16_t flash = rgb565(255, 255, 180);

      for (int r = 24; r <= 28; r++) {
        _tft->drawCircle(cx, cy, r, flash);
      }
    }

    void drawHud(int timeLeft, int displayScore) {
      uint16_t bg = colorBg();
      char buf[24];

      _tft->fillRect(0, 48, SCREENWIDTH, 22, bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorDim(), bg);
      snprintf(buf, sizeof(buf), "Time: %02d    Score: %d", timeLeft, displayScore);
      _tft->drawString(buf, SCREENWIDTH / 2, 58, 2);
    }

    void drawStatus(const char *text) {
      uint16_t bg = colorBg();
      int16_t y = WAM_GRID_Y + WAM_GRID_H() + 24;
      _tft->fillRect(0, y - 12, SCREENWIDTH, 28, bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, bg);
      _tft->drawString(text, SCREENWIDTH / 2, y, 2);
    }

    void drawFooter(const char *text) {
      uint16_t bg = colorBg();
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorDim(), bg);
      _tft->drawString(text, SCREENWIDTH / 2, SCREENHEIGHT - 24, 2);

      if (state == WAM_STATE_ENDED) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Score: %d  Tap to replay", score);
        _tft->drawString(buf, SCREENWIDTH / 2, SCREENHEIGHT - 44, 2);
      } else if (state == WAM_STATE_PLAYING) {
        _tft->drawString("Tap the moles!", SCREENWIDTH / 2, SCREENHEIGHT - 44, 2);
      }
    }
};

#endif
