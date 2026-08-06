#ifndef _SCENE_ACORNCATCH_H_
#define _SCENE_ACORNCATCH_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "GameProgress.h"
#include "GameResult.h"
#include "Input.h"
#include "Physics.h"
#include "SpriteSheet.h"
#include "SpriteText.h"
#include "TouchInput.h"
#include "image_acorn_catch_bg.h"
#include "sprite_acorn.h"
#include "sprite_chu_totoro.h"
#include "sprite_digits.h"
#include "sprite_letters.h"
#include "sprite_mei.h"
#include "sprite_soot_mole.h"
#include "sprite_stopwatch.h"

#define ACORN_CATCH_TARGET 30
// Per-mode starting clocks.
#define ACORN_TIME_ATTACK_START_MS 30000
#define ACORN_COLLECTOR_START_MS 40000
// Time granted per acorn caught (tunable "make it easier" knobs).
#define ACORN_TIME_ATTACK_BONUS_MS 1000  // +1s per acorn in Time Attack
#define ACORN_COLLECTOR_BONUS_MS 500     // +0.5s per acorn in Collector
// Survival lives.
#define SURVIVAL_LIVES 3

#define MAX_FALLING_ACORNS 6
#define GROUND_Y 226
#define HUD_ZONE_Y 48
#define HUD_TIME_DIGIT_W 10
#define HUD_TIME_DIGIT_H 12
#define HUD_SCORE_DIGIT_W 24
#define HUD_SCORE_DIGIT_H 32
#define SPRITE_DIGITS_SMALL_BASE 0
#define SPRITE_DIGITS_LARGE_BASE 10
#define PLAYER_SPEED 4.0f
#define ENEMY_SPEED 3.0f
#define MEI_FRAME_W 24
#define MEI_FRAME_H 34
#define MEI_RUN_FRAMES 6
#define MEI_RUN_FRAME_MS 90
#define PLAYER_GROUND_Y (GROUND_Y - 8)
#define ACORN_FALL_SPEED 5.0f
// Each falling acorn gets a random constant speed in this range (px per 50ms).
#define ACORN_MIN_FALL_SPEED 3.0f
#define ACORN_MAX_FALL_SPEED 8.0f
// Acorns speed up the longer the round runs: this much is added to the spawn
// speed range per elapsed second, capped so it stays playable.
#define ACORN_SPEED_RAMP_PER_SEC 0.15f
#define ACORN_SPEED_RAMP_MAX 12.0f

// Soot-mole hazard: falls occasionally, costs acorns if it hits Mei.
// (Named uniquely so it doesn't clash with the pet scene's ACORN_MAX_SOOT.)
#define ACORN_MAX_SOOT 6
#define SOOT_SIZE 44
#define SOOT_VARIANTS 10
#define SOOT_MIN_FALL_SPEED 3.0f
#define SOOT_MAX_FALL_SPEED 6.0f
#define SOOT_SPAWN_MIN_MS 3500
#define SOOT_SPAWN_MAX_MS 7000
// Survival throws ~1.5x more soot: 1.5x the on-screen cap (4 -> 6) and spawns
// them ~1.5x as often (intervals divided by 1.5).
#define SOOT_SPAWN_MIN_SURVIVAL_MS 1067
#define SOOT_SPAWN_MAX_SURVIVAL_MS 2133
#define SOOT_MAX_CONCURRENT_DEFAULT 2
#define SOOT_MAX_CONCURRENT_SURVIVAL 6
#define SOOT_PENALTY 5
// Aiming: this % of soot drops are targeted to fall on Mei or Chu (the rest are
// fully random), with a small horizontal jitter so it isn't pixel-perfect. Mei
// is targeted more often so the soot feels out to get the player.
#define SOOT_AIM_CHANCE 65
#define SOOT_AIM_JITTER 18
#define SOOT_AIM_MEI_WEIGHT 7  // out of 10; the rest target Chu
// Remove a soot once this fraction of its height has passed below Mei's feet,
// so she can't step onto one resting near the ground.
#define SOOT_CLEAR_BELOW_FEET 0.8f
// After a soot lands on Chu it stops collecting immediately; once it touches
// the ground it is stunned (frozen in place) for this long.
#define CHU_STUN_MS 2000

// Sonic-style acorn scatter shown when Mei is hit (purely visual "explosion").
#define MAX_SCATTER_ACORNS 5
#define SCATTER_GRAVITY 0.6f

// Chu can jump to contest acorns at Mei's height.
#define CHU_JUMP_V (-7.0f)
#define CHU_GRAVITY 0.6f
#define CHU_JUMP_ALIGN_X 18
#define CHU_JUMP_CHANCE 60
#define CHU_JUMP_COOLDOWN_MS 900
// Ground line Chu's feet rest on (kept fixed so the bigger sprite still stands
// on the same floor as Mei). Chu's top-left Y is derived from its height.
#define CHU_FEET_Y 252
// Mei can jump too (Home tap). Single jump, full air control, no double jump.
#define MEI_JUMP_V (-7.5f)
#define MEI_GRAVITY 0.6f
// Physics step for jump/scatter, matching the acorn fall cadence.
#define PHYSICS_STEP_MS 50
#define MAX_END_MESSAGE_GLYPHS 16
#define END_MESSAGE_LINE1_Y 140
#define END_MESSAGE_LINE2_Y 165
#define END_MESSAGE_GAP 1

// Collector win pays coins/happiness equal to the seconds left on the clock
// (finish faster -> earn more), with a small floor so a last-second win pays.
#define COINS_PER_SECOND_LEFT 1
#define COIN_WIN_MINIMUM 1
// Results screen layout: title, the number (large digits), and a label.
#define WIN_TITLE_Y 118
#define WIN_COINS_NUM_Y 150
#define WIN_COINS_LABEL_Y 196
#define WIN_COINS_DIGIT_GAP 2

#define HUD_WATCH_X 2
#define HUD_WATCH_Y 2
#define HUD_TIME_X 42
#define HUD_TIME_Y 12
#define HUD_TIME_DIGIT_GAP 8
#define HUD_SCORE_Y 5
#define HUD_SCORE_DIGIT_GAP HUD_SCORE_DIGIT_W
#define HUD_ACORN_X (SCREENWIDTH - 34)
#define HUD_ACORN_Y 12
#define HUD_SCORE_ONES_X (HUD_ACORN_X - 8 - HUD_SCORE_DIGIT_W)
#define HUD_SCORE_TENS_X (HUD_SCORE_ONES_X - HUD_SCORE_DIGIT_GAP)
#define HUD_SCORE_HUNDREDS_X (HUD_SCORE_TENS_X - HUD_SCORE_DIGIT_GAP)
// Lives row (Survival) drawn as little hearts under the clock.
#define HUD_LIVES_Y 32
#define HUD_LIVES_X0 12
#define HUD_LIVES_GAP 20

