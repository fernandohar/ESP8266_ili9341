#ifndef _SCENE_PETTOTORO_H_
#define _SCENE_PETTOTORO_H_

#include <Arduino.h>
#include "GameScene.h"
#include "PetTotoroState.h"
#include "Input.h"
#include "SpriteSheet.h"
#include "SpriteText.h"
#include "TouchInput.h"
#include "image_pet_totoro_bg.h"
#include "sprite_chu_totoro.h"
#include "sprite_soot.h"
#include "sprite_letters.h"

#define PET_HUD_HEIGHT 40
#define PET_PLAY_TOP PET_HUD_HEIGHT
#define PET_WALK_MIN_X 14
#define PET_WALK_MAX_X 206
#define PET_WALK_MIN_Y 212
#define PET_WALK_MAX_Y 272
#define PET_WALK_SPEED 1.4f
#define MAX_SOOT 8
#define MAX_END_MESSAGE_GLYPHS 16
#define END_MESSAGE_LINE1_Y 150
#define END_MESSAGE_LINE2_Y 175
#define END_MESSAGE_GAP 1

#define PET_HUNGER_TICK_MS 45000
#define PET_HAPPINESS_TICK_MS 60000
#define PET_HEALTH_TICK_MS 30000
#define PET_SOOT_SPAWN_MIN_MS 18000
#define PET_SOOT_SPAWN_MAX_MS 36000
#define PET_SOOT_VARIANT_COUNT 16
#define PET_WALK_DECISION_MIN_MS 1800
#define PET_WALK_DECISION_MAX_MS 4200

enum PetRoomState {
  PET_ROOM_ACTIVE,
  PET_ROOM_GAME_OVER
};

class Scene_PetTotoro : public GameScene {
  public:
    Scene_PetTotoro(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (roomState == PET_ROOM_GAME_OVER) {
        if (input.homePressed || input.home) {
          PetTotoroState::reset();
          *needChangeScene = true;
          *nextSceneIndex = 0;
        }
        return;
      }

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = 0;
        return;
      }

      tickStats(now);
      updateWalk(now);
      updateSoot(now);

