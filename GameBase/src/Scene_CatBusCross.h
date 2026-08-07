#ifndef _SCENE_CATBUSCROSS_H_
#define _SCENE_CATBUSCROSS_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "GameResult.h"
#include "Input.h"
#include "Physics.h"
#include "SpriteSheet.h"
#include "SpriteText.h"
#include "TouchInput.h"
#include "image_grass_tile.h"
#include "sprite_mei.h"
#include "sprite_soot_mole.h"
#include "sprite_catbus_goal.h"

// Mei crosses soot-filled lanes to reach the Cat Bus waiting at the top.
// Grass tile background; moving soot + player + bus are Avatars composited by
// renderScene() (dirty-rect). Home steps Mei up one lane; Left/Right move
// sideways. Touch the upper screen also steps up.

#define CBC_HUD_H 30
#define CBC_GOAL_Y 30
#define CBC_GOAL_H 44
#define CBC_LANE_COUNT 6
#define CBC_LANE_H 38
#define CBC_LANES_TOP 74
#define CBC_SOOT_SIZE 44
#define CBC_SOOT_PER_LANE 2
#define CBC_MAX_SOOT (CBC_LANE_COUNT * CBC_SOOT_PER_LANE)
#define CBC_MEI_W 24
#define CBC_MEI_H 34
#define CBC_PLAYER_SPEED 3.8f
#define CBC_BUS_SPEED 2.2f
#define CBC_START_LIVES 3
#define CBC_HOP_COOLDOWN_MS 220
#define CBC_INTRO_GRACE_MS 350

enum CatBusCrossState {
  CBC_STATE_READY,
  CBC_STATE_PLAYING,
  CBC_STATE_WON,
  CBC_STATE_LOST
};

struct LaneHazard {
  Avatar *avatar;
  int8_t lane;
  float speed;
};

class Scene_CatBusCross : public GameScene {
  public:
    Scene_CatBusCross(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (state == CBC_STATE_WON || state == CBC_STATE_LOST) {
        if (input.homePressed) {
          *needChangeScene = true;
          *nextSceneIndex = SCENE_PET_TOTORO;
        } else if (isTouching && !wasTouching) {
          resetRound(now);
        }
        wasTouching = isTouching;
        return;
      }

      if (state == CBC_STATE_READY) {
        if (now > suppressInputUntilMs) {
          if (input.homePressed || (isTouching && !wasTouching)) {
            beginPlay(now);
          }
        }
        wasTouching = isTouching;
        return;
      }

      updatePlayerInput(input, isTouching, now);
      updateBus(now);
      updateHazards(now);
      checkCollisions(now);
      if (playerRow == CBC_GOAL_ROW) {
        tryCatchBus(now);
      }

      wasTouching = isTouching;
    }

    void render() {
      renderScene();
    }

    void initScene() {
      setBackgroundTile(grass_tile, GRASS_TILE_WIDTH, GRASS_TILE_HEIGHT);

      SpriteSheet mei = meiSheet();
      player = mei.createAvatar(SCREENWIDTH / 2 - CBC_MEI_W / 2, laneMeiY(0), meiRunRegion(0));
      appendAvatar(player);

      catBus = new Avatar(CBC_BUS_START_X, busY(), SPRITE_CATBUS_GOAL_WIDTH, SPRITE_CATBUS_GOAL_HEIGHT,
                          sprite_catbus_goal, sprite_catbus_goalMask);
      appendAvatar(catBus);

      SpriteSheet soot = sootSheet();
      for (int i = 0; i < CBC_MAX_SOOT; i++) {
        int lane = i / CBC_SOOT_PER_LANE;
        int slot = i % CBC_SOOT_PER_LANE;
        int variant = (lane + slot) % 10;
        SpriteSheetRegion region = SpriteSheet::readRegion(sprite_soot_moleRegions, variant);
        hazards[i].avatar = soot.createAvatar(-120, laneSootY(lane), region);
        hazards[i].lane = lane;
        hazards[i].speed = hazardSpeed(lane);
        appendAvatar(hazards[i].avatar);
      }

      initBannerPool();

      wasTouching = false;
      suppressInputUntilMs = millis() + CBC_INTRO_GRACE_MS;
      resetRound(millis());
    }

