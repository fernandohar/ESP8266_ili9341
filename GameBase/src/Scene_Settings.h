#ifndef _SCENE_SETTINGS_H_
#define _SCENE_SETTINGS_H_

#include <Arduino.h>
#include "GameScene.h"
#include "Input.h"
#include "TouchInput.h"
#include "TouchCalibration.h"

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
        *nextSceneIndex = 0;
        return;
      }

#if !defined(WOKWI_CAP_TOUCH)
      // Physical trigger: Left button re-runs calibration even without touch.
      if (input.leftPressed) {
        calibrate();
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY) && inCalButton(touchX, touchY)) {
          calibrate();
          return;
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

      uint16_t bg = rgb565(30, 34, 48);
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
      _tft->drawString("Left btn = Calibrate", SCREENWIDTH / 2, BTN_Y + BTN_H + 34, 2);
#else
      _tft->setTextColor(rgb565(180, 190, 210), bg);
      _tft->drawString("Calibration: HW only", SCREENWIDTH / 2, SCREENHEIGHT / 2, 2);
#endif

      _tft->setTextColor(rgb565(180, 190, 210), bg);
      _tft->drawString("Home = Back", SCREENWIDTH / 2, SCREENHEIGHT - 30, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    static const int16_t BTN_X = 30;
    static const int16_t BTN_Y = 120;
    static const int16_t BTN_W = 180;
    static const int16_t BTN_H = 64;

    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;

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
