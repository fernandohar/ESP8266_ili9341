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
#include "PetSim.h"
#include "ml/MLGameHooks.h"
#include "ml/CareActionPredictor.h"
#if defined(TINYML_GESTURE_INFERENCE)
#include "ml/GesturePredictor.h"
#include "ml/TouchSampler.h"
#endif
#include "Input.h"
#include "SpriteSheet.h"
#include "Attachment.h"
#include "TouchInput.h"
#include "image_acorn_catch_bg.h"
// Baby and adult are the only stages; a third would cost another full sheet to
// draw. The retired sprite_totoro_baby.h and sprite_totoro_junior.h are still in
// src/ - re-adding an include is all it takes to bring either back.
#include "sprite_totoro_pet.h"
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

#define PET_SOOT_SPAWN_MIN_MS 18000
#define PET_SOOT_SPAWN_MAX_MS 36000

// Unhappy pet may leave after this grace period (powered-on time).
#define PET_UNHAPPY_GRACE_MS 60000
// Care XP awarded per soot cleaned (feeding/petting/game wins add more later).
#define PET_CARE_XP_CLEAN 10

// Totoro sprite-sheet region order (shared by the baby and adult sheets).
#define TOTORO_RGN_WALK_A 0
#define TOTORO_RGN_WALK_B 1
#define TOTORO_RGN_SIT 2
#define TOTORO_RGN_STAND 3
#define TOTORO_RGN_SLEEP 4
// Legacy eyes-closed frame. The baby/adult sheets alias it to the standing
// body because their eyes are a separate strip, so nothing draws it any more.
#define TOTORO_RGN_BLINK 5
// Only the baby and adult sheets provide these (see hasEyes).
#define TOTORO_RGN_HUNGRY 6
#define TOTORO_RGN_DANCE 7
#define TOTORO_RGN_SIT_SIDE 8
// Posture cycling: pick a new random pose every few seconds; while walking,
// alternate the two walk frames at this cadence.
#define PET_POSE_MIN_MS 2500
#define PET_POSE_MAX_MS 5000
#define PET_WALK_FRAME_MS 220
// Dancing has only the one body, so it sways by mirroring itself on the spot.
#define PET_DANCE_FRAME_MS 240
// Below this hunger the pet drops everything and clutches its belly.
#define PET_HUNGRY_POSE_THRESHOLD 30
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

// --- Picking Totoro up and dragging it around ---
// A press on the pet is only a drag once the finger has travelled this far; anything
// shorter is still a tap and opens the radial menu on release. Gesture builds ignore
// this and use the hold below instead, since a tap there is a poke and a stroke across
// the pet has to stay available to the classifier.
#define PET_DRAG_THRESHOLD 6
// How high it can be lifted, and how fast it drops back when let go mid-air.
#define PET_DRAG_MIN_Y 60
#define PET_DROP_SPEED 7.0f
// The resistive panel drops a reading now and then; ignore this many empty
// ticks before deciding the finger really left the glass.
#define PET_DRAG_RELEASE_TICKS 2

// --- Touch gestures (esp32-gesture builds) --------------------------------
// Holding still on Totoro *arms* the hold: a blip acknowledges it, and from there
// letting go opens the menu while moving on picks Totoro up instead. One hold serves
// both because they are told apart by what the finger does next, the way long-press
// then drag works on a phone.
//
// Arming is decided live rather than by the classifier, for two reasons: carrying has
// to begin while the finger is still down, and the menu is the one interaction that
// cannot afford a 5% miss rate. `GESTURE_LONG_PRESS` therefore never reaches
// handleGesture(); it stays in the class set only so the model is not forced to file
// holds under some other gesture.
#define PET_GRAB_HOLD_MS 400
// ...and only if the finger stayed put, which is what separates a hold from a slow
// brush stroke over the same spot. Measured across all 642 captures, resampled to the
// 50 ms game tick the scene actually reads the panel on, at the instant contact
// reaches PET_GRAB_HOLD_MS: a real long press had covered 5-23 px, while every brush,
// circle, zigzag or scribble that lasts that long had covered at least 96 px.
#define PET_GRAB_MAX_TRAVEL_PX 80.0f
// How far the finger has to move after arming to mean "carry me" rather than "menu".
// Intent is already established by then, so this only has to clear touch jitter; the
// plain drag threshold in non-gesture builds is 6 px and works.
#define PET_CARRY_BREAK_PX 10
// A poke on empty floor sends Totoro walking there; a swipe sends it to the wall
// in a hurry.
#define PET_WALK_TO_SPEED 1.8f
#define PET_FLEE_SPEED 2.8f
#define PET_WALK_ARRIVE_PX 4
// How long a commanded pose holds before idle posing takes over again. Set as a
// pose deadline, so hunger and sickness still override it the way they override
// any other pose.
#define PET_ATTENTION_HOLD_MS 1800
#define PET_PETTED_HOLD_MS 1500
#define PET_DANCE_HOLD_MS 4500
#define PET_SULK_HOLD_MS 6000

// --- Free / in-place care actions ---
#define PET_PET_HAPPINESS 6
#define PET_PET_MAX_SESSION 3
#define PET_PET_COOLDOWN_MS 60000
#define PET_CARE_XP_PET 2
#define PET_CARE_XP_BATHE 5

// --- Mini-game reward hand-off (applied on return to the home) ---
// The coin half of the payout lives in GameResult.h, since the coin reward
// screen banks it before the player ever gets back here.
#define GAME_WIN_HAPPINESS 12
#define GAME_LOSS_HAPPINESS 4
#define GAME_WIN_CARE_XP 15
#define GAME_LOSS_CARE_XP 5
#define PET_REWARD_TOAST_MS 2200
// How long a speech bubble (love note, greeting) stays up.
#define PET_SPEECH_HOLD_MS 3500

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
  TOTORO_POSE_DANCE,
  TOTORO_POSE_HUNGRY,
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
#if defined(TINYML_GESTURE_INFERENCE)
        // Gestures made during the meal are drawn but never acted on.
        TouchSampler::consumePreview();
        if (TouchSampler::episodeReady()) {
          TouchSampler::consumeEpisode();
        }
#endif
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
#if defined(TINYML_GESTURE_INFERENCE)
          // The overlay owns this touch. Left to build into an episode, dismissing
          // the menu would also poke the pet a moment later.
          TouchSampler::abortEpisode();
#endif
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
#if defined(TINYML_GESTURE_INFERENCE)
          TouchSampler::abortEpisode();
#endif
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

      if (!dragging && !dropping) {
        updatePose(now);  // a held or falling Totoro keeps the pose it has
      }
      updateSoot(now);
      refillPetSession(now);