enum AcornMode {
  ACORN_MODE_TIME_ATTACK = 0,
  ACORN_MODE_SURVIVAL = 1,
  ACORN_MODE_COLLECTOR = 2
};
#define ACORN_MODE_COUNT 3

// Mode-select card layout (shared by the drawing and the touch hit-test so a
// tapped card matches exactly what is on screen).
#define ACORN_MODE_ROW_X 20
#define ACORN_MODE_ROW_Y0 90
#define ACORN_MODE_ROW_H 40
#define ACORN_MODE_ROW_GAP 4
#define ACORN_MODE_ROW_W (SCREENWIDTH - 2 * ACORN_MODE_ROW_X)

enum AcornCatchState {
  ACORN_STATE_MODE_SELECT,
  ACORN_STATE_PLAYING,
  ACORN_STATE_WON,
  ACORN_STATE_LOST
};

// Ignore button presses for a moment so the press that launched the scene
// doesn't immediately act on the mode-select screen.
#define INTRO_INPUT_GRACE_MS 300

class Scene_AcornCatch : public GameScene {
  public:
    Scene_AcornCatch(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        // Only after the round has ended does Home return to the pet home.
        if (input.homePressed) {
          *needChangeScene = true;
          *nextSceneIndex = SCENE_PET_TOTORO;
        }
        return;
      }

      if (state == ACORN_STATE_MODE_SELECT) {
        if ((now - stateStartMs) > INTRO_INPUT_GRACE_MS) {
          if (input.leftPressed) {
            selectedMode = (selectedMode + ACORN_MODE_COUNT - 1) % ACORN_MODE_COUNT;
            addSound(NOTE_A4, noteDurationMs(24, 700));
            drawModeSelect();
          } else if (input.rightPressed) {
            selectedMode = (selectedMode + 1) % ACORN_MODE_COUNT;
            addSound(NOTE_A4, noteDurationMs(24, 700));
            drawModeSelect();
          } else if (input.homePressed) {
            mode = (AcornMode)selectedMode;
            startGame(now);
          } else if (isTouching && !wasTouching) {
            // Tapping a mode card selects and launches that mode directly, like
            // clicking a button (matching the radial menu's touch behaviour).
            uint16_t touchX = 0;
            uint16_t touchY = 0;
            if (getTouchPoint(_tft, &touchX, &touchY)) {
              int picked = modeAtPoint(touchX, touchY);
              if (picked >= 0) {
                selectedMode = picked;
                mode = (AcornMode)selectedMode;
                addSound(NOTE_A4, noteDurationMs(24, 700));
                startGame(now);
                wasTouching = isTouching;
                return;
              }
            }
          }
        }
        wasTouching = isTouching;
        return;
      }

      // During gameplay Home only makes Mei jump; it never exits to the pet home
      // (the round can only be left once it is won or lost).
      if (input.homePressed) {
        tryPlayerJump(now);
      }

      bool step = false;
      if (now >= nextStepMs) {
        step = true;
        nextStepMs = now + PHYSICS_STEP_MS;
      }

      updatePlayer(input);
      updatePlayerJump(step);
      updateEnemy(now, step);
      updateAcorns(now);
      updateSoot(now);
      updateScatter(step);
      if (checkCollisions()) {
        return;
      }
      updateHudAvatars(now);

      // Timed modes end when the clock runs out.
      if (hasTimer && now >= gameEndMs) {
        if (mode == ACORN_MODE_COLLECTOR) {
          loseCollector();
        } else {
          endTimeAttack();
        }
        return;
      }

      // Collector wins the instant the target is reached.
      if (mode == ACORN_MODE_COLLECTOR && score >= ACORN_CATCH_TARGET) {
        winCollector(now);
        return;
      }

