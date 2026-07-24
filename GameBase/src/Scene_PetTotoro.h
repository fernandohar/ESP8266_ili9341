#ifndef _SCENE_PETTOTORO_H_
#define _SCENE_PETTOTORO_H_

#include <Arduino.h>
#include "GameScene.h"
#include "PetTotoroState.h"
#include "Input.h"
#include "SpriteSheet.h"
#include "SpriteText.h"
#include "TouchInput.h"
#include "image_acorn_catch_bg.h"
#include "sprite_totoro_baby.h"
#include "sprite_totoro_junior.h"
#include "sprite_totoro_adult.h"
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
// Totoro sprite-sheet region order (shared by baby/junior/adult sheets).
#define TOTORO_RGN_WALK_A 0
#define TOTORO_RGN_WALK_B 1
#define TOTORO_RGN_SIT 2
#define TOTORO_RGN_STAND 3
#define TOTORO_RGN_SLEEP 4
// Posture cycling: pick a new random pose every few seconds; while walking,
// alternate the two walk frames at this cadence.
#define PET_POSE_MIN_MS 2500
#define PET_POSE_MAX_MS 5000
#define PET_WALK_FRAME_MS 220

// Party mode: baby, junior and adult Totoros share the room. Each is bottom-
// aligned to a common ground line so the different sizes stand on the same floor.
#define PET_COUNT 3
#define PET_GROUND_Y 300

