#ifndef _SCENE_STATUS_H_
#define _SCENE_STATUS_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "Input.h"
#include "TouchInput.h"
#include "PetTotoroState.h"
#include "GameProgress.h"

// Read-only status screen reached from the pet's radial menu ("Info"). Shows the
// four care stats and coins. Growth stage and care-XP are deliberately not shown
// - the pet grows from baby to adult on its own and the numbers behind that are
// not something the player acts on. It is a static screen: everything is drawn
// once in initScene() and render() is a no-op (per the project's static-screen
// guidance). Home or the Back button returns to the pet's home.
class Scene_Status : public GameScene {
  public:
    Scene_Status(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = SCENE_PET_TOTORO;
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          if (inRect(touchX, touchY, BACK_X, BACK_Y, BACK_W, BACK_H)) {
            addSound(NOTE_G5, noteDurationMs(8, 800));
            *needChangeScene = true;
            *nextSceneIndex = SCENE_PET_TOTORO;
          }
        }
      }

      wasTouching = isTouching;
    }

    void render() {
      // Static screen: nothing to animate.
    }

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 250;
      draw();
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    static const int16_t BACK_X = 60;
    static const int16_t BACK_Y = 274;
    static const int16_t BACK_W = 120;
    static const int16_t BACK_H = 36;

    // Vertical layout. With the stage/XP block gone the four stat rows move up
    // and everything below gets room: coins are font 4 (26px tall) and the sick
    // line only appears sometimes, but both now clear the Back button.
    static const int16_t STATS_Y = 70;
    static const int16_t STATS_ROW_H = 30;
    static const int16_t COINS_Y = 166;
    static const int16_t SICK_Y = 240;

    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;

    uint16_t bg() {
      return rgb565(22, 26, 24);
    }

    static bool inRect(uint16_t tx, uint16_t ty, int16_t x, int16_t y, int16_t w, int16_t h) {
      return (tx >= x && tx < x + w && ty >= y && ty < y + h);
    }

    void draw() {
      uint16_t back = bg();
      _tft->fillScreen(back);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(232, 232, 210), back);
      _tft->drawString("Status", SCREENWIDTH / 2, 22, 4);

      int16_t y = STATS_Y;
      const PetTotoroStats &s = PetTotoroState::stats();
      meterRow(y, "Hunger", s.hunger);
      meterRow(y + STATS_ROW_H, "Happy", s.happiness);
      meterRow(y + STATS_ROW_H * 2, "Clean", s.cleanness);

      drawCoins(COINS_Y);

      if (PetTotoroState::isSick()) {
        _tft->setTextDatum(MC_DATUM);
        _tft->setTextColor(rgb565(240, 120, 120), back);
        _tft->drawString("Feeling unhappy - care for it!", SCREENWIDTH / 2, SICK_Y, 2);
      }

      drawBackButton();
      _tft->setTextDatum(TL_DATUM);
    }

    void meterRow(int16_t y, const char *label, int value) {
      uint16_t back = bg();
      _tft->setTextDatum(TL_DATUM);
      _tft->setTextColor(rgb565(212, 216, 206), back);
      _tft->drawString(label, 18, y, 2);

      int pips = value / PET_STAT_PER_PIP;
      if (pips > PET_STAT_PIPS) {
        pips = PET_STAT_PIPS;
      }
      int16_t px = 104;
      int16_t pw = 18;
      int16_t ph = 14;
      int16_t gap = 5;
      for (int i = 0; i < PET_STAT_PIPS; i++) {
        uint16_t c = (i < pips) ? rgb565(250, 210, 70) : rgb565(52, 56, 50);
        _tft->fillRect(px + i * (pw + gap), y, pw, ph, c);
      }

      _tft->setTextDatum(TR_DATUM);
      _tft->setTextColor(rgb565(160, 168, 158), back);
      char buf[8];
      snprintf(buf, sizeof(buf), "%d", value);
      _tft->drawString(buf, SCREENWIDTH - 14, y, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawCoins(int16_t y) {
      uint16_t back = bg();
      _tft->setTextDatum(TL_DATUM);
      _tft->setTextColor(rgb565(250, 220, 120), back);
      char buf[20];
      snprintf(buf, sizeof(buf), "Coins: %d", GameProgress::getCoins());
      _tft->drawString(buf, 18, y, 4);
    }

    void drawBackButton() {
      uint16_t c = rgb565(70, 90, 120);
      _tft->fillRoundRect(BACK_X, BACK_Y, BACK_W, BACK_H, 8, c);
      _tft->drawRoundRect(BACK_X, BACK_Y, BACK_W, BACK_H, 8, rgb565(160, 180, 210));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, c);
      _tft->drawString("Back", SCREENWIDTH / 2, BACK_Y + BACK_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }
};

#endif