#if defined(TINYML_GESTURE_INFERENCE)
      // Something is waiting to be classified. A tap in one spot is offered as soon
      // as the finger lifts; anything that travelled waits out the gap window, which
      // is why the instant reactions - dragging, the Home button - never go through
      // the model.
      //
      // Neither is gated on petPressed: a tap is offered ~48 ms after the finger
      // lifts, which can beat the release handling further down this same tick, and
      // gating on it there threw the gesture away instead of delaying it.
      if (TouchSampler::previewReady()) {
        const GestureEpisode &ep = TouchSampler::episode();
        if (!dragging && !dropping) {
          handleGesture(ep, now);
          previewActedStartMs = ep.startMs;
          havePreviewActed = true;
        }
        TouchSampler::consumePreview();
      }
      if (TouchSampler::episodeReady()) {
        const GestureEpisode &ep = TouchSampler::episode();
        // A single tap that was already acted on when the finger lifted arrives here
        // a second time once its window expires with no follow-up tap. Nothing new
        // has been learned about it, so it must not be acted on twice. A second tap
        // would have made this a two-stroke episode, which is a different gesture and
        // does get its own reaction.
        const bool alreadyActed = havePreviewActed && ep.strokeCount == 1 &&
                                  ep.startMs == previewActedStartMs;
        if (!dragging && !dropping && !alreadyActed) {
          handleGesture(ep, now);
        }
        havePreviewActed = false;
        TouchSampler::consumeEpisode();
      }