      if (isTouching && !wasTouching) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          if (tryCleanSoot(touchX, touchY)) {
            wasTouching = isTouching;
            requestRender();
            return;
          }
        }
      }

      if (totoro != NULL) {
        totoro->updatePos(now);
        clampTotoro();
      }

      if (!PetTotoroState::isAlive() || PetTotoroState::stats().health <= PET_STAT_MIN) {
        triggerGameOver();
        wasTouching = isTouching;
        return;
      }

      wasTouching = isTouching;
      requestRender();
    }

    void render() {
      renderScene();
      if (roomState == PET_ROOM_ACTIVE) {
        drawStatsHud();
      }
    }

    void initScene() {
      setBackground(pet_totoro_bg);
      drawBackground(pet_totoro_bg);

      if (PetTotoroState::isGameOver()) {
        roomState = PET_ROOM_GAME_OVER;
      } else {
        roomState = PET_ROOM_ACTIVE;
      }

      int16_t totoroX = (SCREENWIDTH - SPRITE_CHU_TOTORO_WIDTH) / 2;
      totoro = new Avatar(totoroX, PET_WALK_MIN_Y, SPRITE_CHU_TOTORO_WIDTH, SPRITE_CHU_TOTORO_HEIGHT,
                          sprite_chu_totoro, sprite_chu_totoroMask);
      totoro->setVelocity(0, 0);
      totoro->updateInterval = 50;
      appendAvatar(totoro);

      initSootPool();
      clearEndMessageAvatars();
      wasTouching = false;
      nextWalkDecisionMs = millis() + PET_WALK_DECISION_MIN_MS;
      nextHungerTickMs = millis() + PET_HUNGER_TICK_MS;
      nextHappinessTickMs = millis() + PET_HAPPINESS_TICK_MS;
      nextHealthTickMs = millis() + PET_HEALTH_TICK_MS;
      nextSootSpawnMs = millis() + PET_SOOT_SPAWN_MIN_MS;
      lastHudHealth = -1;
      lastHudHunger = -1;
      lastHudHappiness = -1;
      lastHudCleanness = -1;

      if (roomState == PET_ROOM_GAME_OVER) {
        showEndMessage("GAME OVER", "HOME");
      } else {
        chooseNewWalk(millis());
      }

      renderFullScreen();
      if (roomState == PET_ROOM_ACTIVE) {
        drawStatsHud();
      }
      requestRender();
    }

    void destroyScene() {
      totoro = NULL;
      for (int i = 0; i < MAX_SOOT; i++) {
        sootSlots[i].avatar = NULL;
        sootSlots[i].active = false;
      }
      clearEndMessageAvatars();
      GameScene::destroyScene();
    }

  private:
    struct SootSlot {
      Avatar *avatar;
      bool active;
      int variant;
    };

    Avatar *totoro = NULL;
    SootSlot sootSlots[MAX_SOOT];
    boolean wasTouching = false;
    PetRoomState roomState = PET_ROOM_ACTIVE;

    unsigned long nextWalkDecisionMs = 0;
    unsigned long nextHungerTickMs = 0;
    unsigned long nextHappinessTickMs = 0;
    unsigned long nextHealthTickMs = 0;
    unsigned long nextSootSpawnMs = 0;

    int lastHudHealth = -1;
    int lastHudHunger = -1;
    int lastHudHappiness = -1;
    int lastHudCleanness = -1;

    Avatar *endMessageAvatars[MAX_END_MESSAGE_GLYPHS];
    int endMessageAvatarCount = 0;
    bool endMessageBuilt = false;

    void initSootPool() {
      SpriteSheet sheet(sprite_soot, sprite_sootMask, SPRITE_SOOT_WIDTH, SPRITE_SOOT_HEIGHT);
      for (int i = 0; i < MAX_SOOT; i++) {
        sootSlots[i].variant = 0;
        sootSlots[i].active = false;
        sootSlots[i].avatar = sheet.createAvatar(-40, SCREENHEIGHT + 20,
                                                 SpriteSheet::readRegion(sprite_sootRegions, 0));
        appendAvatar(sootSlots[i].avatar);
      }
    }

    void clampTotoro() {
      if (totoro == NULL) {
        return;
      }
      if (totoro->x < PET_WALK_MIN_X) {
        totoro->x = PET_WALK_MIN_X;
        totoro->velocity.x = fabs(totoro->velocity.x);
        totoro->requestRedraw();
      }
      if (totoro->x + totoro->width > PET_WALK_MAX_X) {
        totoro->x = PET_WALK_MAX_X - totoro->width;
        totoro->velocity.x = -fabs(totoro->velocity.x);
        totoro->requestRedraw();
      }
      if (totoro->y < PET_WALK_MIN_Y) {
        totoro->y = PET_WALK_MIN_Y;
      }
      if (totoro->y > PET_WALK_MAX_Y) {
        totoro->y = PET_WALK_MAX_Y;
      }
    }

    void chooseNewWalk(unsigned long now) {
      nextWalkDecisionMs = now + PET_WALK_DECISION_MIN_MS + random(0, PET_WALK_DECISION_MAX_MS - PET_WALK_DECISION_MIN_MS);
      if (totoro == NULL) {
        return;
      }

      int action = random(0, 5);
      if (action == 0) {
        totoro->setVelocity(0, 0);
      } else if (action <= 2) {
        totoro->setVelocity(-PET_WALK_SPEED, 0);
      } else {
        totoro->setVelocity(PET_WALK_SPEED, 0);
      }
      totoro->requestRedraw();
    }

    void updateWalk(unsigned long now) {
      if (totoro == NULL || roomState != PET_ROOM_ACTIVE) {
        return;
      }
      if (now >= nextWalkDecisionMs) {
        chooseNewWalk(now);
      }
    }

    void tickStats(unsigned long now) {
      if (roomState != PET_ROOM_ACTIVE) {
        return;
      }

      if (now >= nextHungerTickMs) {
        nextHungerTickMs = now + PET_HUNGER_TICK_MS;
        PetTotoroState::adjustHunger(-1);
        addSound(NOTE_A3, noteDurationMs(32, 700));
      }

      if (now >= nextHappinessTickMs) {
        nextHappinessTickMs = now + PET_HAPPINESS_TICK_MS;
        PetTotoroState::adjustHappiness(-1);
      }

      if (now >= nextHealthTickMs) {
        nextHealthTickMs = now + PET_HEALTH_TICK_MS;
        const PetTotoroStats &stats = PetTotoroState::stats();
        if (stats.hunger <= PET_STAT_MIN || stats.cleanness <= PET_STAT_MIN) {
          PetTotoroState::adjustHealth(-1);
          addSound(NOTE_G3, noteDurationMs(24, 600));
        }
      }
    }

    int activeSootCount() const {
      int count = 0;
      for (int i = 0; i < MAX_SOOT; i++) {
        if (sootSlots[i].active) {
          count++;
        }
      }
      return count;
    }

    void updateSoot(unsigned long now) {
      if (roomState != PET_ROOM_ACTIVE) {
        return;
      }
      if (now < nextSootSpawnMs || activeSootCount() >= MAX_SOOT) {
        return;
      }

      nextSootSpawnMs = now + PET_SOOT_SPAWN_MIN_MS + random(0, PET_SOOT_SPAWN_MAX_MS - PET_SOOT_SPAWN_MIN_MS);
      spawnSoot();
    }

    void spawnSoot() {
      for (int i = 0; i < MAX_SOOT; i++) {
        if (sootSlots[i].active) {
          continue;
        }

        int variant = random(0, PET_SOOT_VARIANT_COUNT);
        int16_t spawnX = PET_WALK_MIN_X + random(0, PET_WALK_MAX_X - PET_WALK_MIN_X - SPRITE_SOOT_WIDTH);
        int16_t spawnY = PET_WALK_MIN_Y + random(0, PET_WALK_MAX_Y - PET_WALK_MIN_Y - SPRITE_SOOT_HEIGHT);

        SpriteSheet sheet(sprite_soot, sprite_sootMask, SPRITE_SOOT_WIDTH, SPRITE_SOOT_HEIGHT);
        sheet.applyRegion(sootSlots[i].avatar, SpriteSheet::readRegion(sprite_sootRegions, variant));
        sootSlots[i].avatar->setPos(spawnX, spawnY);
        sootSlots[i].avatar->setVelocity(0, 0);
        sootSlots[i].avatar->requestRedraw();
        sootSlots[i].active = true;
        sootSlots[i].variant = variant;

        PetTotoroState::adjustCleanness(-1);
        addSound(NOTE_C4, noteDurationMs(32, 700));
        requestRender();
        return;
      }
    }

    bool tryCleanSoot(uint16_t touchX, uint16_t touchY) {
      for (int i = 0; i < MAX_SOOT; i++) {
        if (!sootSlots[i].active || sootSlots[i].avatar == NULL) {
          continue;
        }
        if (!sootSlots[i].avatar->contains(touchX, touchY)) {
          continue;
        }

        sootSlots[i].active = false;
        sootSlots[i].avatar->setPos(-40, SCREENHEIGHT + 20);
        sootSlots[i].avatar->requestRedraw();
        PetTotoroState::adjustCleanness(1);
        addSound(NOTE_E5, noteDurationMs(20, 900));
        requestRender();
        return true;
      }
      return false;
    }

    void triggerGameOver() {
      if (roomState == PET_ROOM_GAME_OVER) {
        return;
      }
      roomState = PET_ROOM_GAME_OVER;
      PetTotoroState::markDead();
      if (totoro != NULL) {
        totoro->setVelocity(0, 0);
      }
      showEndMessage("GAME OVER", "HOME");
      addSound(NOTE_G3, noteDurationMs(8, 500));
      addSound(NOTE_E3, noteDurationMs(8, 500));
      requestRender();
    }

    void drawMeter(int16_t x, int16_t y, const char *label, int value) {
      _tft->setTextColor(TFT_WHITE, rgb565(20, 24, 20));
      _tft->setTextDatum(TL_DATUM);
      _tft->drawString(label, x, y, 1);

      int16_t barX = x;
      int16_t barY = y + 12;
      for (int i = 0; i < PET_STAT_MAX; i++) {
        uint16_t color = (i < value) ? rgb565(250, 210, 70) : rgb565(55, 58, 52);
        _tft->fillRect(barX + (i * 12), barY, 10, 8, color);
      }
    }

    void drawStatsHud() {
      const PetTotoroStats &stats = PetTotoroState::stats();
      if (stats.health == lastHudHealth && stats.hunger == lastHudHunger &&
          stats.happiness == lastHudHappiness && stats.cleanness == lastHudCleanness) {
        // Still redraw each frame because avatars move underneath the HUD strip.
      }

      _tft->fillRect(0, 0, SCREENWIDTH, PET_HUD_HEIGHT, rgb565(20, 24, 20));
      drawMeter(6, 4, "HL", stats.health);
      drawMeter(66, 4, "HU", stats.hunger);
      drawMeter(126, 4, "HA", stats.happiness);
      drawMeter(186, 4, "CL", stats.cleanness);

      lastHudHealth = stats.health;
      lastHudHunger = stats.hunger;
      lastHudHappiness = stats.happiness;
      lastHudCleanness = stats.cleanness;
    }

    void clearEndMessageAvatars() {
      for (int i = 0; i < endMessageAvatarCount; i++) {
        endMessageAvatars[i] = NULL;
      }
      endMessageAvatarCount = 0;
      endMessageBuilt = false;
    }

    void showEndMessage(const char *line1, const char *line2 = "") {
      if (endMessageBuilt) {
        return;
      }
      endMessageBuilt = true;
      endMessageAvatarCount = 0;

      Avatar *glyphs[MAX_END_MESSAGE_GLYPHS];
      if (line1 != NULL && line1[0] != '\0') {
        int count = SpriteText::buildCenteredLine(this, line1, END_MESSAGE_LINE1_Y, glyphs,
                                                  MAX_END_MESSAGE_GLYPHS, END_MESSAGE_GAP);
        for (int i = 0; i < count; i++) {
          endMessageAvatars[endMessageAvatarCount++] = glyphs[i];
        }
      }

      if (line2 != NULL && line2[0] != '\0') {
        int count = SpriteText::buildCenteredLine(this, line2, END_MESSAGE_LINE2_Y, glyphs,
                                                  MAX_END_MESSAGE_GLYPHS - endMessageAvatarCount, END_MESSAGE_GAP);
        for (int i = 0; i < count; i++) {
          endMessageAvatars[endMessageAvatarCount++] = glyphs[i];
        }
      }

      renderFullScreen();
    }
};

#endif