    void destroyScene() {
      player = NULL;
      catBus = NULL;
      for (int i = 0; i < CBC_MAX_SOOT; i++) {
        hazards[i].avatar = NULL;
      }
      for (int i = 0; i < CBC_BANNER_POOL; i++) {
        bannerGlyphs[i] = NULL;
      }
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    static const int CBC_BANNER_POOL = 18;
    static const int CBC_BUS_START_X = 60;
    static const int CBC_GOAL_ROW = 6;

    Avatar *player = NULL;
    Avatar *catBus = NULL;
    LaneHazard hazards[CBC_MAX_SOOT];
    Avatar *bannerGlyphs[CBC_BANNER_POOL];

    CatBusCrossState state = CBC_STATE_READY;
    int lives = CBC_START_LIVES;
    int playerRow = 0;
    int meiFrame = 0;
    unsigned long meiFrameMs = 0;
    unsigned long lastHopMs = 0;
    unsigned long suppressInputUntilMs = 0;
    float busDir = 1.0f;
    boolean wasTouching = false;

    SpriteSheet meiSheet() const {
      return SpriteSheet(sprite_mei, sprite_meiMask, SPRITE_MEI_WIDTH, SPRITE_MEI_HEIGHT);
    }

    SpriteSheet sootSheet() const {
      return SpriteSheet(sprite_soot_mole, sprite_soot_moleMask, SPRITE_SOOT_MOLE_WIDTH, SPRITE_SOOT_MOLE_HEIGHT);
    }

    SpriteSheetRegion meiRunRegion(int frame) const {
      return SpriteSheet::readRegion(sprite_meiRegions, frame % 6);
    }

    int16_t laneMeiY(int row) const {
      if (row >= CBC_LANE_COUNT) {
        return CBC_GOAL_Y + (CBC_GOAL_H - CBC_MEI_H) / 2;
      }
      return CBC_LANES_TOP + (CBC_LANE_COUNT - 1 - row) * CBC_LANE_H + (CBC_LANE_H - CBC_MEI_H) / 2;
    }

    int16_t laneSootY(int lane) const {
      return CBC_LANES_TOP + (CBC_LANE_COUNT - 1 - lane) * CBC_LANE_H + (CBC_LANE_H - CBC_SOOT_SIZE) / 2;
    }

    int16_t busY() const {
      return CBC_GOAL_Y + (CBC_GOAL_H - SPRITE_CATBUS_GOAL_HEIGHT) / 2;
    }

    float hazardSpeed(int lane) const {
      float base = 2.0f + lane * 0.25f;
      return (lane % 2 == 0) ? base : -base;
    }

    void initBannerPool() {
      SpriteSheet sheet = SpriteText::letterSheet();
      SpriteSheetRegion placeholder = SpriteSheet::readRegion(sprite_lettersRegions, 0);
      for (int i = 0; i < CBC_BANNER_POOL; i++) {
        bannerGlyphs[i] = sheet.createAvatar(-100, -100, placeholder);
        appendAvatar(bannerGlyphs[i]);
      }
    }

    void showBanner(const char *text, int y) {
      SpriteSheet sheet = SpriteText::letterSheet();
      int width = SpriteText::measureWidth(text);
      int cursor = (SCREENWIDTH - width) / 2;
      int count = 0;

      for (const char *p = text; *p != '\0' && count < CBC_BANNER_POOL; ++p) {
        if (*p == ' ') {
          cursor += SPRITE_LETTERS_CELL_W / 2;
          continue;
        }
        int index = SpriteText::letterRegionIndex(*p);
        if (index < 0) {
          continue;
        }
        SpriteSheetRegion region = SpriteSheet::readRegion(sprite_lettersRegions, index);
        sheet.applyRegion(bannerGlyphs[count], region);
        bannerGlyphs[count]->setPos(cursor, y);
        bannerGlyphs[count]->requestRedraw();
        count++;
        cursor += SPRITE_LETTERS_CELL_W + 2;
      }

      for (int i = count; i < CBC_BANNER_POOL; i++) {
        if (bannerGlyphs[i]->x >= 0) {
          bannerGlyphs[i]->setPos(-100, -100);
        }
      }
      requestRender();
    }

    void hideBanner() {
      for (int i = 0; i < CBC_BANNER_POOL; i++) {
        bannerGlyphs[i]->setPos(-100, -100);
      }
      requestRender();
    }

    void drawHudPanel() {
      _tft->fillRect(0, 0, SCREENWIDTH, CBC_HUD_H, rgb565(24, 48, 28));
      _tft->drawFastHLine(0, CBC_HUD_H - 1, SCREENWIDTH, rgb565(70, 110, 70));
      drawGoalStrip();
      drawLives();
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(240, 240, 210), rgb565(24, 48, 28));
      _tft->drawString("Catch Cat Bus!", SCREENWIDTH / 2, CBC_HUD_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawGoalStrip() {
      _tft->fillRoundRect(4, CBC_GOAL_Y, SCREENWIDTH - 8, CBC_GOAL_H, 6, rgb565(88, 58, 34));
      _tft->drawRoundRect(4, CBC_GOAL_Y, SCREENWIDTH - 8, CBC_GOAL_H, 6, rgb565(140, 100, 55));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(255, 230, 160), rgb565(88, 58, 34));
      _tft->drawString("BUS STOP", SCREENWIDTH / 2, CBC_GOAL_Y + 8, 1);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawLives() {
      uint16_t heart = rgb565(220, 70, 90);
      uint16_t empty = rgb565(80, 80, 80);
      for (int i = 0; i < CBC_START_LIVES; i++) {
        int cx = SCREENWIDTH - 14 - i * 16;
        uint16_t c = (i < lives) ? heart : empty;
        _tft->fillCircle(cx, 10, 4, c);
      }
    }

    void layoutHazards() {
      for (int i = 0; i < CBC_MAX_SOOT; i++) {
        int lane = hazards[i].lane;
        int slot = i % CBC_SOOT_PER_LANE;
        float x = (slot == 0) ? 20.0f : 140.0f;
        if (hazards[i].speed < 0) {
          x = SCREENWIDTH - CBC_SOOT_SIZE - x;
        }
        hazards[i].avatar->setPos(x, laneSootY(lane));
        hazards[i].avatar->requestRedraw();
      }
    }

    void resetPlayer() {
      playerRow = 0;
      player->setPos(SCREENWIDTH / 2 - CBC_MEI_W / 2, laneMeiY(0));
      player->setVelocity(0, 0);
      player->setFlipX(false);
      meiFrame = 0;
      meiSheet().applyRegion(player, meiRunRegion(0));
      player->requestRedraw();
    }

    void resetRound(unsigned long now) {
      (void)now;
      lives = CBC_START_LIVES;
      state = CBC_STATE_READY;
      busDir = 1.0f;
      catBus->setPos(CBC_BUS_START_X, busY());
      catBus->setFlipX(false);
      catBus->requestRedraw();
      layoutHazards();
      resetPlayer();
      hideBanner();

      renderFullScreen();
      drawHudPanel();
      showBanner("CROSS LANES", 128);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, rgb565(24, 48, 28));
      _tft->drawString("Home / tap top = up", SCREENWIDTH / 2, 156, 1);
      _tft->setTextDatum(TL_DATUM);
      requestRender();
    }

    void beginPlay(unsigned long now) {
      (void)now;
      state = CBC_STATE_PLAYING;
      hideBanner();
      addSound(NOTE_C5, noteDurationMs(8, 800));
      addSound(NOTE_E5, noteDurationMs(16, 800));
      requestRender();
    }

    void updatePlayerInput(const GameInput &input, boolean isTouching, unsigned long now) {
      float vx = 0.0f;
      if (input.left) {
        vx = -CBC_PLAYER_SPEED;
      } else if (input.right) {
        vx = CBC_PLAYER_SPEED;
      }
      player->setVelocity(vx, 0);
      player->updatePos(now);

      if (player->x < 4) {
        player->x = 4;
      } else if (player->x + player->width > SCREENWIDTH - 4) {
        player->x = SCREENWIDTH - 4 - player->width;
      }

      if (vx != 0.0f) {
        player->setFlipX(vx < 0);
        if ((now - meiFrameMs) >= 90) {
          meiFrame = (meiFrame + 1) % 6;
          meiFrameMs = now;
          meiSheet().applyRegion(player, meiRunRegion(meiFrame));
        }
        requestRender();
      }

      bool hop = false;
      if (input.homePressed && (now - lastHopMs) >= CBC_HOP_COOLDOWN_MS) {
        hop = true;
      } else if (isTouching && !wasTouching && now > suppressInputUntilMs) {
        uint16_t tx = 0;
        uint16_t ty = 0;
        if (getTouchPoint(_tft, &tx, &ty) && ty < SCREENHEIGHT / 2 &&
            (now - lastHopMs) >= CBC_HOP_COOLDOWN_MS) {
          hop = true;
        }
      }

      if (hop && playerRow < CBC_GOAL_ROW) {
        playerRow++;
        player->y = laneMeiY(playerRow);
        lastHopMs = now;
        addSound(NOTE_G4, noteDurationMs(16, 900));
        requestRender();

      }
    }

    void updateBus(unsigned long now) {
      (void)now;
      if (state != CBC_STATE_PLAYING) {
        return;
      }

      catBus->x += busDir * CBC_BUS_SPEED;
      if (catBus->x <= 4) {
        catBus->x = 4;
        busDir = 1.0f;
        catBus->setFlipX(false);
      } else if (catBus->x + catBus->width >= SCREENWIDTH - 4) {
        catBus->x = SCREENWIDTH - 4 - catBus->width;
        busDir = -1.0f;
        catBus->setFlipX(true);
      }
      requestRender();
    }

    void updateHazards(unsigned long now) {
      (void)now;
      if (state != CBC_STATE_PLAYING) {
        return;
      }

      for (int i = 0; i < CBC_MAX_SOOT; i++) {
        Avatar *s = hazards[i].avatar;
        s->x += hazards[i].speed;
        if (hazards[i].speed > 0 && s->x > SCREENWIDTH + 10) {
          s->x = -CBC_SOOT_SIZE - 10;
        } else if (hazards[i].speed < 0 && s->x + s->width < -10) {
          s->x = SCREENWIDTH + 10;
        }
      }
      requestRender();
    }

    void checkCollisions(unsigned long now) {
      if (state != CBC_STATE_PLAYING || playerRow >= CBC_GOAL_ROW) {
        return;
      }

      for (int i = 0; i < CBC_MAX_SOOT; i++) {
        if (hazards[i].lane != playerRow) {
          continue;
        }
        if (physics::aabbTest(*player, *hazards[i].avatar)) {
          onHit(now);
          return;
        }
      }
    }

    void tryCatchBus(unsigned long now) {
      if (physics::aabbTest(*player, *catBus)) {
        winRound(now);
      }
    }

    void onHit(unsigned long now) {
      lives--;
      addSound(NOTE_A3, noteDurationMs(8, 700));
      addSound(NOTE_E3, noteDurationMs(8, 700));

      if (lives <= 0) {
        loseRound(now);
        return;
      }

      drawLives();
      resetPlayer();
      requestRender();
    }

    void winRound(unsigned long now) {
      (void)now;
      state = CBC_STATE_WON;
      int reward = 3 + lives;
      GameResult::report(GAME_RESULT_WIN, reward);

      renderFullScreen();
      drawHudPanel();
      showBanner("BUS CAUGHT!", 120);
      addSound(NOTE_C5, noteDurationMs(8, 800));
      addSound(NOTE_E5, noteDurationMs(8, 800));
      addSound(NOTE_G5, noteDurationMs(8, 800));

      _tft->fillRoundRect(24, 168, SCREENWIDTH - 48, 56, 8, rgb565(88, 58, 34));
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(255, 230, 160), rgb565(88, 58, 34));
      char buf[32];
      snprintf(buf, sizeof(buf), "+%d coins", reward);
      _tft->drawString(buf, SCREENWIDTH / 2, 188, 2);
      _tft->drawString("Home = back to Totoro", SCREENWIDTH / 2, 210, 1);
      _tft->setTextDatum(TL_DATUM);
      requestRender();
    }

    void loseRound(unsigned long now) {
      (void)now;
      state = CBC_STATE_LOST;
      GameResult::report(GAME_RESULT_LOSS, 0);

      renderFullScreen();
      drawHudPanel();
      showBanner("TRY AGAIN", 130);
      addSound(NOTE_A3, noteDurationMs(4, 600));

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, rgb565(24, 48, 28));
      _tft->drawString("Tap to retry  Home = back", SCREENWIDTH / 2, 180, 1);
      _tft->setTextDatum(TL_DATUM);
      requestRender();
    }
};

#endif
