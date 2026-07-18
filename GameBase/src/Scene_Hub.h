#ifndef _SCENE_HUB_H_
#define _SCENE_HUB_H_

#include <Arduino.h>
#include "GameScene.h"
#include "Input.h"
#include "TouchInput.h"
#include "image_hub_map.h"

#define SCENE_HUB 0
#define SCENE_PET_TOTORO 1
#define SCENE_ACORN_CATCH 2
#define SCENE_SETTINGS 3
#define SCENE_TIC_TAC_TOE 4
#define SCENE_WHACK_A_MOLE 5

struct HubLocation {
  int16_t ringX;
  int16_t ringY;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  int sceneIndex;
};

class Scene_Hub : public GameScene {
  public:
    Scene_Hub(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();

      if (input.leftPressed) {
        moveSelection(-1);
      } else if (input.rightPressed) {
        moveSelection(1);
      } else if (input.homePressed && millis() > suppressLaunchUntilMs) {
        launchSelected(needChangeScene, nextSceneIndex);
      }

      if (isTouching && !wasTouching) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          int touchedIndex = hotspotAt(touchX, touchY);
          if (touchedIndex >= 0) {
            selectedIndex = touchedIndex;
            needsRedraw = true;
            requestRender();
            launchSelected(needChangeScene, nextSceneIndex);
          }
        }
      }

      wasTouching = isTouching;
    }

    void render() {
      renderFullScreen();
      drawSelectionRing();
      needsRedraw = false;
    }

    void initScene() {
      setBackground(hub_map);
      drawBackground(hub_map);
      selectedIndex = 1;
      wasTouching = false;
      needsRedraw = true;
      suppressLaunchUntilMs = millis() + 800;
      requestRender();
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    static const int LOCATION_COUNT = 5;

    HubLocation locations[LOCATION_COUNT] = {
      { 58, 108, 12, 58, 108, 132, SCENE_ACORN_CATCH },
      { 120, 168, 55, 118, 132, 132, SCENE_PET_TOTORO },
      { 178, 248, 118, 195, 118, 115, SCENE_SETTINGS },
      { 180, 60, 130, 12, 100, 80, SCENE_TIC_TAC_TOE },
      { 60, 280, 12, 240, 90, 70, SCENE_WHACK_A_MOLE },
    };

    int selectedIndex = 1;
    boolean wasTouching = false;
    boolean needsRedraw = true;
    unsigned long suppressLaunchUntilMs = 0;

    void moveSelection(int delta) {
      selectedIndex += delta;
      if (selectedIndex < 0) {
        selectedIndex = LOCATION_COUNT - 1;
      } else if (selectedIndex >= LOCATION_COUNT) {
        selectedIndex = 0;
      }
      addSound(NOTE_E5, noteDurationMs(16, 800));
      needsRedraw = true;
      requestRender();
    }

    void launchSelected(boolean *needChangeScene, int *nextSceneIndex) {
      *needChangeScene = true;
      *nextSceneIndex = locations[selectedIndex].sceneIndex;
      addSound(NOTE_G5, noteDurationMs(8, 800));
      addSound(NOTE_C5, noteDurationMs(8, 800));
    }

    int hotspotAt(uint16_t x, uint16_t y) {
      for (int i = 0; i < LOCATION_COUNT; i++) {
        HubLocation &loc = locations[i];
        if (x >= loc.x && x < (loc.x + loc.w) && y >= loc.y && y < (loc.y + loc.h)) {
          return i;
        }
      }
      return -1;
    }

    void drawSelectionRing() {
      HubLocation &loc = locations[selectedIndex];
      int16_t cx = loc.ringX;
      int16_t cy = loc.ringY;
      uint16_t ringColor = rgb565(255, 220, 0);

      for (int r = 44; r <= 48; r++) {
        _tft->drawCircle(cx, cy, r, ringColor);
      }
    }
};

#endif
