#ifndef _SCENE_ACORNCATCH_H_
#define _SCENE_ACORNCATCH_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameProgress.h"
#include "Input.h"
#include "Physics.h"
#include "SpriteSheet.h"
#include "SpriteText.h"
#include "image_acorn_catch_bg.h"
#include "sprite_acorn.h"
#include "sprite_chu_totoro.h"
#include "sprite_digits.h"
#include "sprite_letters.h"
#include "sprite_mei.h"
#include "sprite_soot_mole.h"
#include "sprite_stopwatch.h"

#define ACORN_CATCH_TARGET 30
#define ACORN_CATCH_TIME_MS 40000
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
// Time is only added every Nth acorn caught (higher = harder to bank time).
#define ACORN_TIME_BONUS_EVERY 2
// The time bonus when it does apply, capped at the starting time.
#define ACORN_TIME_BONUS_MS 2000

// Soot-mole hazard: falls occasionally, costs acorns if it hits Mei.
#define MAX_SOOT 2
#define SOOT_SIZE 44
#define SOOT_VARIANTS 10
#define SOOT_MIN_FALL_SPEED 3.0f
#define SOOT_MAX_FALL_SPEED 6.0f
#define SOOT_SPAWN_MIN_MS 3500
#define SOOT_SPAWN_MAX_MS 7000
#define SOOT_PENALTY 5
// Getting hit by soot also burns time off the clock.
#define SOOT_TIME_PENALTY_MS 3000
// Remove a soot once this fraction of its height has passed below Mei's feet,
// so she can't step onto one resting near the ground.
#define SOOT_CLEAR_BELOW_FEET 0.8f

// Sonic-style acorn scatter shown when Mei is hit (purely visual).
#define MAX_SCATTER_ACORNS 5
#define SCATTER_GRAVITY 0.6f

// Chu can jump to contest acorns at Mei's height.
#define CHU_JUMP_V (-7.0f)
#define CHU_GRAVITY 0.6f
#define CHU_JUMP_ALIGN_X 18
#define CHU_JUMP_CHANCE 60
#define CHU_JUMP_COOLDOWN_MS 900
// Mei can jump too (Home tap). Single jump, full air control, no double jump.
#define MEI_JUMP_V (-7.5f)
#define MEI_GRAVITY 0.6f
// Physics step for jump/scatter, matching the acorn fall cadence.
#define PHYSICS_STEP_MS 50
#define MAX_END_MESSAGE_GLYPHS 16
#define END_MESSAGE_LINE1_Y 140
#define END_MESSAGE_LINE2_Y 165
#define END_MESSAGE_GAP 1

// Coins awarded on a win are proportional to the seconds left on the clock when
// the acorn target was reached (finish faster -> earn more). A win always pays
// at least COIN_WIN_MINIMUM so a last-second victory still rewards the player.
#define COINS_PER_SECOND_LEFT 1
#define COIN_WIN_MINIMUM 1
// Win screen layout: title, the coin count (large digits), and a "COINS" label.
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

enum AcornCatchState {
  ACORN_STATE_INTRO,
  ACORN_STATE_PLAYING,
  ACORN_STATE_WON,
  ACORN_STATE_LOST
};

// Intro screen: pulse the "press any button" prompt on/off at this cadence.
#define INTRO_PROMPT_BLINK_MS 500
// Ignore button presses for a moment so the press that launched the scene from
// the hub doesn't immediately skip the intro.
#define INTRO_INPUT_GRACE_MS 300

class Scene_AcornCatch : public GameScene {
  public:
    Scene_AcornCatch(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      (void)isTouching;
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        // Only after the round has ended does Home return to the hub. Require a
        // fresh press so a Home still held from a jump doesn't skip the end
        // screen instantly.
        if (input.homePressed) {
          *needChangeScene = true;
          *nextSceneIndex = 0;
          return;
        }
        return;
      }

