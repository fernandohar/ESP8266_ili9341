#ifndef _SCENE_TOTOROHOME_H_
#define _SCENE_TOTOROHOME_H_
#include "GameScene.h"
#include "Avatar.h"
#include "BackgroundTotoroHome.h"
#include "forest_spirit.h"
#include "forest_spirit_sleep.h"
#include "furniture_chabudai.h"
#include "furniture_cushion.h"
#include "furniture_plant.h"
#include "furniture_acorn.h"
#include "pitches.h"

// Virtual pet room inspired by Ghibli washitsu aesthetic.
// Tap furniture icons to place; tap placed item to remove.
// Forest spirit wanders, breathes, and naps when idle.

#define TOTORO_HOME_MAX_FURNITURE 8

struct FurnitureSlot {
  const uint16_t *bitmap;
  const uint8_t *mask;
  uint16_t width;
  uint16_t height;
  const char *name;
};

class Scene_TotoroHome : public GameScene {
  public:
    Scene_TotoroHome(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, uint16_t touchX, uint16_t touchY,
                boolean *needChangeScene, int *nextSceneIndex) {
      unsigned long now = millis();

      if (isTouching) {
        if (!wasTouching) {
          handleTap(touchX, touchY);
        }
        wasTouching = true;
      } else {
        if (wasTouching) {
          wasTouching = false;
        }
      }

      // Pet AI: wander slowly, nap when idle
      if (spirit) {
        spirit->updatePos(now);
        if (now - lastMoveTime > 4000 && spirit->velocity.x == 0) {
          int dir = (random(0, 2) == 0) ? -1 : 1;
          spirit->setVelocity(dir, 0);
          spirit->updateInterval = 500;
          isSleeping = false;
          lastMoveTime = now;
        }
        if (now - lastMoveTime > 8000) {
          spirit->setVelocity(0, 0);
          if (!isSleeping) {
            isSleeping = true;
            spirit->bitmap = FOREST_SPIRIT_SLEEPBitmap;
            spirit->mask = FOREST_SPIRIT_SLEEPMask;
            spirit->height = FOREST_SPIRIT_SLEEP_HEIGHT;
          }
        } else if (isSleeping && spirit->velocity.x != 0) {
          isSleeping = false;
          spirit->bitmap = FOREST_SPIRITBitmap;
          spirit->mask = FOREST_SPIRITMask;
          spirit->height = FOREST_SPIRIT_HEIGHT;
        }
        boundSpiritToFloor(spirit);
      }

      for (int i = 0; i < numPlaced; i++) {
        // Furniture is static; included in avatar list for rendering
      }
    }

    void render() {
      renderScene();
    }

    void initScene() {
      setBackground(BackgroundTotoroHomeBitmap);
      wasTouching = false;
      numPlaced = 0;
      selectedCatalog = 0;
      isSleeping = false;
      lastMoveTime = millis();

      catalog[0] = {FURNITURE_CHABUDAIBitmap, FURNITURE_CHABUDAIMask,
                    FURNITURE_CHABUDAI_WIDTH, FURNITURE_CHABUDAI_HEIGHT, "table"};
      catalog[1] = {FURNITURE_CUSHIONBitmap, FURNITURE_CUSHIONMask,
                    FURNITURE_CUSHION_WIDTH, FURNITURE_CUSHION_HEIGHT, "cushion"};
      catalog[2] = {FURNITURE_PLANTBitmap, FURNITURE_PLANTMask,
                    FURNITURE_PLANT_WIDTH, FURNITURE_PLANT_HEIGHT, "plant"};
      catalog[3] = {FURNITURE_ACORNBitmap, FURNITURE_ACORNMask,
                    FURNITURE_ACORN_WIDTH, FURNITURE_ACORN_HEIGHT, "acorn"};

      spirit = new Avatar(100, 250, FOREST_SPIRIT_WIDTH, FOREST_SPIRIT_HEIGHT,
                          FOREST_SPIRITBitmap, FOREST_SPIRITMask);
      spirit->id = 0;
      spirit->enableBreathing();
      spirit->setBreathInterval(600);
      spirit->setBreathPosition(18);
      spirit->breathAmount = 2;
      spirit->setVelocity(1, 0);
      spirit->updateInterval = 500;
      appendAvatar(spirit);

      drawBackground(BackgroundTotoroHomeBitmap);
    }

