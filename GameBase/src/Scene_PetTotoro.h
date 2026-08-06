#ifndef _SCENE_PETTOTORO_H_
#define _SCENE_PETTOTORO_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "PetTotoroState.h"
#include "GameProgress.h"
#include "GameResult.h"
#include "PendingMeal.h"
#include "PetSave.h"
#include "Input.h"
#include "SpriteSheet.h"
#include "Attachment.h"
#include "TouchInput.h"
#include "image_acorn_catch_bg.h"
#include "sprite_totoro_baby.h"
#include "sprite_totoro_junior.h"
#include "sprite_totoro_adult.h"
#include "sprite_soot.h"
#include "sprite_grocery_food.h"

#define PET_WALK_MIN_X 14
#define PET_WALK_MAX_X 206
#define PET_WALK_MIN_Y 212
#define PET_WALK_MAX_Y 272
#define PET_WALK_SPEED 1.4f
#define MAX_SOOT 8
#define PET_SOOT_VARIANT_COUNT 16

#define PET_HUNGER_TICK_MS 45000
#define PET_HAPPINESS_TICK_MS 60000
#define PET_HEALTH_TICK_MS 30000
#define PET_SOOT_SPAWN_MIN_MS 18000
#define PET_SOOT_SPAWN_MAX_MS 36000

// Fail-state / recovery tuning.
// Once health bottoms out the pet turns "sick"; if it stays that way for this
// long (of powered-on time) Totoro gives up and runs away. With an RTC wired
// this would instead be measured in real hours incl. time spent powered off.
#define PET_SICK_GRACE_MS 60000
// Health only regenerates while the pet is both well-fed and clean.
#define PET_HEALTH_REGEN_THRESHOLD 60
// Care XP awarded per soot cleaned (feeding/petting/game wins add more later).
#define PET_CARE_XP_CLEAN 10

// Totoro sprite-sheet region order (shared by baby/junior/adult sheets).
#define TOTORO_RGN_WALK_A 0
#define TOTORO_RGN_WALK_B 1
#define TOTORO_RGN_SIT 2
#define TOTORO_RGN_STAND 3
#define TOTORO_RGN_SLEEP 4
// Extra eyes-closed frame; only the baby sheet provides it (index 5).
#define TOTORO_RGN_BLINK 5
// Posture cycling: pick a new random pose every few seconds; while walking,
// alternate the two walk frames at this cadence.
#define PET_POSE_MIN_MS 2500
#define PET_POSE_MAX_MS 5000
#define PET_WALK_FRAME_MS 220
// Idle blink: while standing/sitting with eyes open, briefly shut the eyes.
// TEMP(blink test): fast cadence so the blink is easy to observe.
#define PET_BLINK_MIN_MS 700
#define PET_BLINK_MAX_MS 1500
#define PET_BLINK_DURATION_MS 150
// Production values (restore after confirming the blink works):
// #define PET_BLINK_MIN_MS 2200
// #define PET_BLINK_MAX_MS 5200
// #define PET_BLINK_DURATION_MS 150

// A single Totoro lives in the room, sized by its current growth stage and
// bottom-aligned to this ground line.
#define PET_GROUND_Y 300

// --- Eating animation (food handed off from the grocery via PendingMeal) ---
// Totoro sits still and the bought food, attached to it, steps through its
// new -> half -> eaten frames before the hunger/happiness effect is applied.
#define PET_EAT_PHASE_MS 750    // time on the "new" and "half" frames
#define PET_EAT_FINAL_MS 550    // time on the "eaten" frame
#define PET_EAT_CARE_XP 4

// --- Radial action menu (opens when you tap Totoro) ---
#define PET_MENU_ITEMS 6
#define PET_MENU_CENTER_X 120
#define PET_MENU_CENTER_Y 184
#define PET_MENU_RING_RADIUS 74
#define PET_MENU_ICON_RADIUS 24
#define PET_MENU_PANEL_RADIUS 106
#define PET_MENU_CLOSE_RADIUS 22
// Generous padding around the sprite so small stages are still easy to tap.
#define PET_TAP_PADDING 12

// --- Free / in-place care actions ---
#define PET_PET_HAPPINESS 6
#define PET_PET_MAX_SESSION 3
#define PET_PET_COOLDOWN_MS 60000
#define PET_CARE_XP_PET 2
#define PET_CARE_XP_BATHE 5

// --- Mini-game reward hand-off (applied on return to the home) ---
#define GAME_WIN_HAPPINESS 12
#define GAME_LOSS_HAPPINESS 4
#define GAME_WIN_CARE_XP 15
#define GAME_LOSS_CARE_XP 5
#define GAME_WIN_DEFAULT_COINS 5
#define GAME_LOSS_CONSOLATION_COINS 2
#define PET_REWARD_TOAST_MS 2200

enum PetMenuItem {
  PET_MENU_PLAY = 0,
  PET_MENU_EAT,
  PET_MENU_PET,
  PET_MENU_BATHE,
  PET_MENU_STATUS,
  PET_MENU_SETTINGS
};

enum TotoroPose {
  TOTORO_POSE_WALK,
  TOTORO_POSE_SIT,
  TOTORO_POSE_STAND,
  TOTORO_POSE_SLEEP,
  TOTORO_POSE_COUNT
};

enum PetRoomState {
  PET_ROOM_ACTIVE,
  PET_ROOM_ESCAPED
};

