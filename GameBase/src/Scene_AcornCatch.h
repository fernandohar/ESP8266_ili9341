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
#include "sprite_stopwatch.h"

#define ACORN_CATCH_TARGET 25
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
// Keep Mei's feet on the same ground line as the old 48x68 sprite.
#define PLAYER_GROUND_Y (GROUND_Y + 68 - MEI_FRAME_H)
#define ACORN_FALL_SPEED 5.0f
#define MAX_END_MESSAGE_GLYPHS 16
#define END_MESSAGE_LINE1_Y 140
#define END_MESSAGE_LINE2_Y 165
#define END_MESSAGE_GAP 1

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
  ACORN_STATE_READY,
  ACORN_STATE_PLAYING,
  ACORN_STATE_WON,
  ACORN_STATE_LOST
};

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
        if (input.homePressed || input.home) {
          *needChangeScene = true;
          *nextSceneIndex = 0;
          return;
        }
        return;
      }

      if (input.homePressed && state == ACORN_STATE_PLAYING) {
        *needChangeScene = true;
        *nextSceneIndex = 0;
        return;
      }

      if (state == ACORN_STATE_READY) {
        updateHudAvatars(now);
        if (now - stateStartMs > 800) {
          beginPlay(now);
        }
        return;
      }

      updatePlayer(input);
      updateEnemy();
      updateAcorns(now);
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
        appendAvatar(acorns[i]);
      }

      score = 0;
      state = ACORN_STATE_READY;
      stateStartMs = millis();
      nextSpawnMs = 0;
      clearEndMessageAvatars();
      resetHudCache();
      updateHudAvatars(millis());
      renderFullScreen();
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
      clearEndMessageAvatars();
      GameScene::destroyScene();
    }

  private:
    Avatar *player = NULL;
    Avatar *enemy = NULL;
    Avatar *acorns[MAX_FALLING_ACORNS];
    Avatar *hudWatch = NULL;
    Avatar *hudTimeTens = NULL;
    Avatar *hudTimeOnes = NULL;
    Avatar *hudScoreHundreds = NULL;
    Avatar *hudScoreTens = NULL;
    Avatar *hudScoreOnes = NULL;
    Avatar *hudAcorn = NULL;
    bool acornActive[MAX_FALLING_ACORNS];

    int score = 0;
    int meiFrame = 0;
    bool meiFacingRight = true;
    unsigned long meiFrameMs = 0;
    AcornCatchState state = ACORN_STATE_READY;
    unsigned long stateStartMs = 0;
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
      } else if (state == ACORN_STATE_READY) {
        timeLeft = ACORN_CATCH_TIME_MS / 1000;
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

    void beginPlay(unsigned long now) {
      state = ACORN_STATE_PLAYING;
      stateStartMs = now;
      gameEndMs = now + ACORN_CATCH_TIME_MS;
      nextSpawnMs = now + 500;
      resetHudCache();
      addSound(NOTE_C5, noteDurationMs(8, 800));
      requestRender();
    }

    void winGame(unsigned long now) {
      (void)now;
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_WON;
      freezeGameplay();
      resetHudCache();
      updateHudAvatars(millis());
      GameProgress::addCoin();
      showEndMessage("YOU WIN");
      addSound(NOTE_G5, noteDurationMs(4, 800));
      addSound(NOTE_C6, noteDurationMs(4, 800));
      addSound(NOTE_E6, noteDurationMs(2, 800));
      requestRender();
    }

    void loseGame(unsigned long now) {
      (void)now;
      if (state == ACORN_STATE_WON || state == ACORN_STATE_LOST) {
        return;
      }
      state = ACORN_STATE_LOST;
      freezeGameplay();
      resetHudCache();
      updateHudAvatars(millis());
      showEndMessage("TIME UP", "GAME OVER");
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
      player->y = PLAYER_GROUND_Y;

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

    void updateEnemy() {
      Avatar *target = findNearestAcorn();
      float vx = 0;

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

      enemy->setVelocity(vx, 0);
      enemy->updatePos(millis());

      if (enemy->x < 0) {
        enemy->x = 0;
      }
      if (enemy->x + enemy->width > SCREENWIDTH) {
        enemy->x = SCREENWIDTH - enemy->width;
      }
      enemy->y = GROUND_Y;
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
        acorns[i]->setVelocity(0, ACORN_FALL_SPEED);
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
          acorns[i]->setVelocity(0, ACORN_FALL_SPEED);
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

    bool checkCollisions() {
      for (int i = 0; i < MAX_FALLING_ACORNS; i++) {
        if (!acornActive[i]) {
          continue;
        }

        if (physics::aabbTest(*player, *acorns[i])) {
          score++;
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

      if (physics::aabbTest(*player, *enemy)) {
        // intentional no-op
      }
      return false;
    }
};

#endif