      if (state == ACORN_STATE_INTRO) {
        // Any button starts the game once the initial grace window has passed.
        if ((now - stateStartMs) > INTRO_INPUT_GRACE_MS &&
            (input.leftPressed || input.rightPressed || input.homePressed)) {
          startGame(now);
          return;
        }
        if ((now - introBlinkMs) > INTRO_PROMPT_BLINK_MS) {
          introBlinkMs = now;
          introPromptVisible = !introPromptVisible;
          drawIntroPrompt(introPromptVisible);
        }
        return;
      }

      // During gameplay Home only makes Mei jump; it never exits to the hub
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

      if (now >= gameEndMs) {
        if (score >= ACORN_CATCH_TARGET) {
          winGame(now);
        } else {
          loseGame(now);
        }
        return;
      }

      if (score >= ACORN_CATCH_TARGET) {
        winGame(now);
        return;
      }

      requestRender();
    }

    void render() {
      // The intro is a static screen drawn directly to the TFT; the avatar
      // renderer is idle until the game actually starts.
      if (state == ACORN_STATE_INTRO) {
        return;
      }
      renderScene();
    }

    void initScene() {
      setBackground(acorn_catch_bg);
      drawBackground(acorn_catch_bg);

      initHudAvatars();

      player = meiSheet().createAvatar(109, PLAYER_GROUND_Y,
                                       SpriteSheet::readRegion(sprite_meiRegions, 0));
      player->updateInterval = 50;
      appendAvatar(player);
      meiFrame = 0;
      meiFacingRight = true;
      meiFrameMs = millis();

      enemy = new Avatar(170, GROUND_Y, SPRITE_CHU_TOTORO_WIDTH, SPRITE_CHU_TOTORO_HEIGHT, sprite_chu_totoro, sprite_chu_totoroMask);
      enemy->setVelocity(0, 0);
      enemy->updateInterval = 50;
      appendAvatar(enemy);

      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        acorns[i] = new Avatar(-40, SCREENHEIGHT + 20, SPRITE_ACORN_WIDTH, SPRITE_ACORN_HEIGHT, sprite_acorn, sprite_acornMask);
        acorns[i]->setVelocity(0, 0);
        acorns[i]->updateInterval = 50;
        acornActive[i] = false;
        acornSpeed[i] = ACORN_FALL_SPEED;
        appendAvatar(acorns[i]);
      }

      SpriteSheet sootSh = sootSheet();
      for (int i = 0; i < MAX_SOOT; i++) {
        soots[i] = sootSh.createAvatar(-SOOT_SIZE - 20, SCREENHEIGHT + 60,
                                       SpriteSheet::readRegion(sprite_soot_moleRegions, 0));
        soots[i]->updateInterval = 50;
        sootActive[i] = false;
        appendAvatar(soots[i]);
      }

      for (int i = 0; i < MAX_SCATTER_ACORNS; i++) {
        scatterAcorns[i] = new Avatar(-40, SCREENHEIGHT + 40, SPRITE_ACORN_WIDTH, SPRITE_ACORN_HEIGHT,
                                      sprite_acorn, sprite_acornMask);
        scatterAcorns[i]->setVelocity(0, 0);
        scatterAcorns[i]->updateInterval = 50;
        scatterActive[i] = false;
        appendAvatar(scatterAcorns[i]);
      }

      chuJumping = false;
      chuVy = 0;
      chuJumpVx = 0;
      chuY = GROUND_Y;
      lastChuJumpMs = 0;
      meiJumping = false;
      meiVy = 0;
      meiY = PLAYER_GROUND_Y;
      lastMeiJumpMs = 0;
      nextSootMs = 0;
      nextStepMs = 0;

      score = 0;
      caughtCount = 0;
      coinsEarned = 0;
      state = ACORN_STATE_INTRO;
      stateStartMs = millis();
      introBlinkMs = stateStartMs;
      introPromptVisible = true;
      nextSpawnMs = 0;
      clearEndMessageAvatars();
      resetHudCache();
      drawIntro();
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
      for (int i = 0; i < MAX_SOOT; i++) {
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
    Avatar *soots[MAX_SOOT];
    bool sootActive[MAX_SOOT];
    float sootSpeed[MAX_SOOT];
    unsigned long nextSootMs = 0;
    Avatar *scatterAcorns[MAX_SCATTER_ACORNS];
    bool scatterActive[MAX_SCATTER_ACORNS];
    float scatterVx[MAX_SCATTER_ACORNS];
    float scatterVy[MAX_SCATTER_ACORNS];
    unsigned long nextStepMs = 0;
    bool chuJumping = false;
    float chuVy = 0;
    float chuJumpVx = 0;
    float chuY = GROUND_Y;
    unsigned long lastChuJumpMs = 0;
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
    int meiFrame = 0;
    bool meiFacingRight = true;
    unsigned long meiFrameMs = 0;
    AcornCatchState state = ACORN_STATE_INTRO;
    unsigned long stateStartMs = 0;
    unsigned long introBlinkMs = 0;
    bool introPromptVisible = true;
    unsigned long gameEndMs = 0;
    unsigned long nextSpawnMs = 0;
    int lastHudTimeLeft = -1;
    int lastHudScore = -1;
    Avatar *endMessageAvatars[MAX_END_MESSAGE_GLYPHS];
    int endMessageAvatarCount = 0;
    bool endMessageBuilt = false;

    void resetHudCache() {
      lastHudTimeLeft = -1;
      lastHudScore = -1;
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

      hudAcorn = new Avatar(HUD_ACORN_X, HUD_ACORN_Y, SPRITE_ACORN_WIDTH, SPRITE_ACORN_HEIGHT, sprite_acorn, sprite_acornMask);
      hudAcorn->setVelocity(0, 0);
      hudAcorn->updateInterval = 50;
      appendAvatar(hudAcorn);
    }

    void updateHudAvatars(unsigned long now) {
      int timeLeft = 0;
      if (state == ACORN_STATE_PLAYING) {
        timeLeft = (gameEndMs > now) ? (int)((gameEndMs - now) / 1000) : 0;
      }

      int displayScore = score > 999 ? 999 : score;
      if (timeLeft == lastHudTimeLeft && displayScore == lastHudScore) {
        return;
      }

      setHudDigit(hudTimeTens, timeLeft / 10, false);
      setHudDigit(hudTimeOnes, timeLeft % 10, false);
      setHudDigit(hudScoreHundreds, displayScore / 100, true);
      setHudDigit(hudScoreTens, (displayScore / 10) % 10, true);
      setHudDigit(hudScoreOnes, displayScore % 10, true);

      lastHudTimeLeft = timeLeft;
      lastHudScore = displayScore;
      requestRender();
    }

    // Instruction screen shown before play. Drawn once with the built-in TFT
    // font (the sprite/avatar renderer stays idle until startGame()).
    void drawIntro() {
      uint16_t bg = rgb565(34, 60, 40);      // forest green
      uint16_t panel = rgb565(24, 44, 30);
      uint16_t border = rgb565(120, 160, 120);
      setBackgroundColor(bg);
      _tft->fillScreen(bg);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(240, 220, 120), bg);
      _tft->drawString("ACORN CATCH", SCREENWIDTH / 2, 42, 4);

      _tft->fillRoundRect(18, 84, SCREENWIDTH - 36, 150, 10, panel);
      _tft->drawRoundRect(18, 84, SCREENWIDTH - 36, 150, 10, border);

      char line[24];
      snprintf(line, sizeof(line), "Collect %d acorns", ACORN_CATCH_TARGET);
      _tft->setTextColor(TFT_WHITE, panel);
      _tft->drawString(line, SCREENWIDTH / 2, 108, 2);
      _tft->setTextColor(rgb565(230, 130, 130), panel);
      _tft->drawString("Avoid the soot", SCREENWIDTH / 2, 136, 2);
      _tft->setTextColor(rgb565(200, 220, 255), panel);
      _tft->drawString("LEFT / RIGHT to move", SCREENWIDTH / 2, 172, 2);
      _tft->setTextColor(rgb565(180, 240, 180), panel);
      _tft->drawString("HOME to jump", SCREENWIDTH / 2, 204, 2);

      _tft->setTextDatum(TL_DATUM);

      introPromptVisible = true;
      drawIntroPrompt(true);
    }

    // Blinking "press any button" prompt near the bottom of the intro screen.
    void drawIntroPrompt(bool visible) {
      uint16_t bg = rgb565(34, 60, 40);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(visible ? rgb565(245, 245, 170) : bg, bg);
      _tft->drawString("Press any button to start", SCREENWIDTH / 2, 276, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    // Leave the intro screen and start the actual round.
    void startGame(unsigned long now) {
      drawBackground(acorn_catch_bg);
      resetHudCache();
      beginPlay(now);
      updateHudAvatars(now);
      renderFullScreen();
    }

    void beginPlay(unsigned long now) {
      state = ACORN_STATE_PLAYING;
      stateStartMs = now;
      gameEndMs = now + ACORN_CATCH_TIME_MS;
      nextSpawnMs = now + 500;
      nextSootMs = now + random(SOOT_SPAWN_MIN_MS, SOOT_SPAWN_MAX_MS);
      nextStepMs = now + PHYSICS_STEP_MS;
      resetHudCache();
      addSound(NOTE_C5, noteDurationMs(8, 800));
      requestRender();
    }

    void winGame(unsigned long now) {
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_WON;
      freezeGameplay();
      resetHudCache();

      // Reward is proportional to how much time was left when the target was hit.
      int secondsLeft = (gameEndMs > now) ? (int)((gameEndMs - now) / 1000) : 0;
      coinsEarned = secondsLeft * COINS_PER_SECOND_LEFT;
      if (coinsEarned < COIN_WIN_MINIMUM) {
        coinsEarned = COIN_WIN_MINIMUM;
      }
      GameProgress::addCoins(coinsEarned);

      updateHudAvatars(millis());
      showWinResult(coinsEarned);
      addSound(NOTE_G5, noteDurationMs(4, 800));
      addSound(NOTE_C6, noteDurationMs(4, 800));
      addSound(NOTE_E6, noteDurationMs(2, 800));
      requestRender();
    }

    void loseGame(unsigned long now, const char *line1 = "TIME UP", const char *line2 = "GAME OVER") {
      (void)now;
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_LOST;
      freezeGameplay();
      resetHudCache();
      updateHudAvatars(millis());
      showEndMessage(line1, line2);
      addSound(NOTE_G3, noteDurationMs(4, 600));
      addSound(NOTE_E3, noteDurationMs(4, 600));
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
      for (int i = 0; i < MAX_SOOT; i++) {
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
      chuY = GROUND_Y;
      enemy->y = GROUND_Y;
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

    // Win screen: title, the coins earned as large digits, and a "COINS" label.
    // All glyphs are tracked in endMessageAvatars so they are cleaned up with the
    // rest of the end-message avatars when the scene is destroyed.
    void showWinResult(int coins) {
      if (endMessageBuilt) {
        return;
      }
      endMessageBuilt = true;
      endMessageAvatarCount = 0;

      Avatar *glyphs[MAX_END_MESSAGE_GLYPHS];

      int count = SpriteText::buildCenteredLine(this, "YOU WIN", WIN_TITLE_Y, glyphs,
                                                MAX_END_MESSAGE_GLYPHS, END_MESSAGE_GAP);
      for (int i = 0; i < count; i++) {
        endMessageAvatars[endMessageAvatarCount++] = glyphs[i];
      }

      count = buildCenteredNumber(coins, WIN_COINS_NUM_Y, glyphs,
                                  MAX_END_MESSAGE_GLYPHS - endMessageAvatarCount);
      for (int i = 0; i < count; i++) {
        endMessageAvatars[endMessageAvatarCount++] = glyphs[i];
      }

      count = SpriteText::buildCenteredLine(this, "COINS", WIN_COINS_LABEL_Y, glyphs,
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
      float vx;

      if (chuJumping) {
        // Direction is locked for the whole jump so an airborne Chu can't steer
        // onto Mei, keeping it from being overpowering.
        vx = chuJumpVx;
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
        if (chuY >= GROUND_Y) {
          chuY = GROUND_Y;
          chuJumping = false;
          chuVy = 0;
        }
      }
      enemy->y = chuJumping ? chuY : (float)GROUND_Y;
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

    void updateSoot(unsigned long now) {
      if (now >= nextSootMs) {
        spawnSoot();
        nextSootMs = now + random(SOOT_SPAWN_MIN_MS, SOOT_SPAWN_MAX_MS);
      }

      for (int i = 0; i < MAX_SOOT; i++) {
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

    void spawnSoot() {
      for (int i = 0; i < MAX_SOOT; i++) {
        if (!sootActive[i]) {
          int variant = random(0, SOOT_VARIANTS);
          sootSheet().applyRegion(soots[i], SpriteSheet::readRegion(sprite_soot_moleRegions, variant));
          int16_t spawnX = random(0, SCREENWIDTH - SOOT_SIZE);
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

    // Sonic-style: fling up to `count` acorns out of Mei; purely visual.
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
          // Only every Nth acorn extends the clock, capped at the starting time.
          if (caughtCount % ACORN_TIME_BONUS_EVERY == 0) {
            unsigned long capEnd = millis() + ACORN_CATCH_TIME_MS;
            gameEndMs += ACORN_TIME_BONUS_MS;
            if (gameEndMs > capEnd) {
              gameEndMs = capEnd;
            }
          }
          addSound(NOTE_E5, noteDurationMs(16, 900));
          deactivateAcorn(i);
          updateHudAvatars(millis());
          if (score >= ACORN_CATCH_TARGET) {
            winGame(millis());
            return true;
          }
          requestRender();
          continue;
        }

        if (physics::aabbTest(*enemy, *acorns[i])) {
          addSound(NOTE_A3, noteDurationMs(16, 700));
          deactivateAcorn(i);
          requestRender();
        }
      }

      // Getting hit by soot costs acorns and scatters them out of Mei.
      for (int i = 0; i < MAX_SOOT; i++) {
        if (!sootActive[i]) {
          continue;
        }
        if (physics::aabbTest(*player, *soots[i])) {
          unsigned long hitNow = millis();
          // Burn time off the clock (clamp to avoid unsigned underflow).
          if (gameEndMs > hitNow + SOOT_TIME_PENALTY_MS) {
            gameEndMs -= SOOT_TIME_PENALTY_MS;
          } else {
            gameEndMs = hitNow;
          }

          int lost = (score < SOOT_PENALTY) ? score : SOOT_PENALTY;
          triggerScatter(lost);
          addSound(NOTE_A3, noteDurationMs(8, 700));
          addSound(NOTE_E3, noteDurationMs(8, 700));
          deactivateSoot(i);

          if (score < SOOT_PENALTY) {
            // Penalty would push the score below zero -> immediate loss.
            score = 0;
            updateHudAvatars(hitNow);
            loseGame(hitNow, "GAME OVER", "YOU LOSE");
            return true;
          }

          score -= SOOT_PENALTY;
          updateHudAvatars(hitNow);
          requestRender();
        }
      }

      if (physics::aabbTest(*player, *enemy)) {
        // intentional no-op
      }
      return false;
    }
};

#endif
