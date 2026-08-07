#ifndef _SCENE_COINREWARD_H_
#define _SCENE_COINREWARD_H_

#include <Arduino.h>
#include <string.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "GameProgress.h"
#include "GameResult.h"
#include "Input.h"
#include "PetSave.h"
#include "image_grass_tile.h"

// Payout screen shown between a mini-game and the pet's home: tiled grass with a
// single blue panel reading "REWARDED / <n> COINS". Every game routes through
// here via gameExitSceneIndex() once its round is over, so the amount is always
// announced in the same place.
//
// The coins are banked here rather than back at the pet home, and NVS is written
// straight away, so the number on screen is the number that survives a reboot.
// They are only ever cleared by the factory reset in Settings (PetSave::wipe).

#define COIN_REWARD_BOX_X 24
#define COIN_REWARD_BOX_Y 98
#define COIN_REWARD_BOX_W 192
#define COIN_REWARD_BOX_H 104
#define COIN_REWARD_BOX_RADIUS 12
#define COIN_REWARD_LINE1_CY 132
#define COIN_REWARD_LINE2_CY 176
// Padding kept clear inside the panel when picking the text size.
#define COIN_REWARD_TEXT_INSET 8
// Swallow the tap/press that ended the game so the screen isn't skipped instantly.
#define COIN_REWARD_GRACE_MS 450

class Scene_CoinReward : public GameScene {
  public:
    Scene_CoinReward(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      if (millis() > dismissAfterMs && (input.homePressed || (isTouching && !wasTouching))) {
        *needChangeScene = true;
        *nextSceneIndex = SCENE_PET_TOTORO;
      }
      wasTouching = isTouching;
    }

    // Nothing animates, so the screen is painted once in initScene().
    void render() {
    }

    void initScene() {
      int reward = GameResult::takeCoins();
      if (reward > 0) {
        GameProgress::addCoins(reward);
        PetSave::save();
      }

      setBackgroundTile(grass_tile, GRASS_TILE_WIDTH, GRASS_TILE_HEIGHT);
      renderFullScreen();
      drawPanel(reward);

      // A tap may still be held from the game's end screen; require a release.
      wasTouching = true;
      dismissAfterMs = millis() + COIN_REWARD_GRACE_MS;

      addSound(NOTE_E5, noteDurationMs(16, 900));
      addSound(NOTE_G5, noteDurationMs(16, 900));
      addSound(NOTE_C6, noteDurationMs(8, 900));
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    boolean wasTouching = true;
    unsigned long dismissAfterMs = 0;

    uint16_t colorPanel() const { return rgb565(28, 116, 238); }
    uint16_t colorPanelRim() const { return rgb565(120, 178, 255); }

    void drawPanel(int reward) {
      uint16_t panel = colorPanel();
      _tft->fillRoundRect(COIN_REWARD_BOX_X, COIN_REWARD_BOX_Y, COIN_REWARD_BOX_W,
                          COIN_REWARD_BOX_H, COIN_REWARD_BOX_RADIUS, panel);
      _tft->drawRoundRect(COIN_REWARD_BOX_X, COIN_REWARD_BOX_Y, COIN_REWARD_BOX_W,
                          COIN_REWARD_BOX_H, COIN_REWARD_BOX_RADIUS, colorPanelRim());

      char amount[16];
      snprintf(amount, sizeof(amount), "%d COINS", reward);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, panel);
      _tft->setTextSize(textSizeFor(amount));
      _tft->drawString("REWARDED", SCREENWIDTH / 2, COIN_REWARD_LINE1_CY, 1);
      _tft->drawString(amount, SCREENWIDTH / 2, COIN_REWARD_LINE2_CY, 1);
      _tft->setTextSize(1);
      _tft->setTextDatum(TL_DATUM);
    }

    // Largest whole multiple of the built-in font that still fits the panel.
    // A glyph of the default font advances 6px per character at size 1.
    uint8_t textSizeFor(const char *longestLine) const {
      int inner = COIN_REWARD_BOX_W - 2 * COIN_REWARD_TEXT_INSET;
      int chars = (int)strlen(longestLine);
      for (uint8_t size = 3; size > 1; size--) {
        if (chars * 6 * size <= inner) {
          return size;
        }
      }
      return 1;
    }
};

#endif