class Scene_PetTotoro : public GameScene {
  public:
    Scene_PetTotoro(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (roomState == PET_ROOM_ESCAPED) {
        // Totoro is gone until a factory reset. Home jumps straight to Settings
        // (where Factory Reset lives) without reviving it.
        if (input.homePressed) {
          *needChangeScene = true;
          *nextSceneIndex = SCENE_SETTINGS;
        }
        return;
      }

      // Eating is a brief, non-interruptible animation: just advance it.
      if (eating) {
        updateEating(now);
        wasTouching = isTouching;
        requestRender();
        return;
      }

      // Radial menu is modal: it overlays the frozen room and swallows input
      // until an item is picked or it is dismissed.
      if (menuOpen) {
        if (input.homePressed) {
          closeOverlays();
          wasTouching = isTouching;
          return;
        }
        if (isTouching && !wasTouching) {
          uint16_t touchX = 0;
          uint16_t touchY = 0;
          if (getTouchPoint(_tft, &touchX, &touchY)) {
            handleMenuTouch(touchX, touchY, needChangeScene, nextSceneIndex);
          }
        }
        wasTouching = isTouching;
        return;
      }

      // Play sub-menu (game picker) is likewise modal; Home backs out to the
      // radial menu.
      if (playOpen) {
        if (input.homePressed) {
          openMenu();
          wasTouching = isTouching;
          return;
        }
        if (isTouching && !wasTouching) {
          uint16_t touchX = 0;
          uint16_t touchY = 0;
          if (getTouchPoint(_tft, &touchX, &touchY)) {
            handlePlayTouch(touchX, touchY, needChangeScene, nextSceneIndex);
          }
        }
        wasTouching = isTouching;
        return;
      }

      // In the pet's home, Home opens the radial action menu (there is no hub).
      if (input.homePressed) {
        openMenu();
        wasTouching = isTouching;
        return;
      }

      // Clear the mini-game reward toast once it has been shown for a moment.
      if (rewardToastUntilMs != 0 && now >= rewardToastUntilMs) {
        rewardToastUntilMs = 0;
        repaintRoom();
      }

      // Clear the anniversary love-note bubble after it lingers a moment.
      if (speechUntilMs != 0 && now >= speechUntilMs) {
        speechUntilMs = 0;
        speechText[0] = '\0';
        repaintRoom();
      }

      tickStats(now);
      updatePose(now);
      updateBlink(now);
      updateSoot(now);
      refillPetSession(now);

      if (isTouching && !wasTouching) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          if (tryCleanSoot(touchX, touchY)) {
            wasTouching = isTouching;
            requestRender();
            return;
          }
          if (tapOnPet(touchX, touchY)) {
            openMenu();
            wasTouching = isTouching;
            return;
          }
        }
      }

      Pet &pet = pets[0];
      if (pet.avatar != NULL) {
        pet.avatar->updatePos(now);
        clampPet(pet);
      }

      updateLifeState(now);

      wasTouching = isTouching;
      requestRender();
    }

    void render() {
      if (roomState == PET_ROOM_ESCAPED || menuOpen || playOpen) {
        return;  // static screens (note / menu / play picker) drawn once on entry
      }
      renderScene();
      if (PetTotoroState::isSick()) {
        drawSickBanner();
      }
    }

    void initScene() {
      setBackgroundAsset(&acorn_catch_bg);
      drawBackgroundAsset(&acorn_catch_bg);

      pets[0].avatar = NULL;
      menuOpen = false;
      playOpen = false;
      petSession = PET_PET_MAX_SESSION;
      petCooldownUntilMs = 0;
      rewardToastUntilMs = 0;
      speechUntilMs = 0;
      speechText[0] = '\0';

      // TEMP(anniversary): keep Totoro alive & at home no matter its saved state.
      PetTotoroState::setLife(PET_LIFE_ALIVE);

      if (PetTotoroState::hasEscaped()) {
        roomState = PET_ROOM_ESCAPED;
        wasTouching = false;
        drawEscapedScreen();
        requestRender();
        return;
      }

      roomState = PET_ROOM_ACTIVE;
      // Apply any pending mini-game reward before choosing the sprite, so a
      // stage-up from the earned care-XP is reflected immediately.
      applyGameReward();
      setupStagePet();
      initSootPool();

      wasTouching = false;
      sickSinceMs = 0;
      nextHungerTickMs = millis() + PET_HUNGER_TICK_MS;
      nextHappinessTickMs = millis() + PET_HAPPINESS_TICK_MS;
      nextHealthTickMs = millis() + PET_HEALTH_TICK_MS;
      nextSootSpawnMs = millis() + PET_SOOT_SPAWN_MIN_MS;

      if (PetTotoroState::isSick()) {
        // Resumed into a sick pet: hold it still and start the grace clock now.
        sickSinceMs = millis();
        if (pets[0].avatar != NULL) {
          pets[0].avatar->setVelocity(0, 0);
        }
        pets[0].pose = TOTORO_POSE_SIT;
        applyPoseFrame(pets[0], TOTORO_RGN_SIT);
      } else {
        chooseNewPose(pets[0], millis());
      }

      // A food bought in the grocery is eaten here, with an animation, before
      // its effect lands.
      if (PendingMeal::pending()) {
        startEating(millis());
      }

      renderFullScreen();
      if (PetTotoroState::isSick()) {
        drawSickBanner();
      }
      if (rewardToastUntilMs != 0) {
        drawRewardToast();
      }
      requestRender();
    }

    void destroyScene() {
      pets[0].avatar = NULL;
      for (int i = 0; i < MAX_SOOT; i++) {
        sootSlots[i].avatar = NULL;
        sootSlots[i].active = false;
      }
      // GameScene::destroyScene() frees every avatar (incl. the food
      // attachment); just drop our cached pointer/flag.
      eating = false;
      foodAttach = NULL;
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
      const SpriteAsset *asset;
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
      bool hasBlink;
      bool blinking;
      unsigned long nextBlinkMs;
      unsigned long blinkEndMs;
    };

    Pet pets[1];
    SootSlot sootSlots[MAX_SOOT];
    boolean wasTouching = false;
    PetRoomState roomState = PET_ROOM_ACTIVE;
    unsigned long sickSinceMs = 0;

    bool menuOpen = false;
    bool playOpen = false;
    int petSession = PET_PET_MAX_SESSION;
    unsigned long petCooldownUntilMs = 0;

    // Eating animation state (a food bought in the grocery is attached to the
    // Totoro avatar and chewed through 3 frames before its effect is applied).
    bool eating = false;
    int eatPhase = 0;
    unsigned long eatPhaseUntilMs = 0;
    Attachment *foodAttach = NULL;
    int mealHunger = 0;
    int mealHappiness = 0;
    int mealRegion[3] = {0, 0, 0};

    char rewardToast[24] = {0};
    unsigned long rewardToastUntilMs = 0;

    // Anniversary love note shown in a speech bubble when Totoro is petted.
    char speechText[28] = {0};
    unsigned long speechUntilMs = 0;

    unsigned long nextHungerTickMs = 0;
    unsigned long nextHappinessTickMs = 0;
    unsigned long nextHealthTickMs = 0;
    unsigned long nextSootSpawnMs = 0;

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

    // Build the single Totoro from the sprite sheet that matches its stage.
    void setupStagePet() {
      switch (PetTotoroState::stage()) {
        case PET_STAGE_ADULT:
          setupPet(pets[0], sprite_totoro_adult, sprite_totoro_adultMask,
                   SPRITE_TOTORO_ADULT_WIDTH, SPRITE_TOTORO_ADULT_HEIGHT,
                   sprite_totoro_adultRegions, 88, 88);
          break;
        case PET_STAGE_JUNIOR:
          setupPet(pets[0], sprite_totoro_junior, sprite_totoro_juniorMask,
                   SPRITE_TOTORO_JUNIOR_WIDTH, SPRITE_TOTORO_JUNIOR_HEIGHT,
                   sprite_totoro_juniorRegions, 44, 44);
          break;
        case PET_STAGE_BABY:
        default:
          setupPet(pets[0], &sprite_totoro_baby, SPRITE_TOTORO_BABY_SHEET_WIDTH,
                   SPRITE_TOTORO_BABY_SHEET_HEIGHT, sprite_totoro_babyRegions, 75, 81);
          pets[0].hasBlink = true;  // baby sheet has an eyes-closed frame
          break;
      }
    }

    void setupPet(Pet &p, const SpriteAsset *asset, uint16_t sheetW, uint16_t sheetH,
                  const SpriteSheetRegion *regions, uint16_t frameW, uint16_t frameH) {
      p.asset = asset;
      p.bitmap = NULL;
      p.mask = NULL;
      p.sheetW = sheetW;
      p.sheetH = sheetH;
      p.regions = regions;
      p.frameW = frameW;
      p.frameH = frameH;
      p.pose = TOTORO_POSE_STAND;
      p.walkFrameB = false;
      p.walkFrameMs = millis();
      p.nextPoseMs = millis() + PET_POSE_MIN_MS + random(0, PET_POSE_MAX_MS - PET_POSE_MIN_MS);
      p.hasBlink = false;
      p.blinking = false;
      p.nextBlinkMs = millis() + PET_BLINK_MIN_MS + random(0, PET_BLINK_MAX_MS - PET_BLINK_MIN_MS);
      p.blinkEndMs = 0;
      int16_t x = (SCREENWIDTH - (int16_t)frameW) / 2;
      int16_t y = PET_GROUND_Y - (int16_t)frameH;
      SpriteSheet sheet(asset);
      p.avatar = sheet.createAvatar(x, y, SpriteSheet::readRegion(regions, TOTORO_RGN_STAND));
      appendAvatar(p.avatar);
    }

    void setupPet(Pet &p, const uint16_t *bitmap, const uint8_t *mask,
                  uint16_t sheetW, uint16_t sheetH, const SpriteSheetRegion *regions,
                  uint16_t frameW, uint16_t frameH) {
      p.asset = NULL;
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
      p.hasBlink = false;
      p.blinking = false;
      p.nextBlinkMs = millis() + PET_BLINK_MIN_MS + random(0, PET_BLINK_MAX_MS - PET_BLINK_MIN_MS);
      p.blinkEndMs = 0;
      int16_t x = (SCREENWIDTH - (int16_t)frameW) / 2;
      int16_t y = PET_GROUND_Y - (int16_t)frameH;  // bottom-align to the ground line
      SpriteSheet sheet(bitmap, mask, sheetW, sheetH);
      p.avatar = sheet.createAvatar(x, y, SpriteSheet::readRegion(regions, TOTORO_RGN_STAND));
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
      if (p.asset != NULL) {
        SpriteSheet sheet(p.asset);
        sheet.applyRegion(p.avatar, SpriteSheet::readRegion(p.regions, regionIndex));
      } else {
        SpriteSheet sheet(p.bitmap, p.mask, p.sheetW, p.sheetH);
        sheet.applyRegion(p.avatar, SpriteSheet::readRegion(p.regions, regionIndex));
      }
      p.avatar->requestRedraw();
    }

    // ---- eating animation --------------------------------------------------

    void applyFoodFrame(int frame) {
      if (foodAttach == NULL) {
        return;
      }
      SpriteSheet food(sprite_grocery_food, sprite_grocery_foodMask,
                       SPRITE_GROCERY_FOOD_WIDTH, SPRITE_GROCERY_FOOD_HEIGHT);
      food.applyRegion(foodAttach,
                       SpriteSheet::readRegion(sprite_grocery_foodRegions, mealRegion[frame]));
      foodAttach->requestRedraw();
    }

    // Sit Totoro down and attach the just-bought food to it, starting on the
    // "new" frame. Consumes the PendingMeal (its effect lands in finishEating).
    void startEating(unsigned long now) {
      Pet &p = pets[0];
      if (p.avatar == NULL) {
        PendingMeal::clear();
        return;
      }

      mealHunger = PendingMeal::hunger();
      mealHappiness = PendingMeal::happiness();
      for (int i = 0; i < 3; i++) {
        mealRegion[i] = PendingMeal::region(i);
      }
      PendingMeal::clear();

      // Hold still, sitting, while eating.
      p.pose = TOTORO_POSE_SIT;
      p.avatar->setVelocity(0, 0);
      applyPoseFrame(p, TOTORO_RGN_SIT);

      SpriteSheetRegion r = SpriteSheet::readRegion(sprite_grocery_foodRegions, mealRegion[0]);
      // Hold the food low-centre (belly/paws), just above the ground.
      int16_t ax = ((int16_t)p.frameW - (int16_t)r.width) / 2;
      int16_t ay = (int16_t)p.frameH - (int16_t)r.height - 6;
      foodAttach = new Attachment(ax, ay, p.avatar, r.width, r.height,
                                  sprite_grocery_food, sprite_grocery_foodMask);
      applyFoodFrame(0);
      foodAttach->updatePos(now);
      appendAvatar(foodAttach);  // appended last -> drawn in front of Totoro

      eating = true;
      eatPhase = 0;
      eatPhaseUntilMs = now + PET_EAT_PHASE_MS;
      addSound(NOTE_E4, noteDurationMs(16, 700));
    }

    void updateEating(unsigned long now) {
      if (now >= eatPhaseUntilMs) {
        eatPhase++;
        if (eatPhase >= 3) {
          finishEating(now);
          return;
        }
        applyFoodFrame(eatPhase);  // half, then eaten
        eatPhaseUntilMs = now + (eatPhase == 2 ? PET_EAT_FINAL_MS : PET_EAT_PHASE_MS);
        addSound(NOTE_C4, noteDurationMs(16, 700));  // a little "nom"
      }
      if (foodAttach != NULL) {
        foodAttach->updatePos(now);
      }
    }

    void finishEating(unsigned long now) {
      eating = false;
      // Park the empty food off-screen so renderScene erases it (it is freed on
      // destroyScene); keep the pointer valid until then.
      if (foodAttach != NULL) {
        foodAttach->x = -100;
        foodAttach->y = SCREENHEIGHT + 40;
        foodAttach->requestRedraw();
      }

      PetTotoroState::adjustHunger(mealHunger);
      PetTotoroState::adjustHappiness(mealHappiness);
      PetTotoroState::addCareXP(PET_EAT_CARE_XP);
      PetSave::save();

      snprintf(rewardToast, sizeof(rewardToast), "Yum! +%d", mealHunger);
      rewardToastUntilMs = now + PET_REWARD_TOAST_MS;

      chooseNewPose(pets[0], now);  // resume wandering
      renderScene(true);            // clear the food + repaint the pet
      drawRewardToast();
      addSound(NOTE_G5, noteDurationMs(10, 900));
      addSound(NOTE_C6, noteDurationMs(10, 900));
    }

    // Pick a random posture and hold it for a few seconds. Walking gets a random
    // left/right velocity and animates its two frames; the other poses sit still.
    void chooseNewPose(Pet &p, unsigned long now) {
      p.nextPoseMs = now + PET_POSE_MIN_MS + random(0, PET_POSE_MAX_MS - PET_POSE_MIN_MS);
      p.blinking = false;
      p.nextBlinkMs = now + PET_BLINK_MIN_MS + random(0, PET_BLINK_MAX_MS - PET_BLINK_MIN_MS);
      if (p.avatar == NULL) {
        return;
      }

      // Wander, sit, or stand. (Sleep stays disabled for now.)
      int pick = random(0, 3);
      p.pose = (pick == 0) ? TOTORO_POSE_WALK
             : (pick == 1) ? TOTORO_POSE_SIT
                           : TOTORO_POSE_STAND;
      // Ambient mood cue: a low / unhappy Totoro would rather rest than wander.
      const PetTotoroStats &mood = PetTotoroState::stats();
      if ((mood.happiness < PET_STAT_PER_PIP || mood.health < PET_STAT_PER_PIP) &&
          p.pose == TOTORO_POSE_WALK) {
        p.pose = TOTORO_POSE_SIT;
      }
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
        // case TOTORO_POSE_SLEEP:
        //   p.avatar->setVelocity(0, 0);
        //   applyPoseFrame(p, TOTORO_RGN_SLEEP);
        //   break;
        case TOTORO_POSE_STAND:
        default:
          p.avatar->setVelocity(0, 0);
          applyPoseFrame(p, TOTORO_RGN_STAND);
          break;
      }
    }

    void updatePose(unsigned long now) {
      if (roomState != PET_ROOM_ACTIVE || PetTotoroState::isSick()) {
        return;  // a sick pet just sits there listlessly
      }
      Pet &p = pets[0];
      if (p.avatar == NULL) {
        return;
      }
      if (now >= p.nextPoseMs) {
        chooseNewPose(p, now);
        return;
      }
      if (p.pose == TOTORO_POSE_WALK && (now - p.walkFrameMs) >= PET_WALK_FRAME_MS) {
        p.walkFrameMs = now;
        p.walkFrameB = !p.walkFrameB;
        applyPoseFrame(p, p.walkFrameB ? TOTORO_RGN_WALK_B : TOTORO_RGN_WALK_A);
      }
    }

    // Region that a given pose rests on (used to restore eyes after a blink).
    int poseBaseRegion(int pose) {
      switch (pose) {
        case TOTORO_POSE_WALK:  return TOTORO_RGN_WALK_A;
        case TOTORO_POSE_SIT:   return TOTORO_RGN_SIT;
        case TOTORO_POSE_SLEEP: return TOTORO_RGN_SLEEP;
        case TOTORO_POSE_STAND:
        default:                return TOTORO_RGN_STAND;
      }
    }

    // Periodically shut the baby's eyes for a beat while it idles with them open.
    // Sleeping already shows the closed-eye frame, and walking/sick states skip it.
    void updateBlink(unsigned long now) {
      if (roomState != PET_ROOM_ACTIVE || PetTotoroState::isSick()) {
        return;
      }
      Pet &p = pets[0];
      if (!p.hasBlink || p.avatar == NULL) {
        return;
      }
      bool idleOpen = (p.pose == TOTORO_POSE_STAND || p.pose == TOTORO_POSE_SIT);
      if (p.blinking) {
        if (now >= p.blinkEndMs) {
          p.blinking = false;
          applyPoseFrame(p, poseBaseRegion(p.pose));
          p.nextBlinkMs = now + PET_BLINK_MIN_MS + random(0, PET_BLINK_MAX_MS - PET_BLINK_MIN_MS);
        }
        return;
      }
      if (!idleOpen) {
        p.nextBlinkMs = now + PET_BLINK_MIN_MS + random(0, PET_BLINK_MAX_MS - PET_BLINK_MIN_MS);
        return;
      }
      if (now >= p.nextBlinkMs) {
        p.blinking = true;
        p.blinkEndMs = now + PET_BLINK_DURATION_MS;
        applyPoseFrame(p, TOTORO_RGN_BLINK);
      }
    }

    void tickStats(unsigned long now) {
      if (roomState != PET_ROOM_ACTIVE) {
        return;
      }

      if (now >= nextHungerTickMs) {
        nextHungerTickMs = now + PET_HUNGER_TICK_MS;
        PetTotoroState::adjustHunger(-PET_STAT_PER_PIP);
        addSound(NOTE_A3, noteDurationMs(32, 700));
      }

      if (now >= nextHappinessTickMs) {
        nextHappinessTickMs = now + PET_HAPPINESS_TICK_MS;
        PetTotoroState::adjustHappiness(-PET_STAT_PER_PIP);
      }

      if (now >= nextHealthTickMs) {
        nextHealthTickMs = now + PET_HEALTH_TICK_MS;
        const PetTotoroStats &stats = PetTotoroState::stats();
        if (stats.hunger <= PET_STAT_MIN || stats.cleanness <= PET_STAT_MIN) {
          PetTotoroState::adjustHealth(-PET_STAT_PER_PIP);
          addSound(NOTE_G3, noteDurationMs(24, 600));
        } else if (stats.hunger >= PET_HEALTH_REGEN_THRESHOLD &&
                   stats.cleanness >= PET_HEALTH_REGEN_THRESHOLD &&
                   stats.health < PET_STAT_MAX) {
          PetTotoroState::adjustHealth(PET_STAT_PER_PIP);
        }
      }
    }

    // Drive the alive -> sick -> escaped fail-state (and sick -> alive recovery).
    void updateLifeState(unsigned long now) {
      return;  // TEMP(anniversary): Totoro can't get sick or run away for now.
      const PetTotoroStats &stats = PetTotoroState::stats();
      if (stats.health <= PET_STAT_MIN) {
        if (!PetTotoroState::isSick()) {
          enterSick(now);
        } else if (now - sickSinceMs >= PET_SICK_GRACE_MS) {
          triggerEscape();
        }
      } else if (PetTotoroState::isSick()) {
        PetTotoroState::setLife(PET_LIFE_ALIVE);
        addSound(NOTE_E5, noteDurationMs(16, 900));
        renderFullScreen();  // clear the sick banner cleanly
        chooseNewPose(pets[0], now);
      }
    }

    void enterSick(unsigned long now) {
      PetTotoroState::setLife(PET_LIFE_SICK);
      sickSinceMs = now;
      if (pets[0].avatar != NULL) {
        pets[0].avatar->setVelocity(0, 0);
      }
      pets[0].pose = TOTORO_POSE_SIT;
      applyPoseFrame(pets[0], TOTORO_RGN_SIT);
      addSound(NOTE_G3, noteDurationMs(10, 500));
      addSound(NOTE_E3, noteDurationMs(10, 500));
      requestRender();
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
      return;  // TEMP(anniversary): no pooping/soot while celebrating.
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

        PetTotoroState::adjustCleanness(-PET_STAT_PER_PIP);
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
        PetTotoroState::adjustCleanness(PET_STAT_PER_PIP);
        PetTotoroState::addCareXP(PET_CARE_XP_CLEAN);
        addSound(NOTE_E5, noteDurationMs(20, 900));
        requestRender();
        return true;
      }
      return false;
    }

    void triggerEscape() {
      if (roomState == PET_ROOM_ESCAPED) {
        return;
      }
      roomState = PET_ROOM_ESCAPED;
      PetTotoroState::setLife(PET_LIFE_ESCAPED);
      if (pets[0].avatar != NULL) {
        pets[0].avatar->setVelocity(0, 0);
      }
      addSound(NOTE_G3, noteDurationMs(8, 500));
      addSound(NOTE_E3, noteDurationMs(8, 500));
      addSound(NOTE_C3, noteDurationMs(8, 500));
      drawEscapedScreen();
      requestRender();
    }

    // ---- Radial action menu -------------------------------------------------

    bool tapOnPet(uint16_t x, uint16_t y) {
      Avatar *a = pets[0].avatar;
      if (a == NULL) {
        return false;
      }
      return (x >= a->x - PET_TAP_PADDING && x <= a->x + a->width + PET_TAP_PADDING &&
              y >= a->y - PET_TAP_PADDING && y <= a->y + a->height + PET_TAP_PADDING);
    }

    void refillPetSession(unsigned long now) {
      if (petSession == 0 && now >= petCooldownUntilMs) {
        petSession = PET_PET_MAX_SESSION;
      }
    }

    void getIconPos(int i, int16_t *ix, int16_t *iy) {
      float angle = (-90.0f + (float)i * (360.0f / PET_MENU_ITEMS)) * 0.01745329f;
      *ix = PET_MENU_CENTER_X + (int16_t)round(cos(angle) * PET_MENU_RING_RADIUS);
      *iy = PET_MENU_CENTER_Y + (int16_t)round(sin(angle) * PET_MENU_RING_RADIUS);
    }

    static bool within(uint16_t tx, uint16_t ty, int16_t cx, int16_t cy, int16_t r) {
      int32_t dx = (int32_t)tx - cx;
      int32_t dy = (int32_t)ty - cy;
      return (dx * dx + dy * dy) <= (int32_t)r * r;
    }

    const char *menuLabel(int i) {
      switch (i) {
        case PET_MENU_PLAY: return "Play";
        case PET_MENU_EAT: return "Eat";
        case PET_MENU_PET: return "Pet";
        case PET_MENU_BATHE: return "Bath";
        case PET_MENU_STATUS: return "Info";
        case PET_MENU_SETTINGS: return "Set";
      }
      return "";
    }

    uint16_t menuColor(int i) {
      switch (i) {
        case PET_MENU_PLAY: return rgb565(80, 150, 220);
        case PET_MENU_EAT: return rgb565(210, 150, 70);
        case PET_MENU_PET: return rgb565(220, 110, 150);
        case PET_MENU_BATHE: return rgb565(70, 190, 190);
        case PET_MENU_STATUS: return rgb565(140, 150, 165);
        case PET_MENU_SETTINGS: return rgb565(150, 165, 110);
      }
      return rgb565(120, 120, 120);
    }

    bool menuItemEnabled(int i) {
      (void)i;
      return true;
    }

    void drawMenuIcon(int i) {
      int16_t ix = 0, iy = 0;
      getIconPos(i, &ix, &iy);
      bool enabled = menuItemEnabled(i);
      uint16_t color = enabled ? menuColor(i) : rgb565(64, 68, 64);
      uint16_t textColor = enabled ? TFT_WHITE : rgb565(150, 155, 150);
      _tft->fillCircle(ix, iy, PET_MENU_ICON_RADIUS, color);
      _tft->drawCircle(ix, iy, PET_MENU_ICON_RADIUS, rgb565(18, 22, 18));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(textColor, color);
      _tft->drawString(menuLabel(i), ix, iy, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawMenu() {
      int16_t cx = PET_MENU_CENTER_X;
      int16_t cy = PET_MENU_CENTER_Y;

      _tft->fillCircle(cx, cy, PET_MENU_PANEL_RADIUS, rgb565(16, 20, 16));
      _tft->drawCircle(cx, cy, PET_MENU_PANEL_RADIUS, rgb565(90, 110, 90));
      _tft->drawCircle(cx, cy, PET_MENU_PANEL_RADIUS - 1, rgb565(58, 72, 58));

      for (int i = 0; i < PET_MENU_ITEMS; i++) {
        drawMenuIcon(i);
      }

      _tft->fillCircle(cx, cy, PET_MENU_CLOSE_RADIUS, rgb565(74, 42, 42));
      _tft->drawCircle(cx, cy, PET_MENU_CLOSE_RADIUS, rgb565(200, 120, 120));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, rgb565(74, 42, 42));
      _tft->drawString("X", cx, cy, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // Full repaint of the room beneath any overlay (background + pet).
    void repaintRoom() {
      renderFullScreen();
      if (PetTotoroState::isSick()) {
        drawSickBanner();
      }
    }

    void openMenu() {
      menuOpen = true;
      playOpen = false;
      if (pets[0].avatar != NULL) {
        pets[0].avatar->setVelocity(0, 0);
      }
      repaintRoom();
      drawMenu();
      addSound(NOTE_C5, noteDurationMs(24, 900));
    }

    void openPlayMenu() {
      playOpen = true;
      menuOpen = false;
      repaintRoom();
      drawPlayMenu();
      addSound(NOTE_C5, noteDurationMs(24, 900));
    }

    void closeOverlays() {
      menuOpen = false;
      playOpen = false;
      repaintRoom();
    }

    void handleMenuTouch(uint16_t tx, uint16_t ty, boolean *needChangeScene, int *nextSceneIndex) {
      if (within(tx, ty, PET_MENU_CENTER_X, PET_MENU_CENTER_Y, PET_MENU_CLOSE_RADIUS)) {
        closeOverlays();
        return;
      }
      for (int i = 0; i < PET_MENU_ITEMS; i++) {
        int16_t ix = 0, iy = 0;
        getIconPos(i, &ix, &iy);
        if (within(tx, ty, ix, iy, PET_MENU_ICON_RADIUS)) {
          activateMenuItem(i, needChangeScene, nextSceneIndex);
          return;
        }
      }
      closeOverlays();  // tapped an empty part of the panel
    }

    void activateMenuItem(int i, boolean *needChangeScene, int *nextSceneIndex) {
      (void)needChangeScene;
      (void)nextSceneIndex;
      switch (i) {
        case PET_MENU_PLAY:
          openPlayMenu();
          break;
        case PET_MENU_SETTINGS:
          addSound(NOTE_G5, noteDurationMs(8, 800));
          *needChangeScene = true;
          *nextSceneIndex = SCENE_SETTINGS;
          break;
        case PET_MENU_STATUS:
          addSound(NOTE_G5, noteDurationMs(8, 800));
          *needChangeScene = true;
          *nextSceneIndex = SCENE_STATUS;
          break;
        case PET_MENU_PET:
          doPet();
          closeOverlays();
          showLoveMessage();
          break;
        case PET_MENU_BATHE:
          doBathe();
          closeOverlays();
          break;
        case PET_MENU_EAT:
          addSound(NOTE_G5, noteDurationMs(8, 800));
          *needChangeScene = true;
          *nextSceneIndex = SCENE_GROCERY;
          break;
        default:
          // Unknown item: a soft blip acknowledges the tap and keeps the menu open.
          addSound(NOTE_A3, noteDurationMs(24, 600));
          break;
      }
    }

    // ---- Play sub-menu (game picker) ---------------------------------------

    static const int PET_PLAY_GAME_COUNT = 3;

    const char *playGameLabel(int i) {
      switch (i) {
        case 0: return "Acorn Catch";
        case 1: return "Tic-Tac-Toe";
        case 2: return "Whack-a-Mole";
      }
      return "";
    }

    int playGameScene(int i) {
      switch (i) {
        case 0: return SCENE_ACORN_CATCH;
        case 1: return SCENE_TIC_TAC_TOE;
        case 2: return SCENE_WHACK_A_MOLE;
      }
      return SCENE_PET_TOTORO;
    }

    uint16_t playGameColor(int i) {
      switch (i) {
        case 0: return rgb565(200, 150, 70);
        case 1: return rgb565(90, 160, 200);
        case 2: return rgb565(150, 120, 200);
      }
      return rgb565(120, 120, 120);
    }

    void playButtonRect(int i, int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
      *x = 40;
      *w = SCREENWIDTH - 80;
      *h = 40;
      *y = 116 + i * (*h + 12);
    }

    void backButtonRect(int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
      *w = 120;
      *h = 34;
      *x = (SCREENWIDTH - *w) / 2;
      *y = 262;
    }

    void drawPlayMenu() {
      int16_t px = 20, py = 66, pw = SCREENWIDTH - 40, ph = 236;
      _tft->fillRoundRect(px, py, pw, ph, 12, rgb565(18, 22, 18));
      _tft->drawRoundRect(px, py, pw, ph, 12, rgb565(90, 110, 90));

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(232, 232, 210), rgb565(18, 22, 18));
      _tft->drawString("Play", SCREENWIDTH / 2, py + 22, 4);

      for (int i = 0; i < PET_PLAY_GAME_COUNT; i++) {
        int16_t x, y, w, h;
        playButtonRect(i, &x, &y, &w, &h);
        uint16_t c = playGameColor(i);
        _tft->fillRoundRect(x, y, w, h, 8, c);
        _tft->drawRoundRect(x, y, w, h, 8, rgb565(20, 24, 20));
        _tft->setTextColor(TFT_WHITE, c);
        _tft->drawString(playGameLabel(i), SCREENWIDTH / 2, y + h / 2, 2);
      }

      int16_t bx, by, bw, bh;
      backButtonRect(&bx, &by, &bw, &bh);
      _tft->fillRoundRect(bx, by, bw, bh, 8, rgb565(70, 74, 70));
      _tft->drawRoundRect(bx, by, bw, bh, 8, rgb565(150, 155, 150));
      _tft->setTextColor(TFT_WHITE, rgb565(70, 74, 70));
      _tft->drawString("Back", SCREENWIDTH / 2, by + bh / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    static bool inRect(uint16_t tx, uint16_t ty, int16_t x, int16_t y, int16_t w, int16_t h) {
      return (tx >= x && tx < x + w && ty >= y && ty < y + h);
    }

    void handlePlayTouch(uint16_t tx, uint16_t ty, boolean *needChangeScene, int *nextSceneIndex) {
      for (int i = 0; i < PET_PLAY_GAME_COUNT; i++) {
        int16_t x, y, w, h;
        playButtonRect(i, &x, &y, &w, &h);
        if (inRect(tx, ty, x, y, w, h)) {
          addSound(NOTE_G5, noteDurationMs(8, 800));
          addSound(NOTE_C5, noteDurationMs(8, 800));
          *needChangeScene = true;
          *nextSceneIndex = playGameScene(i);
          return;
        }
      }

      int16_t bx, by, bw, bh;
      backButtonRect(&bx, &by, &bw, &bh);
      if (inRect(tx, ty, bx, by, bw, bh)) {
        openMenu();  // back to the radial menu
        return;
      }

      // Tap outside the panel dismisses to the room.
      int16_t px = 20, py = 66, pw = SCREENWIDTH - 40, ph = 236;
      if (!inRect(tx, ty, px, py, pw, ph)) {
        closeOverlays();
      }
    }

    // Pick a random sweet nothing and pop it in a speech bubble above Totoro.
    void showLoveMessage() {
      static const char *const kLoveLines[] = {
        "I love you",
        "Happy Anniversary",
        "You're my forever",
        "Forever & always",
        "My heart is yours",
        "You complete me",
        "My favorite person",
        "You're my sunshine",
        "Always choose you",
        "Cutest wife ever",
        "My best friend",
        "Love you more",
      };
      const int count = sizeof(kLoveLines) / sizeof(kLoveLines[0]);
      const char *msg = kLoveLines[random(0, count)];
      strncpy(speechText, msg, sizeof(speechText) - 1);
      speechText[sizeof(speechText) - 1] = '\0';
      speechUntilMs = millis() + 3500;
      addSound(NOTE_E5, noteDurationMs(16, 900));
      addSound(NOTE_G5, noteDurationMs(16, 900));
      addSound(NOTE_C6, noteDurationMs(16, 900));
      drawSpeechBubble();
    }

    // A rounded speech bubble with a little downward tail, floated above the pet.
    void drawSpeechBubble() {
      if (speechText[0] == '\0') {
        return;
      }
      uint16_t paper = rgb565(255, 255, 255);
      uint16_t border = rgb565(70, 74, 70);
      uint16_t ink = rgb565(210, 60, 110);

      int16_t textW = _tft->textWidth(speechText, 2);
      int16_t bw = textW + 26;
      if (bw > SCREENWIDTH - 16) bw = SCREENWIDTH - 16;
      if (bw < 90) bw = 90;
      int16_t bh = 34;
      int16_t bx = (SCREENWIDTH - bw) / 2;
      int16_t by = 150;

      _tft->fillRoundRect(bx, by, bw, bh, 8, paper);
      _tft->drawRoundRect(bx, by, bw, bh, 8, border);
      // Tail pointing down toward Totoro.
      int16_t tailCx = SCREENWIDTH / 2;
      _tft->fillTriangle(tailCx - 7, by + bh - 1, tailCx + 7, by + bh - 1,
                         tailCx, by + bh + 11, paper);
      _tft->drawLine(tailCx - 7, by + bh - 1, tailCx, by + bh + 11, border);
      _tft->drawLine(tailCx + 7, by + bh - 1, tailCx, by + bh + 11, border);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(ink, paper);
      _tft->drawString(speechText, SCREENWIDTH / 2, by + bh / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void doPet() {
      if (petSession <= 0) {
        addSound(NOTE_A3, noteDurationMs(24, 600));  // Totoro has had enough pets for now
        return;
      }
      PetTotoroState::adjustHappiness(PET_PET_HAPPINESS);
      PetTotoroState::addCareXP(PET_CARE_XP_PET);
      petSession--;
      if (petSession == 0) {
        petCooldownUntilMs = millis() + PET_PET_COOLDOWN_MS;
      }
      addSound(NOTE_E5, noteDurationMs(16, 900));
      addSound(NOTE_G5, noteDurationMs(16, 900));
    }

    void doBathe() {
      PetTotoroState::setCleanness(PET_STAT_MAX);
      for (int i = 0; i < MAX_SOOT; i++) {
        if (sootSlots[i].active) {
          sootSlots[i].active = false;
          if (sootSlots[i].avatar != NULL) {
            sootSlots[i].avatar->setPos(-40, SCREENHEIGHT + 20);
          }
        }
      }
      PetTotoroState::addCareXP(PET_CARE_XP_BATHE);
      addSound(NOTE_C5, noteDurationMs(16, 900));
      addSound(NOTE_E5, noteDurationMs(16, 900));
      addSound(NOTE_G5, noteDurationMs(16, 900));
    }

    // Consume a pending mini-game result: grant coins, happiness and care-XP
    // (a win pays more; a loss still gives a small consolation) and queue a
    // brief toast. Safe to call every entry; it no-ops without a pending result.
    void applyGameReward() {
      if (!GameResult::pending()) {
        return;
      }
      GameOutcome outcome = GameResult::outcome();
      int reportedCoins = GameResult::coins();
      int reportedHappiness = GameResult::happiness();
      int coins;
      int happiness;
      const char *label;
      if (outcome == GAME_RESULT_WIN) {
        coins = (reportedCoins >= 0) ? reportedCoins : GAME_WIN_DEFAULT_COINS;
        happiness = (reportedHappiness >= 0) ? reportedHappiness : GAME_WIN_HAPPINESS;
        PetTotoroState::addCareXP(GAME_WIN_CARE_XP);
        label = "Win!";
        addSound(NOTE_E5, noteDurationMs(10, 900));
        addSound(NOTE_G5, noteDurationMs(10, 900));
        addSound(NOTE_C6, noteDurationMs(10, 900));
      } else {
        // A loss always pays the fixed consolation (games report 0 coins on a
        // loss); only happiness can be overridden per game.
        coins = GAME_LOSS_CONSOLATION_COINS;
        happiness = (reportedHappiness >= 0) ? reportedHappiness : GAME_LOSS_HAPPINESS;
        PetTotoroState::addCareXP(GAME_LOSS_CARE_XP);
        label = "Nice try!";
        addSound(NOTE_C5, noteDurationMs(12, 700));
        addSound(NOTE_E5, noteDurationMs(12, 700));
      }
      PetTotoroState::adjustHappiness(happiness);
      if (coins > 0) {
        GameProgress::addCoins(coins);
      }
      snprintf(rewardToast, sizeof(rewardToast), "%s +%d coins", label, coins);
      rewardToastUntilMs = millis() + PET_REWARD_TOAST_MS;
      GameResult::clear();
    }

    void drawRewardToast() {
      int16_t y = 6;
      uint16_t bg = rgb565(40, 95, 55);
      _tft->fillRect(0, y, SCREENWIDTH, 22, bg);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, bg);
      _tft->drawString(rewardToast, SCREENWIDTH / 2, y + 11, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawSickBanner() {
      int16_t y = 6;
      _tft->fillRect(0, y, SCREENWIDTH, 20, rgb565(150, 40, 40));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, rgb565(150, 40, 40));
      _tft->drawString("Totoro is sick - care for it!", SCREENWIDTH / 2, y + 10, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawEscapedScreen() {
      uint16_t bg = rgb565(24, 28, 24);
      _tft->fillScreen(bg);

      int16_t cardX = 24;
      int16_t cardY = 74;
      int16_t cardW = SCREENWIDTH - 48;
      int16_t cardH = 150;
      uint16_t paper = rgb565(240, 232, 205);
      _tft->fillRoundRect(cardX, cardY, cardW, cardH, 10, paper);
      _tft->drawRoundRect(cardX, cardY, cardW, cardH, 10, rgb565(120, 110, 80));

      int16_t cx = SCREENWIDTH / 2;
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(70, 58, 44), paper);
      _tft->drawString("Totoro ran away", cx, cardY + 26, 2);
      _tft->drawString("\"Please take better", cx, cardY + 62, 2);
      _tft->drawString("care of me...\"", cx, cardY + 84, 2);
      _tft->drawString("- Totoro", cx, cardY + 120, 2);

      _tft->setTextColor(rgb565(235, 175, 175), bg);
      _tft->drawString("Factory Reset in Settings", cx, cardY + cardH + 24, 2);
      _tft->drawString("to raise a new Totoro", cx, cardY + cardH + 44, 2);
      _tft->setTextColor(rgb565(170, 185, 205), bg);
      _tft->drawString("HOME -> Settings", cx, cardY + cardH + 72, 2);
      _tft->setTextDatum(TL_DATUM);
    }
};

#endif