    void destroyScene() {
      for (int i = 0; i < numPlaced; i++) {
        delete placed[i];
        placed[i] = NULL;
      }
      numPlaced = 0;
      spirit = NULL;
      GameScene::destroyScene();
    }

  private:
    boolean wasTouching = false;
    boolean isSleeping = false;
    unsigned long lastMoveTime = 0;
    int selectedCatalog = 0;
    int numPlaced = 0;
    Avatar *spirit = NULL;
    Avatar *placed[TOTORO_HOME_MAX_FURNITURE];
    FurnitureSlot catalog[4];

    static const int CATALOG_Y = 2;
    static const int FLOOR_Y_MIN = 210;
    static const int FLOOR_Y_MAX = 290;

    void handleTap(uint16_t tx, uint16_t ty) {
      // Catalog bar at top (y < 40): select item type
      if (ty < 40) {
        int slot = tx / 60;
        if (slot >= 0 && slot < 4) {
          selectedCatalog = slot;
          tone(16, NOTE_C5, 80);
        }
        return;
      }

      // Tap placed furniture to remove
      for (int i = numPlaced - 1; i >= 0; i--) {
        Avatar *f = placed[i];
        if (tx >= f->x && tx < f->x + f->width &&
            ty >= f->y && ty < f->y + f->height) {
          removeFurniture(i);
          tone(16, NOTE_G4, 100);
          return;
        }
      }

      // Place selected furniture on tatami floor
      if (ty >= FLOOR_Y_MIN && ty <= FLOOR_Y_MAX && numPlaced < TOTORO_HOME_MAX_FURNITURE) {
        placeFurniture(tx - catalog[selectedCatalog].width / 2, ty - catalog[selectedCatalog].height / 2);
        tone(16, NOTE_E5, 100);
      }
    }

    void placeFurniture(int x, int y) {
      FurnitureSlot &item = catalog[selectedCatalog];
      x = constrain(x, 10, SCREENWIDTH - item.width - 10);
      y = constrain(y, FLOOR_Y_MIN, FLOOR_Y_MAX - item.height);
      Avatar *f = new Avatar(x, y, item.width, item.height, item.bitmap, item.mask);
      f->id = 10 + numPlaced;
      placed[numPlaced] = f;
      appendAvatar(f);
      numPlaced++;
    }

    void removeFurniture(int index) {
      Avatar *f = placed[index];
      for (int i = 0; i < numAvatar; i++) {
        if (avatars[i] == f) {
          for (int j = i; j < numAvatar - 1; j++) {
            avatars[j] = avatars[j + 1];
          }
          numAvatar--;
          break;
        }
      }
      delete f;
      for (int i = index; i < numPlaced - 1; i++) {
        placed[i] = placed[i + 1];
      }
      numPlaced--;
    }

    void boundSpiritToFloor(Avatar *a) {
      if (a->x <= 5) {
        a->x = 5;
        a->velocity.x = abs(a->velocity.x);
      }
      if (a->x + a->width >= SCREENWIDTH - 5) {
        a->x = SCREENWIDTH - 5 - a->width;
        a->velocity.x = -abs(a->velocity.x);
      }
      if (a->y < FLOOR_Y_MIN) a->y = FLOOR_Y_MIN;
      if (a->y + a->height > FLOOR_Y_MAX) {
        a->y = FLOOR_Y_MAX - a->height;
      }
    }
};

#endif