      requestRender();
    }

    void render() {
      // The mode-select screen is drawn directly to the TFT; the avatar renderer
      // stays idle until a mode is chosen.
      if (state == ACORN_STATE_MODE_SELECT) {
        return;
      }
      renderScene();
    }

    void initScene() {
      chuGroundY = CHU_FEET_Y - SPRITE_CHU_TOTORO_HEIGHT;

      setBackgroundAsset(&acorn_catch_bg);
      drawBackgroundAsset(&acorn_catch_bg);

      initHudAvatars();

      player = meiSheet().createAvatar(109, PLAYER_GROUND_Y,
                                       SpriteSheet::readRegion(sprite_meiRegions, 0));
      player->updateInterval = 50;
      appendAvatar(player);
      meiFrame = 0;
      meiFacingRight = true;
      meiFrameMs = millis();

      enemy = new Avatar(170, chuGroundY, SPRITE_CHU_TOTORO_WIDTH, SPRITE_CHU_TOTORO_HEIGHT, sprite_chu_totoro, sprite_chu_totoroMask);
      enemy->setVelocity(0, 0);
      enemy->updateInterval = 50;
      appendAvatar(enemy);

      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        acorns[i] = new Avatar(-40, SCREENHEIGHT + 20, SPRITE_ACORN_WIDTH, SPRITE_ACORN_HEIGHT, NULL, NULL);
        acorns[i]->setSpriteAsset(&sprite_acorn);
        acorns[i]->setVelocity(0, 0);
        acorns[i]->updateInterval = 50;
        acornActive[i] = false;
        acornSpeed[i] = ACORN_FALL_SPEED;
        appendAvatar(acorns[i]);
      }

      SpriteSheet sootSh = sootSheet();
      for (int i = 0; i < ACORN_MAX_SOOT; i++) {
        soots[i] = sootSh.createAvatar(-SOOT_SIZE - 20, SCREENHEIGHT + 60,
                                       SpriteSheet::readRegion(sprite_soot_moleRegions, 0));
        soots[i]->updateInterval = 50;
        sootActive[i] = false;
        appendAvatar(soots[i]);
      }

      for (int i = 0; i < MAX_SCATTER_ACORNS; i++) {
        scatterAcorns[i] = new Avatar(-40, SCREENHEIGHT + 40, SPRITE_ACORN_WIDTH, SPRITE_ACORN_HEIGHT,
                                      NULL, NULL);
        scatterAcorns[i]->setSpriteAsset(&sprite_acorn);
        scatterAcorns[i]->setVelocity(0, 0);
        scatterAcorns[i]->updateInterval = 50;
        scatterActive[i] = false;
        appendAvatar(scatterAcorns[i]);
      }

      chuJumping = false;
      chuVy = 0;
      chuJumpVx = 0;
      chuY = chuGroundY;
      lastChuJumpMs = 0;
      chuHitPending = false;
      chuStunUntilMs = 0;
      meiJumping = false;
      meiVy = 0;
      meiY = PLAYER_GROUND_Y;
      lastMeiJumpMs = 0;
      nextSootMs = 0;
      nextStepMs = 0;

      score = 0;
      caughtCount = 0;
      coinsEarned = 0;
      lives = SURVIVAL_LIVES;
      selectedMode = ACORN_MODE_TIME_ATTACK;
      state = ACORN_STATE_MODE_SELECT;
      stateStartMs = millis();
      // Ignore a still-held tap carried over from the radial menu until released.
      wasTouching = true;
      nextSpawnMs = 0;
      clearEndMessageAvatars();
      resetHudCache();
      drawModeSelect();
    }

    void destroyScene() {
      player = NULL;
      enemy = NULL;
      hudWatch = NULL;
      hudTimeTens = NULL;
      hudTimeOnes = NULL;
      hudScoreHundreds = NULL;
      hudScoreTens = NULL;
      hudScoreOnes = NULL;
      hudAcorn = NULL;
      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        acorns[i] = NULL;
        acornActive[i] = false;
      }
      for (int i = 0; i < ACORN_MAX_SOOT; i++) {
        soots[i] = NULL;
        sootActive[i] = false;
      }
      for (int i = 0; i < MAX_SCATTER_ACORNS; i++) {
        scatterAcorns[i] = NULL;
        scatterActive[i] = false;
      }
      clearEndMessageAvatars();
      GameScene::destroyScene();
    }

  private:
    Avatar *player = NULL;
    Avatar *enemy = NULL;
    Avatar *acorns[MAX_FALLING_ACORNS];
    float acornSpeed[MAX_FALLING_ACORNS];
    Avatar *soots[ACORN_MAX_SOOT];
    bool sootActive[ACORN_MAX_SOOT];
    float sootSpeed[ACORN_MAX_SOOT];
    unsigned long nextSootMs = 0;
    Avatar *scatterAcorns[MAX_SCATTER_ACORNS];
    bool scatterActive[MAX_SCATTER_ACORNS];
    float scatterVx[MAX_SCATTER_ACORNS];
    float scatterVy[MAX_SCATTER_ACORNS];
    unsigned long nextStepMs = 0;
    bool chuJumping = false;
    bool chuFacingRight = true;  // sprite's default orientation; flip when moving left
    float chuVy = 0;
    float chuJumpVx = 0;
    float chuY = 0;
    unsigned long lastChuJumpMs = 0;
    // Soot-hit state: chuHitPending means "hit but still finishing its jump"
    // (can't collect, not yet stunned). chuStunUntilMs != 0 means grounded and
    // frozen in place until that time. Both block acorn collection.
    bool chuHitPending = false;
    unsigned long chuStunUntilMs = 0;
    int chuGroundY = 0;
    bool meiJumping = false;
    float meiVy = 0;
    float meiY = PLAYER_GROUND_Y;
    unsigned long lastMeiJumpMs = 0;
    Avatar *hudWatch = NULL;
    Avatar *hudTimeTens = NULL;
    Avatar *hudTimeOnes = NULL;
    Avatar *hudScoreHundreds = NULL;
    Avatar *hudScoreTens = NULL;
    Avatar *hudScoreOnes = NULL;
    Avatar *hudAcorn = NULL;
    bool acornActive[MAX_FALLING_ACORNS];

    int score = 0;
    int caughtCount = 0;
    int coinsEarned = 0;
    int lives = SURVIVAL_LIVES;
    int meiFrame = 0;
    bool meiFacingRight = true;
    unsigned long meiFrameMs = 0;
    AcornCatchState state = ACORN_STATE_MODE_SELECT;
    AcornMode mode = ACORN_MODE_TIME_ATTACK;
    int selectedMode = ACORN_MODE_TIME_ATTACK;
    bool hasTimer = true;
    bool hasTarget = false;
    bool wasTouching = false;
    int sootMaxConcurrent = SOOT_MAX_CONCURRENT_DEFAULT;
    unsigned long sootSpawnMinMs = SOOT_SPAWN_MIN_MS;
    unsigned long sootSpawnMaxMs = SOOT_SPAWN_MAX_MS;
    unsigned long stateStartMs = 0;
    unsigned long gameEndMs = 0;
    unsigned long nextSpawnMs = 0;
    int lastHudTimeLeft = -1;
    int lastHudScore = -1;
    int lastHudLives = -1;
    Avatar *endMessageAvatars[MAX_END_MESSAGE_GLYPHS];
    int endMessageAvatarCount = 0;
    bool endMessageBuilt = false;

    void resetHudCache() {
      lastHudTimeLeft = -1;
      lastHudScore = -1;
      lastHudLives = -1;
    }

    SpriteSheet digitSheet() const {
      return SpriteSheet(sprite_digits, sprite_digitsMask, SPRITE_DIGITS_WIDTH, SPRITE_DIGITS_HEIGHT);
    }

    SpriteSheet meiSheet() const {
      return SpriteSheet(sprite_mei, sprite_meiMask, SPRITE_MEI_WIDTH, SPRITE_MEI_HEIGHT);
    }

    SpriteSheet sootSheet() const {
      return SpriteSheet(sprite_soot_mole, sprite_soot_moleMask, SPRITE_SOOT_MOLE_WIDTH, SPRITE_SOOT_MOLE_HEIGHT);
    }

    // Random speed in [lo, hi] at 0.1 px resolution.
    float randSpeed(float lo, float hi) const {
      return lo + (float)random(0, (int)((hi - lo) * 10.0f) + 1) / 10.0f;
    }

    // Six right-facing run frames (0..5). Left-facing is done at draw time via
    // the Avatar's runtime horizontal flip, so no mirrored frames are stored.
    void applyMeiFrame() {
      player->setFlipX(!meiFacingRight);
      meiSheet().applyRegion(player, SpriteSheet::readRegion(sprite_meiRegions, meiFrame));
      player->requestRedraw();
    }

    void setHudDigit(Avatar *avatar, int digit, bool large) {
      if (avatar == NULL || digit < 0 || digit > 9) {
        return;
      }
      int index = digit + (large ? SPRITE_DIGITS_LARGE_BASE : SPRITE_DIGITS_SMALL_BASE);
      digitSheet().applyRegion(avatar, SpriteSheet::readRegion(sprite_digitsRegions, index));
      avatar->requestRedraw();
    }

    void initHudAvatars() {
      hudWatch = new Avatar(HUD_WATCH_X, HUD_WATCH_Y, SPRITE_STOPWATCH_WIDTH, SPRITE_STOPWATCH_HEIGHT,
                            sprite_stopwatch, sprite_stopwatchMask);
      hudWatch->setVelocity(0, 0);
      hudWatch->updateInterval = 50;
      appendAvatar(hudWatch);

      SpriteSheet sheet = digitSheet();
      hudTimeTens = sheet.createAvatar(HUD_TIME_X, HUD_TIME_Y,
                                       SpriteSheet::readRegion(sprite_digitsRegions, SPRITE_DIGITS_SMALL_BASE));
      hudTimeOnes = sheet.createAvatar(HUD_TIME_X + HUD_TIME_DIGIT_W + HUD_TIME_DIGIT_GAP, HUD_TIME_Y,
                                       SpriteSheet::readRegion(sprite_digitsRegions, SPRITE_DIGITS_SMALL_BASE));
      appendAvatar(hudTimeTens);
      appendAvatar(hudTimeOnes);

      hudScoreHundreds = sheet.createAvatar(HUD_SCORE_HUNDREDS_X, HUD_SCORE_Y,
                                            SpriteSheet::readRegion(sprite_digitsRegions, SPRITE_DIGITS_LARGE_BASE));
      hudScoreTens = sheet.createAvatar(HUD_SCORE_TENS_X, HUD_SCORE_Y,
                                      SpriteSheet::readRegion(sprite_digitsRegions, SPRITE_DIGITS_LARGE_BASE));
      hudScoreOnes = sheet.createAvatar(HUD_SCORE_ONES_X, HUD_SCORE_Y,
                                        SpriteSheet::readRegion(sprite_digitsRegions, SPRITE_DIGITS_LARGE_BASE));
      appendAvatar(hudScoreHundreds);
      appendAvatar(hudScoreTens);
      appendAvatar(hudScoreOnes);

      hudAcorn = new Avatar(HUD_ACORN_X, HUD_ACORN_Y, SPRITE_ACORN_WIDTH, SPRITE_ACORN_HEIGHT, NULL, NULL);
      hudAcorn->setSpriteAsset(&sprite_acorn);
      hudAcorn->setVelocity(0, 0);
      hudAcorn->updateInterval = 50;
      appendAvatar(hudAcorn);
    }

    void updateHudAvatars(unsigned long now) {
      int timeNumber = 0;
      if (state == ACORN_STATE_PLAYING) {
        if (mode == ACORN_MODE_SURVIVAL) {
          timeNumber = (int)((now - stateStartMs) / 1000);  // count up
        } else {
          timeNumber = (gameEndMs > now) ? (int)((gameEndMs - now) / 1000) : 0;
        }
      }

      int displayScore = score > 999 ? 999 : score;
      if (timeNumber == lastHudTimeLeft && displayScore == lastHudScore && lives == lastHudLives) {
        return;
      }

      setHudDigit(hudTimeTens, (timeNumber / 10) % 10, false);
      setHudDigit(hudTimeOnes, timeNumber % 10, false);
      setHudDigit(hudScoreHundreds, displayScore / 100, true);
      setHudDigit(hudScoreTens, (displayScore / 10) % 10, true);
      setHudDigit(hudScoreOnes, displayScore % 10, true);

      if (mode == ACORN_MODE_SURVIVAL && lives != lastHudLives) {
        drawLives(lives);
      }

      lastHudTimeLeft = timeNumber;
      lastHudScore = displayScore;
      lastHudLives = lives;
      requestRender();
    }

    // Little hearts under the clock; only shown in Survival. Redrawing over the
    // same spots (filled -> hollow) covers the previous heart, so no background
    // repaint is needed (they sit above the falling-sprite zone).
    void drawLives(int livesLeft) {
      for (int i = 0; i < SURVIVAL_LIVES; i++) {
        drawHeart(HUD_LIVES_X0 + i * HUD_LIVES_GAP, HUD_LIVES_Y, i < livesLeft);
      }
    }

    void drawHeart(int cx, int cy, bool filled) {
      uint16_t col = filled ? rgb565(235, 70, 70) : rgb565(60, 66, 60);
      _tft->fillCircle(cx - 3, cy, 4, col);
      _tft->fillCircle(cx + 3, cy, 4, col);
      _tft->fillTriangle(cx - 6, cy + 2, cx + 6, cy + 2, cx, cy + 9, col);
    }

    // Which mode card (0..2) contains the point, or -1 if none.
    int modeAtPoint(int px, int py) const {
      for (int i = 0; i < ACORN_MODE_COUNT; i++) {
        int ry = ACORN_MODE_ROW_Y0 + i * (ACORN_MODE_ROW_H + ACORN_MODE_ROW_GAP);
        if (px >= ACORN_MODE_ROW_X && px < ACORN_MODE_ROW_X + ACORN_MODE_ROW_W &&
            py >= ry && py < ry + ACORN_MODE_ROW_H) {
          return i;
        }
      }
      return -1;
    }

    const char *modeName(int m) const {
      switch (m) {
        case ACORN_MODE_TIME_ATTACK: return "TIME ATTACK";
        case ACORN_MODE_SURVIVAL: return "SURVIVAL";
        case ACORN_MODE_COLLECTOR: return "COLLECTOR";
      }
      return "";
    }

    const char *modeDesc1(int m) const {
      switch (m) {
        case ACORN_MODE_TIME_ATTACK: return "30s clock, +1s per acorn";
        case ACORN_MODE_SURVIVAL: return "3 lives, dodge the soot";
        case ACORN_MODE_COLLECTOR: return "Collect 30 acorns";
      }
      return "";
    }

    const char *modeDesc2(int m) const {
      switch (m) {
        case ACORN_MODE_TIME_ATTACK: return "Grab all you can!";
        case ACORN_MODE_SURVIVAL: return "Last as long as you can!";
        case ACORN_MODE_COLLECTOR: return "before time runs out";
      }
      return "";
    }

    // Mode picker: LEFT/RIGHT to change the highlighted mode, HOME to start.
    void drawModeSelect() {
      uint16_t bg = rgb565(34, 60, 40);       // forest green
      uint16_t card = rgb565(24, 44, 30);
      uint16_t cardSel = rgb565(70, 120, 80);
      uint16_t border = rgb565(120, 160, 120);
      setBackgroundColor(bg);
      _tft->fillScreen(bg);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(240, 220, 120), bg);
      _tft->drawString("ACORN CATCH", SCREENWIDTH / 2, 34, 4);
      _tft->setTextColor(rgb565(200, 220, 255), bg);
      _tft->drawString("Choose a mode", SCREENWIDTH / 2, 62, 2);

      for (int i = 0; i < ACORN_MODE_COUNT; i++) {
        int ry = ACORN_MODE_ROW_Y0 + i * (ACORN_MODE_ROW_H + ACORN_MODE_ROW_GAP);
        bool sel = (i == selectedMode);
        _tft->fillRoundRect(ACORN_MODE_ROW_X, ry, ACORN_MODE_ROW_W, ACORN_MODE_ROW_H, 8, sel ? cardSel : card);
        _tft->drawRoundRect(ACORN_MODE_ROW_X, ry, ACORN_MODE_ROW_W, ACORN_MODE_ROW_H, 8, sel ? rgb565(230, 240, 200) : border);
        _tft->setTextColor(sel ? TFT_WHITE : rgb565(180, 200, 180), sel ? cardSel : card);
        _tft->drawString(modeName(i), SCREENWIDTH / 2, ry + ACORN_MODE_ROW_H / 2, 4);
      }

      int descY = ACORN_MODE_ROW_Y0 + ACORN_MODE_COUNT * (ACORN_MODE_ROW_H + ACORN_MODE_ROW_GAP) + 8;
      _tft->setTextColor(rgb565(230, 230, 170), bg);
      _tft->drawString(modeDesc1(selectedMode), SCREENWIDTH / 2, descY, 2);
      _tft->drawString(modeDesc2(selectedMode), SCREENWIDTH / 2, descY + 20, 2);

      _tft->setTextColor(rgb565(180, 240, 180), bg);
      _tft->drawString("LEFT / RIGHT choose   HOME start", SCREENWIDTH / 2, 300, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // Leave the mode-select screen and start the actual round.
    void startGame(unsigned long now) {
      drawBackgroundAsset(&acorn_catch_bg);
      resetHudCache();
      beginPlay(now);
      updateHudAvatars(now);
      renderFullScreen();
      if (mode == ACORN_MODE_SURVIVAL) {
        drawLives(lives);
      }
    }

    void beginPlay(unsigned long now) {
      state = ACORN_STATE_PLAYING;
      stateStartMs = now;
      chuHitPending = false;
      chuStunUntilMs = 0;
      lives = SURVIVAL_LIVES;
      nextStepMs = now + PHYSICS_STEP_MS;
      nextSpawnMs = now + 500;

      switch (mode) {
        case ACORN_MODE_TIME_ATTACK:
          hasTimer = true;
          hasTarget = false;
          gameEndMs = now + ACORN_TIME_ATTACK_START_MS;
          sootMaxConcurrent = SOOT_MAX_CONCURRENT_DEFAULT;
          sootSpawnMinMs = SOOT_SPAWN_MIN_MS;
          sootSpawnMaxMs = SOOT_SPAWN_MAX_MS;
          break;
        case ACORN_MODE_SURVIVAL:
          hasTimer = false;
          hasTarget = false;
          gameEndMs = 0;
          sootMaxConcurrent = SOOT_MAX_CONCURRENT_SURVIVAL;
          sootSpawnMinMs = SOOT_SPAWN_MIN_SURVIVAL_MS;
          sootSpawnMaxMs = SOOT_SPAWN_MAX_SURVIVAL_MS;
          break;
        case ACORN_MODE_COLLECTOR:
        default:
          hasTimer = true;
          hasTarget = true;
          gameEndMs = now + ACORN_COLLECTOR_START_MS;
          sootMaxConcurrent = SOOT_MAX_CONCURRENT_DEFAULT;
          sootSpawnMinMs = SOOT_SPAWN_MIN_MS;
          sootSpawnMaxMs = SOOT_SPAWN_MAX_MS;
          break;
      }

      nextSootMs = now + random(sootSpawnMinMs, sootSpawnMaxMs);
      resetHudCache();
      addSound(NOTE_C5, noteDurationMs(8, 800));
      requestRender();
    }

    // Add the per-mode time bonus for catching one acorn.
    void addAcornTimeBonus() {
      if (mode == ACORN_MODE_TIME_ATTACK) {
        gameEndMs += ACORN_TIME_ATTACK_BONUS_MS;  // no cap: pure time attack
      } else if (mode == ACORN_MODE_COLLECTOR) {
        unsigned long capEnd = millis() + ACORN_COLLECTOR_START_MS;
        gameEndMs += ACORN_COLLECTOR_BONUS_MS;
        if (gameEndMs > capEnd) {
          gameEndMs = capEnd;
        }
      }
    }

    void winCollector(unsigned long now) {
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_WON;
      freezeGameplay();
      resetHudCache();

      int secondsLeft = (gameEndMs > now) ? (int)((gameEndMs - now) / 1000) : 0;
      coinsEarned = secondsLeft * COINS_PER_SECOND_LEFT;
      if (coinsEarned < COIN_WIN_MINIMUM) {
        coinsEarned = COIN_WIN_MINIMUM;
      }
      GameResult::report(GAME_RESULT_WIN, coinsEarned, secondsLeft);

      updateHudAvatars(millis());
      showResultScreen("YOU WIN", coinsEarned, "COINS");
      addSound(NOTE_G5, noteDurationMs(4, 800));
      addSound(NOTE_C6, noteDurationMs(4, 800));
      addSound(NOTE_E6, noteDurationMs(2, 800));
      requestRender();
    }

    void loseCollector() {
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_LOST;
      GameResult::report(GAME_RESULT_LOSS, 0, -1);  // pet grants the consolation
      freezeGameplay();
      resetHudCache();
      updateHudAvatars(millis());
      showEndMessage("TIME UP", "GAME OVER");
      addSound(NOTE_G3, noteDurationMs(4, 600));
      addSound(NOTE_E3, noteDurationMs(4, 600));
      requestRender();
    }

    // Time Attack: the clock ran out. Coins = acorns collected, happiness =
    // seconds actually played.
    void endTimeAttack() {
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_WON;
      freezeGameplay();
      resetHudCache();

      int playedSec = (int)((millis() - stateStartMs) / 1000);
      coinsEarned = score;
      GameResult::report(GAME_RESULT_WIN, coinsEarned, playedSec);

      updateHudAvatars(millis());
      showResultScreen("TIME UP", coinsEarned, "COINS");
      addSound(NOTE_G5, noteDurationMs(6, 800));
      addSound(NOTE_C6, noteDurationMs(4, 800));
      requestRender();
    }

    // Survival: Mei ran out of lives. Coins = acorns collected, happiness =
    // seconds survived.
    void endSurvival() {
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_LOST;
      freezeGameplay();
      resetHudCache();

      int survivedSec = (int)((millis() - stateStartMs) / 1000);
      coinsEarned = score;
      // Still a positive play session, so grant the collected coins + time-based
      // happiness (reported as a "win" so care-XP uses the better tier).
      GameResult::report(GAME_RESULT_WIN, coinsEarned, survivedSec);

      updateHudAvatars(millis());
      showResultScreen("GAME OVER", coinsEarned, "COINS");
      addSound(NOTE_G3, noteDurationMs(6, 600));
      addSound(NOTE_E3, noteDurationMs(6, 600));
      requestRender();
    }

    void freezeGameplay() {
      player->setVelocity(0, 0);
      enemy->setVelocity(0, 0);
      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        if (acornActive[i]) {
          deactivateAcorn(i);
        }
      }
      for (int i = 0; i < ACORN_MAX_SOOT; i++) {
        if (sootActive[i]) {
          deactivateSoot(i);
        }
      }
      for (int i = 0; i < MAX_SCATTER_ACORNS; i++) {
        scatterActive[i] = false;
        scatterAcorns[i]->setPos(-40, SCREENHEIGHT + 40);
      }
      chuJumping = false;
      chuVy = 0;
      chuY = chuGroundY;
      chuHitPending = false;
      chuStunUntilMs = 0;
      enemy->y = chuGroundY;
      meiJumping = false;
      meiVy = 0;
      meiY = PLAYER_GROUND_Y;
      player->y = PLAYER_GROUND_Y;
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

    // Results screen: a title line, a big number, and a label under it. All
    // glyphs are tracked so they are cleaned up with the scene.
    void showResultScreen(const char *title, int number, const char *label) {
      if (endMessageBuilt) {
        return;
      }
      endMessageBuilt = true;
      endMessageAvatarCount = 0;

      Avatar *glyphs[MAX_END_MESSAGE_GLYPHS];

      int count = SpriteText::buildCenteredLine(this, title, WIN_TITLE_Y, glyphs,
                                                MAX_END_MESSAGE_GLYPHS, END_MESSAGE_GAP);
      for (int i = 0; i < count; i++) {
        endMessageAvatars[endMessageAvatarCount++] = glyphs[i];
      }

      count = buildCenteredNumber(number, WIN_COINS_NUM_Y, glyphs,
                                  MAX_END_MESSAGE_GLYPHS - endMessageAvatarCount);
      for (int i = 0; i < count; i++) {
        endMessageAvatars[endMessageAvatarCount++] = glyphs[i];
      }

      count = SpriteText::buildCenteredLine(this, label, WIN_COINS_LABEL_Y, glyphs,
                                            MAX_END_MESSAGE_GLYPHS - endMessageAvatarCount, END_MESSAGE_GAP);
      for (int i = 0; i < count; i++) {
        endMessageAvatars[endMessageAvatarCount++] = glyphs[i];
      }

      renderFullScreen();
    }

    // Lay out `value` as centered large-digit glyphs on row `y`, using the same
    // digit sheet as the HUD score. Returns the number of glyphs created.
    int buildCenteredNumber(int value, int y, Avatar *out[], int maxOut) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%d", value);
      int digits = (int)strlen(buf);
      int totalWidth = digits * HUD_SCORE_DIGIT_W + (digits - 1) * WIN_COINS_DIGIT_GAP;
      int cursor = (SCREENWIDTH - totalWidth) / 2;

      int count = 0;
      SpriteSheet sheet = digitSheet();
      for (const char *p = buf; *p != '\0' && count < maxOut; ++p) {
        int digit = *p - '0';
        SpriteSheetRegion region = SpriteSheet::readRegion(sprite_digitsRegions,
                                                           digit + SPRITE_DIGITS_LARGE_BASE);
        Avatar *glyph = sheet.createAvatar((float)cursor, (float)y, region);
        appendAvatar(glyph);
        out[count++] = glyph;
        cursor += HUD_SCORE_DIGIT_W + WIN_COINS_DIGIT_GAP;
      }
      return count;
    }

    void updatePlayer(const GameInput &input) {
      float vx = 0;
      bool prevFacing = meiFacingRight;
      if (input.left) {
        vx = -PLAYER_SPEED;
        meiFacingRight = false;
      } else if (input.right) {
        vx = PLAYER_SPEED;
        meiFacingRight = true;
      }
      player->setVelocity(vx, 0);
      player->updatePos(millis());

      if (player->x < 0) {
        player->x = 0;
      }
      if (player->x + player->width > SCREENWIDTH) {
        player->x = SCREENWIDTH - player->width;
      }
      if (!meiJumping) {
        player->y = PLAYER_GROUND_Y;  // vertical is owned by updatePlayerJump while airborne
      }

      unsigned long now = millis();
      if (vx != 0) {
        bool advance = (now - meiFrameMs) >= MEI_RUN_FRAME_MS;
        if (advance) {
          meiFrame = (meiFrame + 1) % MEI_RUN_FRAMES;
          meiFrameMs = now;
        }
        if (advance || meiFacingRight != prevFacing) {
          applyMeiFrame();
          requestRender();
        }
      } else if (meiFrame != 0 || meiFacingRight != prevFacing) {
        meiFrame = 0;  // settle on the first pose when standing still
        applyMeiFrame();
        requestRender();
      }
    }

    // Start a single jump from the ground. No double jump: ignored while airborne.
    void tryPlayerJump(unsigned long now) {
      if (meiJumping || player == NULL) {
        return;
      }
      meiJumping = true;
      meiVy = MEI_JUMP_V;
      meiY = player->y;
      lastMeiJumpMs = now;
      addSound(NOTE_C5, noteDurationMs(16, 900));
    }

    // Integrate Mei's jump arc on physics steps; full horizontal air control is
    // handled separately in updatePlayer().
    void updatePlayerJump(bool step) {
      if (!meiJumping || player == NULL) {
        return;
      }
      if (step) {
        meiY += meiVy;
        meiVy += MEI_GRAVITY;
        if (meiY >= PLAYER_GROUND_Y) {
          meiY = PLAYER_GROUND_Y;
          meiJumping = false;
          meiVy = 0;
        }
      }
      player->y = meiY;
      requestRender();
    }

    void updateEnemy(unsigned long now, bool step) {
      // Grounded-and-stunned Chu stands still and cannot collect acorns.
      if (chuStunUntilMs != 0 && now >= chuStunUntilMs) {
        chuStunUntilMs = 0;
      }
      if (chuStunUntilMs != 0) {
        chuJumping = false;
        chuVy = 0;
        chuY = chuGroundY;
        enemy->setVelocity(0, 0);
        enemy->y = chuGroundY;
        enemy->updatePos(now);
        return;
      }

      float vx;

      if (chuJumping) {
        // Direction is locked for the whole jump so an airborne Chu can't steer
        // onto Mei, keeping it from being overpowering. (A soot-hit Chu keeps
        // falling on this locked path until it lands, then gets stunned.)
        vx = chuJumpVx;
      } else if (chuHitPending) {
        // Hit while already grounded (not jumping): begin the stun right away.
        chuHitPending = false;
        chuStunUntilMs = now + CHU_STUN_MS;
        enemy->setVelocity(0, 0);
        enemy->y = chuGroundY;
        enemy->updatePos(now);
        return;
      } else {
        vx = 0;
        Avatar *target = findNearestAcorn();
        if (target != NULL) {
          if (enemy->x + enemy->width / 2 < target->x + 2) {
            vx = ENEMY_SPEED;
          } else if (enemy->x + enemy->width / 2 > target->x + target->width - 2) {
            vx = -ENEMY_SPEED;
          }
        } else if (player != NULL) {
          if (enemy->x < player->x - 10) {
            vx = ENEMY_SPEED * 0.6f;
          } else if (enemy->x > player->x + 10) {
            vx = -ENEMY_SPEED * 0.6f;
          }
        }

        // Jump to contest acorns at Mei's height when lined up with her. Single
        // jump only (guarded by chuJumping) with a cooldown.
        if (step && player != NULL && (now - lastChuJumpMs) >= CHU_JUMP_COOLDOWN_MS) {
          int chuCx = (int)(enemy->x + enemy->width / 2);
          int meiCx = (int)(player->x + player->width / 2);
          if (abs(chuCx - meiCx) < CHU_JUMP_ALIGN_X && random(0, 100) < CHU_JUMP_CHANCE) {
            chuJumping = true;
            chuVy = CHU_JUMP_V;
            chuJumpVx = vx;
            lastChuJumpMs = now;
          }
        }
      }

      // Face the direction of travel (mirror the sprite when moving leftward).
      if (vx > 0 && !chuFacingRight) {
        chuFacingRight = true;
        enemy->setFlipX(false);
        enemy->requestRedraw();
      } else if (vx < 0 && chuFacingRight) {
        chuFacingRight = false;
        enemy->setFlipX(true);
        enemy->requestRedraw();
      }

      enemy->setVelocity(vx, 0);
      enemy->updatePos(now);

      if (enemy->x < 0) {
        enemy->x = 0;
      }
      if (enemy->x + enemy->width > SCREENWIDTH) {
        enemy->x = SCREENWIDTH - enemy->width;
      }

      if (chuJumping && step) {
        chuY += chuVy;
        chuVy += CHU_GRAVITY;
        if (chuY >= chuGroundY) {
          chuY = chuGroundY;
          chuJumping = false;
          chuVy = 0;
          // If Chu was hit mid-jump, the stun begins the moment it lands.
          if (chuHitPending) {
            chuHitPending = false;
            chuStunUntilMs = now + CHU_STUN_MS;
          }
        }
      }
      enemy->y = chuJumping ? chuY : (float)chuGroundY;
    }

    // Chu may collect acorns only when it hasn't been soot-hit (neither falling
    // after a hit nor grounded-and-stunned).
    bool chuCanCollect() const {
      return !chuHitPending && chuStunUntilMs == 0;
    }

    Avatar *findNearestAcorn() {
      Avatar *nearest = NULL;
      float bestDist = 99999.0f;

      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        if (!acornActive[i]) {
          continue;
        }
        float dx = acorns[i]->x - enemy->x;
        float dy = acorns[i]->y - enemy->y;
        float dist = dx * dx + dy * dy;
        if (dist < bestDist) {
          bestDist = dist;
          nearest = acorns[i];
        }
      }
      return nearest;
    }

    void updateAcorns(unsigned long now) {
      if (now >= nextSpawnMs && countActiveAcorns() < 4) {
        spawnAcorn();
        nextSpawnMs = now + 700 + random(0, 500);
      }

      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        if (!acornActive[i]) {
          continue;
        }
        acorns[i]->setVelocity(0, acornSpeed[i]);
        acorns[i]->updatePos(now);

        if (acorns[i]->y > SCREENHEIGHT) {
          deactivateAcorn(i);
        }
      }
    }

    int countActiveAcorns() {
      int count = 0;
      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        if (acornActive[i]) {
          count++;
        }
      }
      return count;
    }

    void spawnAcorn() {
      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        if (!acornActive[i]) {
          int16_t spawnX = 20 + random(0, SCREENWIDTH - SPRITE_ACORN_WIDTH - 40);
          acorns[i]->setPos(spawnX, HUD_ZONE_Y);
          // Speed ramps up with elapsed play time.
          float elapsedSec = (millis() - stateStartMs) / 1000.0f;
          float ramp = elapsedSec * ACORN_SPEED_RAMP_PER_SEC;
          if (ramp > ACORN_SPEED_RAMP_MAX) {
            ramp = ACORN_SPEED_RAMP_MAX;
          }
          acornSpeed[i] = randSpeed(ACORN_MIN_FALL_SPEED + ramp, ACORN_MAX_FALL_SPEED + ramp);
          acorns[i]->setVelocity(0, acornSpeed[i]);
          acornActive[i] = true;
          requestRender();
          return;
        }
      }
    }

    void deactivateAcorn(int index) {
      acornActive[index] = false;
      acorns[index]->setPos(acorns[index]->x, SCREENHEIGHT + 20);
      acorns[index]->setVelocity(0, 0);
      requestRender();
    }

    int countActiveSoot() {
      int count = 0;
      for (int i = 0; i < ACORN_MAX_SOOT; i++) {
        if (sootActive[i]) {
          count++;
        }
      }
      return count;
    }

    void updateSoot(unsigned long now) {
      if (now >= nextSootMs && countActiveSoot() < sootMaxConcurrent) {
        spawnSoot();
        nextSootMs = now + random(sootSpawnMinMs, sootSpawnMaxMs);
      }

      for (int i = 0; i < ACORN_MAX_SOOT; i++) {
        if (!sootActive[i]) {
          continue;
        }
        soots[i]->setVelocity(0, sootSpeed[i]);
        soots[i]->updatePos(now);
        // Disappear once most of the soot is below Mei's feet line. Use the
        // fixed ground line (not Mei's live Y) so a jump can't clear soot early.
        float feetY = (float)PLAYER_GROUND_Y + player->height;
        float clearY = feetY - (1.0f - SOOT_CLEAR_BELOW_FEET) * SOOT_SIZE;
        if (soots[i]->y >= clearY || soots[i]->y > SCREENHEIGHT) {
          deactivateSoot(i);
        }
      }
    }

    // Pick where a new soot drops. Most of the time it is aimed to land on Mei
    // (or, less often, Chu) with a little jitter, so the soot feels out to get
    // you; the rest of the time it falls at a fully random column.
    int16_t chooseSootSpawnX() {
      int maxX = SCREENWIDTH - SOOT_SIZE;
      if (random(0, 100) < SOOT_AIM_CHANCE) {
        Avatar *target = player;
        if (enemy != NULL && random(0, 10) >= SOOT_AIM_MEI_WEIGHT) {
          target = enemy;
        }
        if (target != NULL) {
          int centerX = (int)(target->x + target->width / 2);
          int x = centerX - SOOT_SIZE / 2 + random(-SOOT_AIM_JITTER, SOOT_AIM_JITTER + 1);
          if (x < 0) {
            x = 0;
          }
          if (x > maxX) {
            x = maxX;
          }
          return (int16_t)x;
        }
      }
      return (int16_t)random(0, maxX);
    }

    void spawnSoot() {
      for (int i = 0; i < ACORN_MAX_SOOT; i++) {
        if (!sootActive[i]) {
          int variant = random(0, SOOT_VARIANTS);
          sootSheet().applyRegion(soots[i], SpriteSheet::readRegion(sprite_soot_moleRegions, variant));
          int16_t spawnX = chooseSootSpawnX();
          soots[i]->setPos(spawnX, HUD_ZONE_Y);
          sootSpeed[i] = randSpeed(SOOT_MIN_FALL_SPEED, SOOT_MAX_FALL_SPEED);
          soots[i]->setVelocity(0, sootSpeed[i]);
          soots[i]->requestRedraw();
          sootActive[i] = true;
          requestRender();
          return;
        }
      }
    }

    void deactivateSoot(int index) {
      sootActive[index] = false;
      soots[index]->setPos(soots[index]->x, SCREENHEIGHT + 60);
      soots[index]->setVelocity(0, 0);
      requestRender();
    }

    // Sonic-style: fling up to `count` acorns out of Mei; purely visual explosion.
    void triggerScatter(int count) {
      if (count <= 0) {
        return;
      }
      float cx = player->x + player->width / 2.0f - SPRITE_ACORN_WIDTH / 2.0f;
      float cy = player->y + player->height / 3.0f;
      int spawned = 0;
      for (int i = 0; i < MAX_SCATTER_ACORNS && spawned < count; i++) {
        if (scatterActive[i]) {
          continue;
        }
        scatterAcorns[i]->setPos(cx, cy);
        float dir = (spawned % 2 == 0) ? 1.0f : -1.0f;
        scatterVx[i] = dir * (2.0f + random(0, 30) / 10.0f);  // 2.0 .. 5.0 outward
        scatterVy[i] = -(4.0f + random(0, 30) / 10.0f);        // 4.0 .. 7.0 upward
        scatterActive[i] = true;
        spawned++;
      }
      requestRender();
    }

    void updateScatter(bool step) {
      if (!step) {
        return;
      }
      bool any = false;
      for (int i = 0; i < MAX_SCATTER_ACORNS; i++) {
        if (!scatterActive[i]) {
          continue;
        }
        any = true;
        scatterVy[i] += SCATTER_GRAVITY;
        float nx = scatterAcorns[i]->x + scatterVx[i];
        float ny = scatterAcorns[i]->y + scatterVy[i];
        scatterAcorns[i]->setPos(nx, ny);
        if (ny > SCREENHEIGHT || nx < -SPRITE_ACORN_WIDTH || nx > SCREENWIDTH) {
          scatterActive[i] = false;
          scatterAcorns[i]->setPos(-40, SCREENHEIGHT + 40);
        }
      }
      if (any) {
        requestRender();
      }
    }

    bool checkCollisions() {
      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        if (!acornActive[i]) {
          continue;
        }

        if (physics::aabbTest(*player, *acorns[i])) {
          score++;
          caughtCount++;
          addAcornTimeBonus();
          addSound(NOTE_E5, noteDurationMs(16, 900));
          deactivateAcorn(i);
          updateHudAvatars(millis());
          if (mode == ACORN_MODE_COLLECTOR && score >= ACORN_CATCH_TARGET) {
            winCollector(millis());
            return true;
          }
          requestRender();
          continue;
        }

        // Chu eats the acorn (denies it to Mei) unless it has been soot-hit
        // (either still falling or grounded-and-stunned).
        if (chuCanCollect() && physics::aabbTest(*enemy, *acorns[i])) {
          addSound(NOTE_A3, noteDurationMs(16, 700));
          deactivateAcorn(i);
          requestRender();
        }
      }

      // Soot collisions: hitting Mei costs acorns (with an explosion) and, in
      // Survival, a life; hitting Chu stuns it.
      for (int i = 0; i < ACORN_MAX_SOOT; i++) {
        if (!sootActive[i]) {
          continue;
        }

        if (physics::aabbTest(*player, *soots[i])) {
          unsigned long hitNow = millis();
          int lost = (score < SOOT_PENALTY) ? score : SOOT_PENALTY;
          triggerScatter(lost);           // explosion effect
          score -= lost;                  // never below zero; no score game-over
          addSound(NOTE_A3, noteDurationMs(8, 700));
          addSound(NOTE_E3, noteDurationMs(8, 700));
          deactivateSoot(i);

          if (mode == ACORN_MODE_SURVIVAL) {
            lives--;
            updateHudAvatars(hitNow);
            if (lives <= 0) {
              endSurvival();
              return true;
            }
          } else {
            updateHudAvatars(hitNow);
          }
          requestRender();
          continue;
        }

        // Soot lands on Chu: it stops collecting at once. If it is airborne it
        // keeps falling on its locked jump path and only becomes stunned once it
        // touches the ground; if already grounded it stuns immediately.
        if (chuCanCollect() && physics::aabbTest(*enemy, *soots[i])) {
          if (chuJumping) {
            chuHitPending = true;      // stun starts when it lands
          } else {
            chuStunUntilMs = millis() + CHU_STUN_MS;
          }
          addSound(NOTE_C4, noteDurationMs(8, 600));
          deactivateSoot(i);
          requestRender();
        }
      }

      return false;
    }
};

#endif