enum TotoroPose {
  TOTORO_POSE_WALK,
  TOTORO_POSE_SIT,
  TOTORO_POSE_STAND,
  TOTORO_POSE_SLEEP,
  TOTORO_POSE_COUNT
};

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
      updatePose(now);
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

      for (int i = 0; i < PET_COUNT; i++) {
        if (pets[i].avatar != NULL) {
          pets[i].avatar->updatePos(now);
          clampPet(pets[i]);
        }
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
      setBackground(acorn_catch_bg);
      drawBackground(acorn_catch_bg);

      if (PetTotoroState::isGameOver()) {
        roomState = PET_ROOM_GAME_OVER;
      } else {
        roomState = PET_ROOM_ACTIVE;
      }

      setupPet(pets[0], sprite_totoro_baby, sprite_totoro_babyMask,
               SPRITE_TOTORO_BABY_WIDTH, SPRITE_TOTORO_BABY_HEIGHT,
               sprite_totoro_babyRegions, 32, 32, 30);
      setupPet(pets[1], sprite_totoro_junior, sprite_totoro_juniorMask,
               SPRITE_TOTORO_JUNIOR_WIDTH, SPRITE_TOTORO_JUNIOR_HEIGHT,
               sprite_totoro_juniorRegions, 44, 44, 150);
      setupPet(pets[2], sprite_totoro_adult, sprite_totoro_adultMask,
               SPRITE_TOTORO_ADULT_WIDTH, SPRITE_TOTORO_ADULT_HEIGHT,
               sprite_totoro_adultRegions, 88, 88, 76);

      initSootPool();
      clearEndMessageAvatars();
      wasTouching = false;
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
        for (int i = 0; i < PET_COUNT; i++) {
          chooseNewPose(pets[i], millis());
        }
      }

      renderFullScreen();
      if (roomState == PET_ROOM_ACTIVE) {
        drawStatsHud();
      }
      requestRender();
    }

    void destroyScene() {
      for (int i = 0; i < PET_COUNT; i++) {
        pets[i].avatar = NULL;
      }
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

    struct Pet {
      Avatar *avatar;
      const uint16_t *bitmap;
      const uint8_t *mask;
      uint16_t sheetW;
      uint16_t sheetH;
      const SpriteSheetRegion *regions;
      uint16_t frameW;
      uint16_t frameH;
      int pose;
      unsigned long nextPoseMs;
      unsigned long walkFrameMs;
      bool walkFrameB;
    };

    Pet pets[PET_COUNT];
    SootSlot sootSlots[MAX_SOOT];
    boolean wasTouching = false;
    PetRoomState roomState = PET_ROOM_ACTIVE;

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

    void setupPet(Pet &p, const uint16_t *bitmap, const uint8_t *mask,
                  uint16_t sheetW, uint16_t sheetH, const SpriteSheetRegion *regions,
                  uint16_t frameW, uint16_t frameH, int16_t startX) {
      p.bitmap = bitmap;
      p.mask = mask;
      p.sheetW = sheetW;
      p.sheetH = sheetH;
      p.regions = regions;
      p.frameW = frameW;
      p.frameH = frameH;
      p.pose = TOTORO_POSE_STAND;
      p.walkFrameB = false;
      p.walkFrameMs = millis();
      p.nextPoseMs = millis() + PET_POSE_MIN_MS + random(0, PET_POSE_MAX_MS - PET_POSE_MIN_MS);
      int16_t y = PET_GROUND_Y - (int16_t)frameH;  // bottom-align to shared ground line
      SpriteSheet sheet(bitmap, mask, sheetW, sheetH);
      p.avatar = sheet.createAvatar(startX, y, SpriteSheet::readRegion(regions, TOTORO_RGN_STAND));
      appendAvatar(p.avatar);
    }

    void clampPet(Pet &p) {
      if (p.avatar == NULL) {
        return;
      }
      if (p.avatar->x < PET_WALK_MIN_X) {
        p.avatar->x = PET_WALK_MIN_X;
        p.avatar->velocity.x = fabs(p.avatar->velocity.x);
        p.avatar->requestRedraw();
      }
      // Wider pets get less horizontal room; clamp against their own width.
      float maxX = PET_WALK_MAX_X - p.avatar->width;
      if (p.avatar->x > maxX) {
        p.avatar->x = maxX;
        p.avatar->velocity.x = -fabs(p.avatar->velocity.x);
        p.avatar->requestRedraw();
      }
    }

    void applyPoseFrame(Pet &p, int regionIndex) {
      if (p.avatar == NULL) {
        return;
      }
      SpriteSheet sheet(p.bitmap, p.mask, p.sheetW, p.sheetH);
      sheet.applyRegion(p.avatar, SpriteSheet::readRegion(p.regions, regionIndex));
      p.avatar->requestRedraw();
    }

    // Pick a random posture and hold it for a few seconds. Walking gets a random
    // left/right velocity and animates its two frames; the other poses sit still.
    void chooseNewPose(Pet &p, unsigned long now) {
      p.nextPoseMs = now + PET_POSE_MIN_MS + random(0, PET_POSE_MAX_MS - PET_POSE_MIN_MS);
      if (p.avatar == NULL) {
        return;
      }

      p.pose = random(0, TOTORO_POSE_COUNT);
      switch (p.pose) {
        case TOTORO_POSE_WALK: {
          float dir = (random(0, 2) == 0) ? -PET_WALK_SPEED : PET_WALK_SPEED;
          p.avatar->setVelocity(dir, 0);
          p.walkFrameB = false;
          p.walkFrameMs = now;
          applyPoseFrame(p, TOTORO_RGN_WALK_A);
          break;
        }
        case TOTORO_POSE_SIT:
          p.avatar->setVelocity(0, 0);
          applyPoseFrame(p, TOTORO_RGN_SIT);
          break;
        case TOTORO_POSE_SLEEP:
          p.avatar->setVelocity(0, 0);
          applyPoseFrame(p, TOTORO_RGN_SLEEP);
          break;
        case TOTORO_POSE_STAND:
        default:
          p.avatar->setVelocity(0, 0);
          applyPoseFrame(p, TOTORO_RGN_STAND);
          break;
      }
    }

    void updatePose(unsigned long now) {
      if (roomState != PET_ROOM_ACTIVE) {
        return;
      }
      for (int i = 0; i < PET_COUNT; i++) {
        Pet &p = pets[i];
        if (p.avatar == NULL) {
          continue;
        }
        if (now >= p.nextPoseMs) {
          chooseNewPose(p, now);
          continue;
        }
        if (p.pose == TOTORO_POSE_WALK && (now - p.walkFrameMs) >= PET_WALK_FRAME_MS) {
          p.walkFrameMs = now;
          p.walkFrameB = !p.walkFrameB;
          applyPoseFrame(p, p.walkFrameB ? TOTORO_RGN_WALK_B : TOTORO_RGN_WALK_A);
        }
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
      for (int i = 0; i < PET_COUNT; i++) {
        if (pets[i].avatar != NULL) {
          pets[i].avatar->setVelocity(0, 0);
        }
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
