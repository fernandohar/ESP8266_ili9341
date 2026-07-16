#ifndef _SCENE_STUB_H_
#define _SCENE_STUB_H_

#include <Arduino.h>
#include "GameScene.h"
#include "Input.h"

// Simple placeholder scene; Home button returns to the hub.
class Scene_Stub : public GameScene {
  public:
    Scene_Stub(TFT_eSPI *tft, const char *title) : _title(title) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      (void)isTouching;

      const GameInput &input = Input::current();
      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = 0;
      }
    }

    void render() {
      renderFullScreen();
    }

    void initScene() {
      setBackgroundColor(rgb565(135, 206, 235));
      _tft->fillScreen(rgb565(135, 206, 235));
      _tft->setTextColor(TFT_DARKGREEN, rgb565(135, 206, 235));
      _tft->setTextDatum(MC_DATUM);
      _tft->drawString(_title, SCREENWIDTH / 2, SCREENHEIGHT / 2 - 20, 4);
      _tft->setTextColor(TFT_BLACK, rgb565(135, 206, 235));
      _tft->drawString("Coming Soon", SCREENWIDTH / 2, SCREENHEIGHT / 2 + 20, 2);
      _tft->drawString("Home = Back", SCREENWIDTH / 2, SCREENHEIGHT - 30, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void destroyScene() {
      GameScene::destroyScene();
    }

  private:
    const char *_title;
};

#endif
