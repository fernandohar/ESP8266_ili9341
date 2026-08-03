#ifndef _SCENE_SETTINGS_H_
#define _SCENE_SETTINGS_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "Input.h"
#include "TouchInput.h"
#include "TouchCalibration.h"
#include "PetSave.h"

// Settings screen. Currently exposes touch calibration, reachable either by
// tapping the on-screen button or by pressing the Left hardware button (handy
// when touch is miscalibrated and the button can't be tapped reliably).
class Scene_Settings : public GameScene {
  public:
    Scene_Settings(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = SCENE_PET_TOTORO;
        return;
      }

#if !defined(WOKWI_CAP_TOUCH)
      // Auto-disarm the factory-reset confirm if the second tap never comes.
      if (resetArmed && millis() > resetArmedUntilMs) {
        resetArmed = false;
        drawResetButton(bgColor());
      }

      // Physical trigger: Left button re-runs calibration even without touch.
      if (input.leftPressed) {
        calibrate();
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          if (inCalButton(touchX, touchY)) {
            calibrate();
            return;
          }
          if (inResetButton(touchX, touchY)) {
            handleResetTap();
            wasTouching = isTouching;
            return;
          }
        }
      }
      wasTouching = isTouching;
#else
      (void)isTouching;
#endif
    }

    // Static screen: everything is drawn in initScene(), so per-frame render is a no-op.
    void render() {}

    void initScene() {
      wasTouching = false;
      suppressTouchUntilMs = millis() + 500;
      resetArmed = false;

      uint16_t bg = bgColor();
      setBackgroundColor(bg);
      _tft->fillScreen(bg);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, bg);
      _tft->drawString("Settings", SCREENWIDTH / 2, 40, 4);

#if !defined(WOKWI_CAP_TOUCH)
      uint16_t btnColor = rgb565(70, 130, 180);
      _tft->fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, btnColor);
      _tft->drawRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, TFT_WHITE);
      _tft->setTextColor(TFT_WHITE, btnColor);
      _tft->drawString("Calibrate Touch", SCREENWIDTH / 2, BTN_Y + BTN_H / 2, 2);

      _tft->setTextColor(rgb565(180, 190, 210), bg);
      _tft->drawString("Left btn = Calibrate", SCREENWIDTH / 2, BTN_Y + BTN_H + 14, 2);
#else
      _tft->setTextColor(rgb565(180, 190, 210), bg);
      _tft->drawString("Calibration: HW only", SCREENWIDTH / 2, BTN_Y + BTN_H / 2, 2);
#endif

      drawResetButton(bg);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(180, 190, 210), bg);
      _tft->drawString("Home = Back", SCREENWIDTH / 2, SCREENHEIGHT - 18, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    static const int16_t BTN_X = 30;
    static const int16_t BTN_Y = 96;
    static const int16_t BTN_W = 180;
    static const int16_t BTN_H = 56;

    static const int16_t RESET_BTN_Y = 196;
    static const int16_t RESET_BTN_H = 52;

    // How long the factory-reset button stays "armed" waiting for the second
    // confirming tap before it disarms itself.
    static const unsigned long RESET_ARM_WINDOW_MS = 4000;

    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;
    bool resetArmed = false;
    unsigned long resetArmedUntilMs = 0;

    uint16_t bgColor() {
      return rgb565(30, 34, 48);
    }

    // Redraws the factory-reset button in its current (armed/idle) state,
    // clearing the region first so changing captions don't leave residue.
    void drawResetButton(uint16_t bg) {
      _tft->fillRect(0, RESET_BTN_Y - 2, SCREENWIDTH, RESET_BTN_H + 34, bg);

      uint16_t color = resetArmed ? rgb565(200, 60, 60) : rgb565(120, 70, 82);
      _tft->fillRoundRect(BTN_X, RESET_BTN_Y, BTN_W, RESET_BTN_H, 8, color);
      _tft->drawRoundRect(BTN_X, RESET_BTN_Y, BTN_W, RESET_BTN_H, 8, TFT_WHITE);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, color);
      _tft->drawString(resetArmed ? "Tap again to confirm" : "Factory Reset",
                       SCREENWIDTH / 2, RESET_BTN_Y + RESET_BTN_H / 2, 2);

      _tft->setTextColor(resetArmed ? rgb565(255, 180, 180) : rgb565(180, 190, 210), bg);
      _tft->drawString(resetArmed ? "Erases pet + coins!" : "Erases all progress",
                       SCREENWIDTH / 2, RESET_BTN_Y + RESET_BTN_H + 16, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    bool inResetButton(uint16_t x, uint16_t y) {
      return x >= BTN_X && x < (BTN_X + BTN_W) &&
             y >= RESET_BTN_Y && y < (RESET_BTN_Y + RESET_BTN_H);
    }

    // Two-tap confirm: first tap arms, second tap within the window wipes.
    void handleResetTap() {
      if (!resetArmed) {
        resetArmed = true;
        resetArmedUntilMs = millis() + RESET_ARM_WINDOW_MS;
        addSound(NOTE_A4, noteDurationMs(16, 800));
        drawResetButton(bgColor());
      } else {
        doFactoryReset();
      }
    }

    void doFactoryReset() {
      PetSave::wipe();
      addSound(NOTE_C4, noteDurationMs(6, 700));
      addSound(NOTE_G3, noteDurationMs(4, 700));

      uint16_t bg = bgColor();
      _tft->fillScreen(bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(120, 230, 140), bg);
      _tft->drawString("Save wiped!", SCREENWIDTH / 2, SCREENHEIGHT / 2 - 12, 4);
      _tft->setTextColor(rgb565(180, 190, 210), bg);
      _tft->drawString("Restarting...", SCREENWIDTH / 2, SCREENHEIGHT / 2 + 22, 2);
      _tft->setTextDatum(TL_DATUM);
      delay(1000);

#if defined(ARDUINO_ARCH_ESP32)
      ESP.restart();
#else
      resetArmed = false;
      initScene();
#endif
    }

#if !defined(WOKWI_CAP_TOUCH)
    bool inCalButton(uint16_t x, uint16_t y) {
      return x >= BTN_X && x < (BTN_X + BTN_W) && y >= BTN_Y && y < (BTN_Y + BTN_H);
    }

    void calibrate() {
      uint16_t cal[5];
      TouchCalibration::run(_tft, cal);
      initScene();  // redraw the settings screen after the blocking calibration
    }
#endif
};

#endif