#endif

      uint16_t touchX = 0;
      uint16_t touchY = 0;
      bool havePoint = isTouching && getTouchPoint(_tft, &touchX, &touchY);
      if (isTouching && !wasTouching) {
        if (havePoint) {
          if (tryCleanSoot(touchX, touchY)) {
#if defined(TINYML_GESTURE_INFERENCE)
            TouchSampler::abortEpisode();  // that touch was aimed at the soot
#endif
            wasTouching = isTouching;
            requestRender();
            return;
          }
          if (tapOnPet(touchX, touchY)) {
            beginPress(touchX, touchY);
          }
        }
      } else if (petPressed && havePoint) {
#if defined(TINYML_GESTURE_INFERENCE)
        // Path length, not displacement: a brush that strokes back and forth over the
        // same spot ends up near where it started, and would read as a hold.
        {
          const float dx = (float)((int16_t)touchX - pressLastX);
          const float dy = (float)((int16_t)touchY - pressLastY);
          pressTravelPx += sqrtf(dx * dx + dy * dy);
          pressLastX = (int16_t)touchX;
          pressLastY = (int16_t)touchY;
        }
        if (!dragging) {
          if (!holdArmed) {
            if ((now - pressStartMs) >= PET_GRAB_HOLD_MS &&
                pressTravelPx < PET_GRAB_MAX_TRAVEL_PX) {
              armHold(touchX, touchY);
            }
          } else if (abs((int16_t)touchX - holdAnchorX) >= PET_CARRY_BREAK_PX ||
                     abs((int16_t)touchY - holdAnchorY) >= PET_CARRY_BREAK_PX) {
            startDrag();  // moved on after the hold: carry it instead
          }
        }
#endif
        updateDrag(touchX, touchY);
      } else if (petPressed && !isTouching) {
        if (dragging && dragReleaseTicks < PET_DRAG_RELEASE_TICKS) {
          dragReleaseTicks++;
        } else if (releasePress(now)) {
#if defined(TINYML_GESTURE_INFERENCE)
          if (holdArmed) {
            holdArmed = false;
            openMenu();  // held still and let go, rather than moving on to carry
            wasTouching = isTouching;
            return;
          }
          // Any shorter press is the classifier's to interpret: one poke gets
          // Totoro's attention, two get a greeting. Acting here would fire first and
          // every time, so the release does nothing but let go.
#else
          openMenu();  // pressed and let go without moving: still a tap
          wasTouching = isTouching;
          return;
#endif
        }
      }

      if (dragging) {
        // Totoro is in hand: it does not walk, and the face rides along.
        updateFace(pets[0]);
        if (eyeAttach != NULL) {
          eyeAttach->updatePos(now);
        }
        wasTouching = isTouching;
        requestRender();
        return;
      }

      Pet &pet = pets[0];
      if (pet.avatar != NULL) {
        pet.avatar->updatePos(now);
        clampPet(pet);
#if defined(TINYML_GESTURE_INFERENCE)
        updateWalkTarget(pet, now);  // stop on the commanded spot, if any
#endif
        if (dropping) {
          settleDrop(pet, now);
        }
      }
      updateFace(pet);  // mood may have shifted this tick
      if (eyeAttach != NULL) {
        eyeAttach->updatePos(now);  // ride along after the body has moved
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
      if (dragging && speechUntilMs != 0) {
        drawSpeechBubble();  // the pet can be carried across the bubble
      }
    }

    void initScene() {
      setBackgroundAsset(&acorn_catch_bg);
      drawBackgroundAsset(&acorn_catch_bg);

      pets[0].avatar = NULL;
      eyeAttach = NULL;
      menuOpen = false;
      playOpen = false;
      petPressed = false;
      dragging = false;
      dropping = false;
      dragReleaseTicks = 0;
#if defined(TINYML_GESTURE_INFERENCE)
      hasWalkTarget = false;
      havePreviewActed = false;
      holdArmed = false;
#endif
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
      unhappySinceMs = 0;
      nextSootSpawnMs = millis() + PET_SOOT_SPAWN_MIN_MS;

      if (PetTotoroState::isSick()) {
        // Resumed while very unhappy: hold still and start the grace clock now.
        unhappySinceMs = millis();
        if (pets[0].avatar != NULL) {
          pets[0].avatar->setVelocity(0, 0);
        }
        pets[0].pose = TOTORO_POSE_SIT;
        setMirrored(pets[0], false);
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
      mlLogHubVisit();
      requestRender();
    }

    void destroyScene() {
      pets[0].avatar = NULL;
      petPressed = false;
      dragging = false;
      dropping = false;
      for (int i = 0; i < MAX_SOOT; i++) {
        sootSlots[i].avatar = NULL;
        sootSlots[i].active = false;
      }
      // GameScene::destroyScene() frees every avatar (incl. the attachments);
      // just drop our cached pointers/flag.
      eating = false;
      foodAttach = NULL;
      eyeAttach = NULL;
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
      int faceRegion;   // body region currently shown
      unsigned long nextPoseMs;
      unsigned long poseFrameMs;
      bool poseFrameB;
      // The face is a separate sheet cell hung on the body, so a mood change
      // costs one small region swap instead of a second copy of every pose.
      bool hasEyes;
      const uint8_t *eyeBaseTable;     // per body region: sheet region of variant 0
      const uint8_t *eyeOffsetXTable;  // per body region, mirrored when flipped
      const uint8_t *eyeOffsetYTable;
      uint8_t bodyRegionCount;
      uint8_t eyeRegion;   // strip currently applied
      uint8_t eyeAppliedX;
      uint8_t eyeAppliedY;
    };

    Pet pets[1];
    SootSlot sootSlots[MAX_SOOT];
    boolean wasTouching = false;
    PetRoomState roomState = PET_ROOM_ACTIVE;
    unsigned long unhappySinceMs = 0;

    bool menuOpen = false;
    // Care suggestion for the open menu, and the ring icon it highlights (-1 for
    // none). Refreshed by openMenu().
    CarePrediction careHint = {CARE_ACTION_PLAY, 0.0f, false};
    int hintedMenuItem = -1;
    bool playOpen = false;
    int petSession = PET_PET_MAX_SESSION;
    unsigned long petCooldownUntilMs = 0;

    // Drag state. A press on the pet starts as an undecided "petPressed" and
    // only becomes a drag once the finger travels; dropping covers the fall
    // back to the ground after being released in mid-air.
    bool petPressed = false;
    bool dragging = false;
    bool dropping = false;
    uint8_t dragReleaseTicks = 0;
    int16_t pressStartX = 0;
    int16_t pressStartY = 0;
    int16_t grabOffsetX = 0;  // pet position minus touch position at grab time
    int16_t grabOffsetY = 0;

#if defined(TINYML_GESTURE_INFERENCE)
    // Walking to a commanded spot rather than wandering. Fleeing uses the same
    // machinery but sits down facing the wall on arrival instead of standing.
    bool hasWalkTarget = false;
    bool walkTargetFlee = false;
    bool walkTargetRight = false;
    int16_t walkTargetX = 0;

    // Which tap was already reacted to on lift, so the same tap arriving again as a
    // closed episode is recognised and ignored.
    bool havePreviewActed = false;
    unsigned long previewActedStartMs = 0;

    // A hold that has been recognised but not yet resolved into either the menu (let
    // go) or a carry (move on). The anchor is where the finger was when it armed, not
    // where it first landed, so a hold that drifted slowly does not instantly count
    // as having moved.
    bool holdArmed = false;
    int16_t holdAnchorX = 0;
    int16_t holdAnchorY = 0;

    // How long this press has lasted and how far it has wandered, measured from the
    // scene's own touch reads rather than from TouchSampler. The menu hangs off these,
    // and it must not be possible to lose it to a press too light for the fast
    // sampler's engage threshold - which would leave the pet with no menu at all.
    unsigned long pressStartMs = 0;
    float pressTravelPx = 0.0f;
    int16_t pressLastX = 0;
    int16_t pressLastY = 0;
#endif

    // Eating animation state (a food bought in the grocery is attached to the
    // Totoro avatar and chewed through 3 frames before its effect is applied).
    bool eating = false;
    int eatPhase = 0;
    unsigned long eatPhaseUntilMs = 0;
    Attachment *foodAttach = NULL;
    Attachment *eyeAttach = NULL;
    int mealHunger = 0;
    int mealHappiness = 0;
    int mealRegion[3] = {0, 0, 0};

    char rewardToast[24] = {0};
    unsigned long rewardToastUntilMs = 0;

    // Anniversary love note shown in a speech bubble when Totoro is petted.
    char speechText[28] = {0};
    unsigned long speechUntilMs = 0;

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
    // Both sheets carry their eyes as separate cells and expose the same three
    // offset tables, so setupFace() drives either one unchanged.
    void setupStagePet() {
      if (PetTotoroState::stage() == PET_STAGE_ADULT) {
        setupPet(pets[0], &sprite_totoro_adult, SPRITE_TOTORO_ADULT_SHEET_WIDTH,
                 SPRITE_TOTORO_ADULT_SHEET_HEIGHT, sprite_totoro_adultRegions);
        setupFace(pets[0], sprite_totoro_adultEyeBase, sprite_totoro_adultEyeOffsetX,
                  sprite_totoro_adultEyeOffsetY, SPRITE_TOTORO_ADULT_BODY_REGION_COUNT);
        return;
      }
      setupPet(pets[0], &sprite_totoro_pet, SPRITE_TOTORO_PET_SHEET_WIDTH,
               SPRITE_TOTORO_PET_SHEET_HEIGHT, sprite_totoro_petRegions);
      setupFace(pets[0], sprite_totoro_petEyeBase, sprite_totoro_petEyeOffsetX,
                sprite_totoro_petEyeOffsetY, SPRITE_TOTORO_PET_BODY_REGION_COUNT);
    }

    // Fields every stage shares; the caller still has to attach the avatar.
    void initPetCommon(Pet &p, const SpriteSheetRegion *regions, const SpriteSheetRegion &stand) {
      p.regions = regions;
      p.frameW = stand.width;
      p.frameH = stand.height;
      p.pose = TOTORO_POSE_STAND;
      p.faceRegion = TOTORO_RGN_STAND;
      p.poseFrameB = false;
      p.poseFrameMs = millis();
      p.nextPoseMs = millis() + PET_POSE_MIN_MS + random(0, PET_POSE_MAX_MS - PET_POSE_MIN_MS);
      p.hasEyes = false;
      p.eyeBaseTable = NULL;
      p.eyeOffsetXTable = NULL;
      p.eyeOffsetYTable = NULL;
      p.bodyRegionCount = 0;
      p.eyeRegion = 0xFF;  // no strip applied yet
      p.eyeAppliedX = 0;
      p.eyeAppliedY = 0;
    }

    void setupPet(Pet &p, const SpriteAsset *asset, uint16_t sheetW, uint16_t sheetH,
                  const SpriteSheetRegion *regions) {
      p.asset = asset;
      p.bitmap = NULL;
      p.mask = NULL;
      p.sheetW = sheetW;
      p.sheetH = sheetH;
      SpriteSheetRegion stand = SpriteSheet::readRegion(regions, TOTORO_RGN_STAND);
      initPetCommon(p, regions, stand);
      int16_t x = (SCREENWIDTH - (int16_t)p.frameW) / 2;
      int16_t y = PET_GROUND_Y - (int16_t)p.frameH;
      SpriteSheet sheet(asset);
      p.avatar = sheet.createAvatar(x, y, stand);
      appendAvatar(p.avatar);
    }

    void setupPet(Pet &p, const uint16_t *bitmap, const uint8_t *mask,
                  uint16_t sheetW, uint16_t sheetH, const SpriteSheetRegion *regions) {
      p.asset = NULL;
      p.bitmap = bitmap;
      p.mask = mask;
      p.sheetW = sheetW;
      p.sheetH = sheetH;
      SpriteSheetRegion stand = SpriteSheet::readRegion(regions, TOTORO_RGN_STAND);
      initPetCommon(p, regions, stand);
      int16_t x = (SCREENWIDTH - (int16_t)p.frameW) / 2;
      int16_t y = PET_GROUND_Y - (int16_t)p.frameH;  // bottom-align to the ground line
      SpriteSheet sheet(bitmap, mask, sheetW, sheetH);
      p.avatar = sheet.createAvatar(x, y, stand);
      appendAvatar(p.avatar);
    }

    // Hang the eye strip on the body. Both stage sheets ship their eyes as
    // separate cells, indexed through the caller's per-pose tables.
    void setupFace(Pet &p, const uint8_t *base, const uint8_t *offsetX,
                   const uint8_t *offsetY, uint8_t bodyRegions) {
      if (p.avatar == NULL) {
        return;
      }
      p.hasEyes = true;
      p.eyeBaseTable = base;
      p.eyeOffsetXTable = offsetX;
      p.eyeOffsetYTable = offsetY;
      p.bodyRegionCount = bodyRegions;
      SpriteSheetRegion eye = SpriteSheet::readRegion(p.regions, pgm_read_byte(&base[0]));
      eyeAttach = new Attachment(0, 0, p.avatar, eye.width, eye.height, NULL, NULL);
      appendAvatar(eyeAttach);  // appended after the body -> drawn on top of it
      updateFace(p);
    }

    // Happiness picks the expression. Bands are lower-inclusive, and the
    // returned index counts the strips left to right in the pose worksheets.
    static uint8_t eyeVariantForHappiness(int happiness) {
      if (happiness >= 80) return 1;  // eye 2: happiest
      if (happiness >= 60) return 4;  // eye 5: excited
      if (happiness >= 40) return 2;  // eye 3: content
      if (happiness >= 20) return 0;  // eye 1: normal
      return 3;                       // eye 4: sad
    }

    // Re-point the face at the mood's strip and at the current pose's eye
    // socket. Cheap to call every tick: it bails out unless something moved.
    void updateFace(Pet &p) {
      if (!p.hasEyes || eyeAttach == NULL || p.avatar == NULL) {
        return;
      }
      if (p.faceRegion >= p.bodyRegionCount) {
        return;
      }
      uint8_t variant = eyeVariantForHappiness(PetTotoroState::stats().happiness);
      // Side-on poses show a single eye, so the strip to use is per pose too.
      uint8_t region = pgm_read_byte(&p.eyeBaseTable[p.faceRegion]) + variant;
      uint8_t offsetX = pgm_read_byte(&p.eyeOffsetXTable[p.faceRegion]);
      uint8_t offsetY = pgm_read_byte(&p.eyeOffsetYTable[p.faceRegion]);
      bool flip = p.avatar->flipX;
      if (region == p.eyeRegion && offsetX == p.eyeAppliedX && offsetY == p.eyeAppliedY &&
          flip == eyeAttach->flipX) {
        return;
      }

      if (region != p.eyeRegion) {
        p.eyeRegion = region;
        SpriteSheet sheet(p.asset);
        sheet.applyRegion(eyeAttach, SpriteSheet::readRegion(p.regions, region));
      }
      p.eyeAppliedX = offsetX;
      p.eyeAppliedY = offsetY;
      // The body is centred on its own outline, so mirroring the cell moves the
      // eye socket to the far side.
      int16_t attachX = flip ? (int16_t)p.frameW - (int16_t)eyeAttach->width - (int16_t)offsetX
                             : (int16_t)offsetX;
      eyeAttach->setAttachOffset(attachX, offsetY);
      eyeAttach->setFlipX(flip);
      eyeAttach->updatePos(millis());
      eyeAttach->requestRedraw();
    }

    // Every pose is drawn facing left or head-on, so mirroring is what turns
    // the pet to face right.
    void setMirrored(Pet &p, bool mirrored) {
      if (p.avatar != NULL) {
        p.avatar->setFlipX(mirrored);
      }
    }

    void clampPet(Pet &p) {
      if (p.avatar == NULL) {
        return;
      }
      if (p.avatar->x < PET_WALK_MIN_X) {
        p.avatar->x = PET_WALK_MIN_X;
        p.avatar->velocity.x = fabs(p.avatar->velocity.x);
        if (p.pose == TOTORO_POSE_WALK) {
          setMirrored(p, true);  // bounced off the left wall, now heading right
        }
        p.avatar->requestRedraw();
      }
      // Wider pets get less horizontal room; clamp against their own width.
      float maxX = PET_WALK_MAX_X - p.avatar->width;
      if (p.avatar->x > maxX) {
        p.avatar->x = maxX;
        p.avatar->velocity.x = -fabs(p.avatar->velocity.x);
        if (p.pose == TOTORO_POSE_WALK) {
          setMirrored(p, false);
        }
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
      p.faceRegion = regionIndex;
      p.avatar->requestRedraw();
      updateFace(p);
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

      // Hold still, sitting face-on, while eating.
      p.pose = TOTORO_POSE_SIT;
      p.avatar->setVelocity(0, 0);
      setMirrored(p, false);
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
      if (eyeAttach != NULL) {
        eyeAttach->updatePos(now);
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
      mlLogCareState();

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
      if (p.avatar == NULL) {
        return;
      }

      const PetTotoroStats &mood = PetTotoroState::stats();

      // An empty stomach beats every other urge: stand still and clutch it.
      if (p.hasEyes && mood.hunger < PET_HUNGRY_POSE_THRESHOLD) {
        p.pose = TOTORO_POSE_HUNGRY;
        p.avatar->setVelocity(0, 0);
        setMirrored(p, false);
        applyPoseFrame(p, TOTORO_RGN_HUNGRY);
        return;
      }

      // Wander, sit, dance, or stand. (Sleep stays disabled for now.)
      int pick = random(0, p.hasEyes ? 4 : 3);
      p.pose = (pick == 0) ? TOTORO_POSE_WALK
             : (pick == 1) ? TOTORO_POSE_SIT
             : (pick == 2) ? TOTORO_POSE_STAND
                           : TOTORO_POSE_DANCE;
      // Ambient mood cue: a low / unhappy Totoro would rather rest than romp.
      if ((mood.happiness < PET_STAT_PER_PIP) &&
          (p.pose == TOTORO_POSE_WALK || p.pose == TOTORO_POSE_DANCE)) {
        p.pose = TOTORO_POSE_SIT;
      }
      switch (p.pose) {
        case TOTORO_POSE_WALK: {
          bool right = (random(0, 2) == 0);
          p.avatar->setVelocity(right ? PET_WALK_SPEED : -PET_WALK_SPEED, 0);
          setMirrored(p, right);
          p.poseFrameB = false;
          p.poseFrameMs = now;
          applyPoseFrame(p, TOTORO_RGN_WALK_A);
          break;
        }
        case TOTORO_POSE_SIT:
          p.avatar->setVelocity(0, 0);
          // Half the time it flops down side-on, keeping the way it faced.
          if (p.hasEyes && random(0, 2) == 0) {
            applyPoseFrame(p, TOTORO_RGN_SIT_SIDE);
          } else {
            setMirrored(p, false);
            applyPoseFrame(p, TOTORO_RGN_SIT);
          }
          break;
        case TOTORO_POSE_DANCE:
          p.avatar->setVelocity(0, 0);
          setMirrored(p, false);
          p.poseFrameB = false;
          p.poseFrameMs = now;
          applyPoseFrame(p, TOTORO_RGN_DANCE);
          break;
        // case TOTORO_POSE_SLEEP:
        //   p.avatar->setVelocity(0, 0);
        //   applyPoseFrame(p, TOTORO_RGN_SLEEP);
        //   break;
        case TOTORO_POSE_STAND:
        default:
          p.avatar->setVelocity(0, 0);
          setMirrored(p, false);
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
      // Hunger overrides the timer in both directions, so feeding the pet
      // perks it up straight away instead of after the pose hold expires.
      bool starving = p.hasEyes && PetTotoroState::stats().hunger < PET_HUNGRY_POSE_THRESHOLD;
      if (now >= p.nextPoseMs || starving != (p.pose == TOTORO_POSE_HUNGRY)) {
        chooseNewPose(p, now);
        return;
      }
      if (p.pose == TOTORO_POSE_WALK && (now - p.poseFrameMs) >= PET_WALK_FRAME_MS) {
        p.poseFrameMs = now;
        p.poseFrameB = !p.poseFrameB;
        applyPoseFrame(p, p.poseFrameB ? TOTORO_RGN_WALK_B : TOTORO_RGN_WALK_A);
      }
      if (p.pose == TOTORO_POSE_DANCE && (now - p.poseFrameMs) >= PET_DANCE_FRAME_MS) {
        p.poseFrameMs = now;
        p.poseFrameB = !p.poseFrameB;
        setMirrored(p, p.poseFrameB);
      }
    }

    // Very low happiness -> Totoro may leave after a grace period.
    void updateLifeState(unsigned long now) {
      if (roomState != PET_ROOM_ACTIVE) {
        return;
      }
      const PetTotoroStats &stats = PetTotoroState::stats();
      if (stats.happiness <= PET_ESCAPE_HAPPINESS) {
        if (unhappySinceMs == 0) {
          enterUnhappy(now);
        } else if (now - unhappySinceMs >= PET_UNHAPPY_GRACE_MS) {
          triggerEscape();
        }
      } else if (unhappySinceMs != 0) {
        unhappySinceMs = 0;
        addSound(NOTE_E5, noteDurationMs(16, 900));
        renderFullScreen();
        chooseNewPose(pets[0], now);
      }
    }

    void enterUnhappy(unsigned long now) {
      unhappySinceMs = now;
      if (pets[0].avatar != NULL) {
        pets[0].avatar->setVelocity(0, 0);
      }
      pets[0].pose = TOTORO_POSE_SIT;
      setMirrored(pets[0], false);
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

    // ---- Picking Totoro up --------------------------------------------------

    // Where the current sprite's feet rest. Poses differ in height, so this is
    // read off the avatar rather than cached.
    float petGroundY(const Pet &p) const {
      return (float)PET_GROUND_Y - (float)p.avatar->height;
    }

    // Back to wandering (or to sitting, if the pet is too sick to).
    void restPose(Pet &p, unsigned long now) {
      if (PetTotoroState::isSick()) {
        p.pose = TOTORO_POSE_SIT;
        setMirrored(p, false);
        applyPoseFrame(p, TOTORO_RGN_SIT);
        return;
      }
      chooseNewPose(p, now);
    }

    void beginPress(uint16_t touchX, uint16_t touchY) {
      Avatar *a = pets[0].avatar;
      if (a == NULL) {
        return;
      }
      petPressed = true;
      dragging = false;
      dragReleaseTicks = 0;
#if defined(TINYML_GESTURE_INFERENCE)
      holdArmed = false;
      pressStartMs = millis();
      pressTravelPx = 0.0f;
      pressLastX = (int16_t)touchX;
      pressLastY = (int16_t)touchY;
#endif
      pressStartX = (int16_t)touchX;
      pressStartY = (int16_t)touchY;
      grabOffsetX = (int16_t)a->x - (int16_t)touchX;
      grabOffsetY = (int16_t)a->y - (int16_t)touchY;
    }

#if defined(TINYML_GESTURE_INFERENCE)
    // The hold has been recognised. Nothing visible happens yet - what it means
    // depends on whether the finger now leaves or moves - so a short blip is the
    // acknowledgement, without which a hold would feel ignored until release.
    void armHold(uint16_t touchX, uint16_t touchY) {
      holdArmed = true;
      holdAnchorX = (int16_t)touchX;
      holdAnchorY = (int16_t)touchY;
      // This contact belongs to the live tier from here on, whichever way it goes.
      TouchSampler::abortEpisode();
      addSound(NOTE_E4, noteDurationMs(32, 700));
    }
#endif

    void startDrag() {
      Pet &p = pets[0];
      dragging = true;
      dropping = false;
#if defined(TINYML_GESTURE_INFERENCE)
      holdArmed = false;
      // This contact belongs to the live tier now. Without this the classifier
      // would also get it once the finger lifted and fire a second behaviour on
      // top of the drag.
      TouchSampler::abortEpisode();
      hasWalkTarget = false;
#endif
      p.avatar->setVelocity(0, 0);
      // Held up with its arms out, and it keeps that pose until it is let go.
      p.pose = TOTORO_POSE_DANCE;
      setMirrored(p, false);
      applyPoseFrame(p, p.hasEyes ? TOTORO_RGN_DANCE : TOTORO_RGN_STAND);
      addSound(NOTE_G4, noteDurationMs(20, 800));
    }

    void updateDrag(uint16_t touchX, uint16_t touchY) {
      Avatar *a = pets[0].avatar;
      if (a == NULL) {
        return;
      }
      dragReleaseTicks = 0;
      if (!dragging) {
#if defined(TINYML_GESTURE_INFERENCE)
        // Travel alone must not start a drag here. Picking Totoro up the moment the
        // finger moved 6 px meant every stroke that began on the pet was claimed as a
        // drag before the classifier ever saw it - so a swipe *was* a drag, and a
        // long press only did what a short one already did. A hold is now the one way
        // to pick it up, which leaves a stroke across the pet free to be a swipe or a
        // brush.
        return;
#else
        int16_t dx = (int16_t)touchX - pressStartX;
        int16_t dy = (int16_t)touchY - pressStartY;
        if (abs(dx) < PET_DRAG_THRESHOLD && abs(dy) < PET_DRAG_THRESHOLD) {
          return;  // hasn't moved enough yet - could still be a tap
        }
        startDrag();
#endif
      }

      // The pet keeps the spot on its body that was grabbed under the finger.
      float nx = (float)((int16_t)touchX + grabOffsetX);
      float ny = (float)((int16_t)touchY + grabOffsetY);
      float maxX = (float)PET_WALK_MAX_X - a->width;
      if (nx < PET_WALK_MIN_X) nx = PET_WALK_MIN_X;
      if (nx > maxX) nx = maxX;
      float ground = petGroundY(pets[0]);
      if (ny < PET_DRAG_MIN_Y) ny = PET_DRAG_MIN_Y;
      if (ny > ground) ny = ground;
      a->setPos(nx, ny);
    }

    // Finger lifted. Returns true when the press never turned into a drag, so
    // the caller can treat it as a tap.
    bool releasePress(unsigned long now) {
      petPressed = false;
      dragReleaseTicks = 0;
      if (!dragging) {
        return true;
      }
      dragging = false;

      Pet &p = pets[0];
      if (p.avatar == NULL) {
        return false;
      }
      if (p.avatar->y < petGroundY(p)) {
        dropping = true;  // let go in mid-air: fall back to the floor
        p.avatar->setVelocity(0, PET_DROP_SPEED);
      } else {
        p.avatar->setVelocity(0, 0);
        restPose(p, now);
      }
      return false;
    }

    void settleDrop(Pet &p, unsigned long now) {
      float ground = petGroundY(p);
      if (p.avatar->y < ground) {
        return;
      }
      p.avatar->setPos(p.avatar->x, ground);
      p.avatar->setVelocity(0, 0);
      dropping = false;
      addSound(NOTE_C4, noteDurationMs(24, 700));  // a soft landing thud
      restPose(p, now);
    }

#if defined(TINYML_GESTURE_INFERENCE)

    // ---- Gesture-driven behaviour -------------------------------------------

    // Hold one pose for a while instead of the usual random cycling. Expressed as
    // a pose deadline rather than a separate "commanded" flag so that hunger and
    // sickness still take precedence, exactly as they do over an idle pose.
    void commandPose(unsigned long now, TotoroPose pose, int region,
                     unsigned long holdMs) {
      Pet &p = pets[0];
      if (p.avatar == NULL || PetTotoroState::isSick()) {
        // updatePose leaves a sick pet alone entirely, so a pose forced on it here
        // would be held for good rather than for holdMs.
        return;
      }
      hasWalkTarget = false;
      p.pose = pose;
      p.avatar->setVelocity(0, 0);
      p.poseFrameB = false;
      p.poseFrameMs = now;
      setMirrored(p, false);
      // The expressive regions only exist on the baby and adult sheets.
      applyPoseFrame(p, p.hasEyes ? region : TOTORO_RGN_STAND);
      p.nextPoseMs = now + holdMs;
      p.avatar->requestRedraw();
    }

    // Poked: stop, stand up, look at whoever did it. Deliberately no stat change -
    // petting is the action that buys happiness, and a free tap that raised it
    // would make every other care action pointless.
    void petAttention(unsigned long now, int16_t towardX) {
      Pet &p = pets[0];
      if (p.avatar == NULL) {
        return;
      }
      commandPose(now, TOTORO_POSE_STAND, TOTORO_RGN_STAND, PET_ATTENTION_HOLD_MS);
      setMirrored(p, towardX > (int16_t)(p.avatar->x + p.avatar->width / 2));
      addSound(NOTE_E5, noteDurationMs(20, 900));
      addSound(NOTE_A5, noteDurationMs(20, 900));
    }

    // Walk to a spot and stop there, rather than the usual wall-to-wall wander.
    void walkTo(int16_t destX, unsigned long now, bool flee) {
      Pet &p = pets[0];
      if (p.avatar == NULL || PetTotoroState::isSick()) {
        return;  // a sick pet stays where it is, as it does for every other pose
      }

      const float maxX = (float)PET_WALK_MAX_X - p.avatar->width;
      if (destX < PET_WALK_MIN_X) {
        destX = PET_WALK_MIN_X;
      }
      if ((float)destX > maxX) {
        destX = (int16_t)maxX;
      }

      walkTargetX = destX;
      walkTargetFlee = flee;
      // Which way it ends up facing. For a flee that is the wall it is heading for,
      // not the direction it travels, so that being swiped toward the wall it is
      // already standing at still turns its back on you.
      walkTargetRight = flee ? (destX >= (int16_t)maxX) : ((float)destX > p.avatar->x);
      hasWalkTarget = true;

      if (fabs(p.avatar->x - (float)destX) <= PET_WALK_ARRIVE_PX) {
        arriveAtTarget(p, now);  // poked where it already stands
        return;
      }

      p.pose = TOTORO_POSE_WALK;
      const float speed = flee ? PET_FLEE_SPEED : PET_WALK_TO_SPEED;
      p.avatar->setVelocity(walkTargetRight ? speed : -speed, 0);
      setMirrored(p, walkTargetRight);
      p.poseFrameB = false;
      p.poseFrameMs = now;
      applyPoseFrame(p, TOTORO_RGN_WALK_A);
      // Far enough out that the idle timer cannot interrupt the walk; arriving
      // sets its own, shorter deadline.
      p.nextPoseMs = now + 12000;
    }

    void updateWalkTarget(Pet &p, unsigned long now) {
      if (!hasWalkTarget || p.avatar == NULL) {
        return;
      }
      // Hunger, sickness or a newer command took the pose over: the errand is off.
      if (p.pose != TOTORO_POSE_WALK) {
        hasWalkTarget = false;
        return;
      }

      const float remaining = (float)walkTargetX - p.avatar->x;
      const bool overshot = walkTargetRight ? (remaining <= 0.0f) : (remaining >= 0.0f);
      if (fabs(remaining) > PET_WALK_ARRIVE_PX && !overshot) {
        return;
      }
      arriveAtTarget(p, now);
    }

    void arriveAtTarget(Pet &p, unsigned long now) {
      hasWalkTarget = false;
      p.avatar->setPos((float)walkTargetX, p.avatar->y);
      p.avatar->setVelocity(0, 0);

      if (walkTargetFlee) {
        // Sulking in the corner with its back to the room. SIT_SIDE faces left
        // unmirrored, so facing away means mirroring only at the right-hand wall.
        p.pose = TOTORO_POSE_SIT;
        setMirrored(p, walkTargetRight);
        applyPoseFrame(p, p.hasEyes ? TOTORO_RGN_SIT_SIDE : TOTORO_RGN_SIT);
        p.nextPoseMs = now + PET_SULK_HOLD_MS;
        addSound(NOTE_A3, noteDurationMs(24, 600));
      } else {
        p.pose = TOTORO_POSE_STAND;
        setMirrored(p, false);
        applyPoseFrame(p, TOTORO_RGN_STAND);
        p.nextPoseMs = now + PET_ATTENTION_HOLD_MS;
      }
      p.avatar->requestRedraw();
    }

    // Swiped off: trots to the far wall and sulks there. No happiness penalty -
    // the gesture says "go away", and stats already fall on their own when the pet
    // is left alone, so charging for it would punish the player twice.
    void sendAway(bool right, unsigned long now) {
      Pet &p = pets[0];
      if (p.avatar == NULL) {
        return;
      }
      const int16_t dest =
          right ? (int16_t)((float)PET_WALK_MAX_X - p.avatar->width) : (int16_t)PET_WALK_MIN_X;
      walkTo(dest, now, true);
      addSound(NOTE_D4, noteDurationMs(24, 700));
    }

    void commandDance(unsigned long now) {
      commandPose(now, TOTORO_POSE_DANCE, TOTORO_RGN_DANCE, PET_DANCE_HOLD_MS);
      addSound(NOTE_C5, noteDurationMs(24, 900));
      addSound(NOTE_E5, noteDurationMs(24, 900));
      addSound(NOTE_G5, noteDurationMs(24, 900));
    }

    // True when the gesture was aimed at Totoro. Both the centroid and the first
    // point count, because a brush wanders off the body and a swipe deliberately
    // ends far from where it started.
    bool gestureHitPet(const GestureEpisode &ep, int16_t centroidX, int16_t centroidY) {
      if (tapOnPet((uint16_t)centroidX, (uint16_t)centroidY)) {
        return true;
      }
      if (ep.sampleCount == 0) {
        return false;
      }
      return tapOnPet((uint16_t)ep.samples[0].x, (uint16_t)ep.samples[0].y);
    }

    // Every gesture here is something Totoro does in the room. None of them opens a
    // modal overlay, deliberately: the menu is on a hold, handled live, so that the
    // one interaction the player cannot work around does not depend on the model
    // reading a gesture correctly.
    void handleGesture(const GestureEpisode &ep, unsigned long now) {
      GesturePrediction gesture = GesturePredictor::classify(ep);
      if (!gesture.recognised) {
        return;  // unknown, or not confident enough to be worth acting on
      }

      int16_t gx = 0;
      int16_t gy = 0;
      gestureEpisodeCentroid(ep, &gx, &gy);
      const bool onPet = gestureHitPet(ep, gx, gy);

      switch (gesture.label) {
        case GESTURE_DOUBLE_POKE:
          if (onPet) {
            // The first tap already got its own reaction, so Totoro is looking at
            // you by now and the greeting lands on top of that.
            showGreeting(now);
          }
          break;

        case GESTURE_POKE:
          if (onPet) {
            petAttention(now, gx);
          } else if (pets[0].avatar != NULL) {
            // Aim to stand on the spot that was poked, not to put its left edge
            // there.
            walkTo((int16_t)(gx - (int16_t)(pets[0].avatar->width / 2)), now, false);
          }
          break;

        case GESTURE_BRUSH:
          if (onPet) {
            doPet();  // silently a no-op once the petting session is used up
            // Wiggle first, speech bubble second: the bubble is drawn immediately
            // and sits just above the pet, so it wants to be the last thing down.
            commandPose(now, TOTORO_POSE_DANCE, TOTORO_RGN_DANCE, PET_PETTED_HOLD_MS);
            showLoveMessage();
          }
          break;

        case GESTURE_SWIPE:
          if (onPet) {
            sendAway(gestureEpisodeNetDx(ep) >= 0, now);
          }
          break;

        case GESTURE_CIRCLE:
        case GESTURE_ZIGZAG:
          commandDance(now);
          break;

        case GESTURE_LONG_PRESS:
          // Never arrives: a hold on Totoro is claimed by the live tier, which
          // aborts the episode. A hold on empty floor means nothing.
          break;

        default:
          break;
      }

      requestRender();
    }
#endif  // TINYML_GESTURE_INFERENCE

    // Put the pet back down wherever it is: used when an overlay takes over
    // mid-drag, so it never stays frozen in the air.
    void cancelDrag() {
      petPressed = false;
      dragging = false;
      dropping = false;
      dragReleaseTicks = 0;
#if defined(TINYML_GESTURE_INFERENCE)
      hasWalkTarget = false;
      holdArmed = false;
#endif
      Pet &p = pets[0];
      if (p.avatar != NULL) {
        p.avatar->setVelocity(0, 0);
        p.avatar->setPos(p.avatar->x, petGroundY(p));
      }
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

    // Which ring icon a care action points at. Info and Settings are not care
    // actions, so they are never suggested.
    static int menuItemForCareAction(CareAction action) {
      switch (action) {
        case CARE_ACTION_EAT: return PET_MENU_EAT;
        case CARE_ACTION_PLAY: return PET_MENU_PLAY;
        case CARE_ACTION_PET: return PET_MENU_PET;
        case CARE_ACTION_BATH: return PET_MENU_BATHE;
        default: return -1;
      }
    }

    // Ask on every open rather than caching: stats drift while the menu is shut.
    void refreshCareHint() {
      careHint = CareActionPredictor::predict(MLDataLogger::buildHubSample());
      hintedMenuItem = menuItemForCareAction(careHint.action);
    }

    void drawMenuIcon(int i) {
      int16_t ix = 0, iy = 0;
      getIconPos(i, &ix, &iy);
      bool enabled = menuItemEnabled(i);
      uint16_t color = enabled ? menuColor(i) : rgb565(64, 68, 64);
      uint16_t textColor = enabled ? TFT_WHITE : rgb565(150, 155, 150);
      _tft->fillCircle(ix, iy, PET_MENU_ICON_RADIUS, color);
      _tft->drawCircle(ix, iy, PET_MENU_ICON_RADIUS, rgb565(18, 22, 18));
      if (i == hintedMenuItem) {
        // Two rings just outside the icon; the 26px gap between neighbours
        // leaves room for them without touching the next icon.
        uint16_t ring = rgb565(255, 235, 120);
        _tft->drawCircle(ix, iy, PET_MENU_ICON_RADIUS + 2, ring);
        _tft->drawCircle(ix, iy, PET_MENU_ICON_RADIUS + 3, ring);
      }
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
      refreshCareHint();
      cancelDrag();  // the menu is modal, so never leave the pet hanging mid-air
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

    static const int PET_PLAY_GAME_COUNT = 7;

    const char *playGameLabel(int i) {
      switch (i) {
        case 0: return "Acorn Catch";
        case 1: return "Tic-Tac-Toe";
        case 2: return "Whack-a-Mole";
        case 3: return "Cat Bus Cross";
        case 4: return "Slide Puzzle";
        case 5: return "Klotski";
        case 6: return "Four in a Row";
      }
      return "";
    }

    int playGameScene(int i) {
      switch (i) {
        case 0: return SCENE_ACORN_CATCH;
        case 1: return SCENE_TIC_TAC_TOE;
        case 2: return SCENE_WHACK_A_MOLE;
        case 3: return SCENE_CAT_BUS_CROSS;
        case 4: return SCENE_SLIDE_PUZZLE;
        case 5: return SCENE_KLOTSKI;
        case 6: return SCENE_CONNECT_FOUR;
      }
      return SCENE_PET_TOTORO;
    }

    uint16_t playGameColor(int i) {
      switch (i) {
        case 0: return rgb565(200, 150, 70);
        case 1: return rgb565(90, 160, 200);
        case 2: return rgb565(150, 120, 200);
        case 3: return rgb565(220, 120, 80);
        case 4: return rgb565(110, 175, 130);
        case 5: return rgb565(180, 140, 90);
        case 6: return rgb565(140, 185, 70);
      }
      return rgb565(120, 120, 120);
    }

    // The panel, in one place so the layout below and the tap-outside-to-dismiss
    // test in handlePlayTouch() cannot disagree about where its edges are.
    void playPanelRect(int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
      *x = 10;
      *y = 60;
      *w = SCREENWIDTH - 20;
      *h = 250;
    }

    // Two columns, filled left to right. One column would need buttons too short
    // to read, and the widest label ("Cat Bus Cross") needs ~87px, which is what
    // sets the 104px column width. An odd game out sits centred on its own row.
    void playButtonRect(int i, int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
      *w = 104;
      *h = 34;
      *y = 102 + (i / 2) * (*h + 6);
      bool aloneOnRow = (i == PET_PLAY_GAME_COUNT - 1) && (i % 2 == 0);
      if (aloneOnRow) {
        *x = (SCREENWIDTH - *w) / 2;
      } else {
        *x = (i % 2 == 0) ? 12 : 124;
      }
    }

    void backButtonRect(int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
      *w = 120;
      *h = 32;
      *x = (SCREENWIDTH - *w) / 2;
      *y = 264;
    }

    void drawPlayMenu() {
      int16_t px, py, pw, ph;
      playPanelRect(&px, &py, &pw, &ph);
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
        _tft->drawString(playGameLabel(i), x + w / 2, y + h / 2, 2);
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
      int16_t px, py, pw, ph;
      playPanelRect(&px, &py, &pw, &ph);
      if (!inRect(tx, ty, px, py, pw, ph)) {
        closeOverlays();
      }
    }

    // Pick a random sweet nothing and pop it in a speech bubble above Totoro.
#if defined(TINYML_GESTURE_INFERENCE)
    // Double-poked: Totoro says hello. Shares the one speech bubble with the love
    // note, so the two can never overlap on screen.
    void showGreeting(unsigned long now) {
      static const char *const kGreetings[] = {
        "Hi there, how are you",
        "How are you doing",
        "Totoro loves you",
        "Hello, hello!",
        "Nice to see you",
        "I missed you today",
        "Hope your day is good",
        "Totoro is happy now",
      };
      const int count = sizeof(kGreetings) / sizeof(kGreetings[0]);
      // Stand up and look pleased first: the bubble sits just above the pet, so it
      // wants to be the last thing drawn.
      commandPose(now, TOTORO_POSE_STAND, TOTORO_RGN_STAND, PET_ATTENTION_HOLD_MS);
      addSound(NOTE_G5, noteDurationMs(16, 900));
      addSound(NOTE_E5, noteDurationMs(16, 900));
      addSound(NOTE_G5, noteDurationMs(16, 900));
      showSpeech(kGreetings[random(0, count)]);
    }
#endif

    // Puts a line in the bubble and draws it. Callers pick their own sounds so a
    // greeting and a love note still sound different.
    void showSpeech(const char *msg) {
      strncpy(speechText, msg, sizeof(speechText) - 1);
      speechText[sizeof(speechText) - 1] = '\0';
      speechUntilMs = millis() + PET_SPEECH_HOLD_MS;
      drawSpeechBubble();
    }

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
      addSound(NOTE_E5, noteDurationMs(16, 900));
      addSound(NOTE_G5, noteDurationMs(16, 900));
      addSound(NOTE_C6, noteDurationMs(16, 900));
      showSpeech(kLoveLines[random(0, count)]);
    }

    // A rounded speech bubble with a little downward tail, floated above the pet.
    // Wraps onto a second line when one line would be wider than the screen, which
    // is what lets a greeting be a whole sentence instead of two words.
    void drawSpeechBubble() {
      if (speechText[0] == '\0') {
        return;
      }
      uint16_t paper = rgb565(255, 255, 255);
      uint16_t border = rgb565(70, 74, 70);
      uint16_t ink = rgb565(210, 60, 110);

      const int16_t maxBw = SCREENWIDTH - 16;
      char line1[sizeof(speechText)];
      char line2[sizeof(speechText)];
      splitSpeechLines(line1, line2, sizeof(line1), maxBw - 26);

      int16_t textW = _tft->textWidth(line1, 2);
      if (line2[0] != '\0') {
        const int16_t w2 = _tft->textWidth(line2, 2);
        if (w2 > textW) textW = w2;
      }
      int16_t bw = textW + 26;
      if (bw > maxBw) bw = maxBw;
      if (bw < 90) bw = 90;
      const int16_t lineH = 18;
      int16_t bh = (line2[0] != '\0') ? (34 + lineH) : 34;
      int16_t bx = (SCREENWIDTH - bw) / 2;
      // Grow upward, so the tail stays pinned just above Totoro's head.
      int16_t by = 184 - bh;

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
      if (line2[0] == '\0') {
        _tft->drawString(line1, SCREENWIDTH / 2, by + bh / 2, 2);
      } else {
        _tft->drawString(line1, SCREENWIDTH / 2, by + bh / 2 - lineH / 2, 2);
        _tft->drawString(line2, SCREENWIDTH / 2, by + bh / 2 + lineH / 2, 2);
      }
      _tft->setTextDatum(TL_DATUM);
    }

    // Breaks speechText at the word boundary that leaves the two halves most even,
    // among those that let the first half fit. Leaves line2 empty when the whole
    // string already fits, so short notes keep their single-line bubble.
    void splitSpeechLines(char *line1, char *line2, size_t cap, int16_t maxTextW) {
      line1[0] = '\0';
      line2[0] = '\0';
      strncpy(line1, speechText, cap - 1);
      line1[cap - 1] = '\0';
      if (_tft->textWidth(line1, 2) <= maxTextW) {
        return;
      }

      const int len = (int)strlen(speechText);
      int best = -1;
      int bestImbalance = 0;
      for (int i = 0; i < len; i++) {
        if (speechText[i] != ' ') {
          continue;
        }
        char head[sizeof(speechText)];
        memcpy(head, speechText, i);
        head[i] = '\0';
        if (_tft->textWidth(head, 2) > maxTextW) {
          break;  // every later break point is wider still
        }
        const int imbalance = abs((len - i - 1) - i);
        if (best < 0 || imbalance < bestImbalance) {
          best = i;
          bestImbalance = imbalance;
        }
      }
      if (best < 0) {
        return;  // one long word: let it be clipped rather than broken mid-word
      }

      memcpy(line1, speechText, best);
      line1[best] = '\0';
      strncpy(line2, speechText + best + 1, cap - 1);
      line2[cap - 1] = '\0';
    }

    void doPet() {
      if (petSession <= 0) {
        addSound(NOTE_A3, noteDurationMs(24, 600));  // Totoro has had enough pets for now
        return;
      }
      PetTotoroState::adjustHappiness(PET_PET_HAPPINESS);
      PetTotoroState::recordPetting();
      PetTotoroState::addCareXP(PET_CARE_XP_PET);
      petSession--;
      if (petSession == 0) {
        petCooldownUntilMs = millis() + PET_PET_COOLDOWN_MS;
      }
      mlLogCareState();
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
      mlLogCareState();
      addSound(NOTE_C5, noteDurationMs(16, 900));
      addSound(NOTE_E5, noteDurationMs(16, 900));
      addSound(NOTE_G5, noteDurationMs(16, 900));
    }

    // Consume a pending mini-game result: grant happiness and care-XP (a win
    // pays more; a loss still gives a small consolation) and queue a brief
    // toast. Safe to call every entry; it no-ops without a pending result.
    //
    // Coins are normally already banked and announced by the coin reward screen,
    // so takeCoins() usually returns 0 here and the toast is just the verdict.
    // It still pays out if a game ever hands back without that screen.
    void applyGameReward() {
      if (!GameResult::pending()) {
        return;
      }
      GameOutcome outcome = GameResult::outcome();
      int reportedHappiness = GameResult::happiness();
      int coins = GameResult::takeCoins();
      int happiness;
      const char *label;
      if (outcome == GAME_RESULT_WIN) {
        happiness = (reportedHappiness >= 0) ? reportedHappiness : GAME_WIN_HAPPINESS;
        PetTotoroState::addCareXP(GAME_WIN_CARE_XP);
        PetTotoroState::adjustExcitement(PET_GAME_WIN_EXCITEMENT);
        label = "Win!";
        addSound(NOTE_E5, noteDurationMs(10, 900));
        addSound(NOTE_G5, noteDurationMs(10, 900));
        addSound(NOTE_C6, noteDurationMs(10, 900));
      } else if (outcome == GAME_RESULT_NEUTRAL) {
        happiness = GAME_LOSS_HAPPINESS;
        PetTotoroState::addCareXP(GAME_LOSS_CARE_XP);
        PetTotoroState::adjustExcitement(PET_GAME_CASUAL_EXCITEMENT);
        label = "Good game!";
        addSound(NOTE_C5, noteDurationMs(12, 700));
        addSound(NOTE_E5, noteDurationMs(12, 700));
      } else {
        happiness = (reportedHappiness >= 0) ? reportedHappiness : GAME_LOSS_HAPPINESS;
        PetTotoroState::addCareXP(GAME_LOSS_CARE_XP);
        PetTotoroState::adjustExcitement(-PET_GAME_LOSE_EXCITEMENT);
        label = "Nice try!";
        addSound(NOTE_C5, noteDurationMs(12, 700));
        addSound(NOTE_E5, noteDurationMs(12, 700));
      }
      PetTotoroState::adjustHappiness(happiness);
      PetTotoroState::adjustHunger(-PET_GAME_PLAY_HUNGER_COST);
      PetTotoroState::adjustCleanness(-PET_GAME_PLAY_CLEAN_COST);
      if (coins > 0) {
        GameProgress::addCoins(coins);
        snprintf(rewardToast, sizeof(rewardToast), "%s +%d coins", label, coins);
      } else {
        snprintf(rewardToast, sizeof(rewardToast), "%s", label);
      }
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
      _tft->drawString("Totoro is unhappy - care for it!", SCREENWIDTH / 2, y + 10, 2);
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
