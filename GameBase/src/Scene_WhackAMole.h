#ifndef _SCENE_WHACKAMOLE_H_
#define _SCENE_WHACKAMOLE_H_

#include <Arduino.h>
#include "GameScene.h"
#include "Input.h"
#include "SpriteSheet.h"
#include "SpriteText.h"
#include "TouchInput.h"
#include "image_grass_tile.h"
#include "sprite_soot_mole.h"

// Grass tiled across the whole screen (setBackgroundTile) instead of a
// ~150 KB full-screen image; soot-sprite "moles" pop up at random spots on the
// grass. The TIME/SCORE/HITS wooden sign at the bottom is drawn with primitives
// (drawHudSign) since it is no longer baked into the background.
// Moles are real Avatars composited by renderScene() (see AGENTS.md), so
// hiding one is just moving it off-screen - the dirty-rect renderer restores
// the background underneath automatically.

#define WAM_MOLE_VARIANTS 10
#define WAM_MOLE_SIZE 44
#define WAM_MAX_ACTIVE_MOLES 3
#define WAM_ROUND_MS 30000

// Keep moles fully on screen and clear of the wooden HUD sign (which starts
// around y=277 in the 240x320 background).
#define WAM_PLAY_TOP_Y 4
#define WAM_PLAY_BOTTOM_Y 266

#define WAM_MAX_LEVEL 5
#define WAM_HITS_PER_LEVEL 4
#define WAM_POINTS_PER_HIT 10
#define WAM_LEVEL_UP_MS 1100

static const int WAM_VISIBLE_MIN_MS[WAM_MAX_LEVEL] = { 650, 560, 480, 410, 350 };
static const int WAM_VISIBLE_MAX_MS[WAM_MAX_LEVEL] = { 1450, 1250, 1050, 900, 750 };
static const int WAM_SPAWN_MIN_MS[WAM_MAX_LEVEL]   = { 380, 330, 290, 250, 220 };
static const int WAM_SPAWN_MAX_MS[WAM_MAX_LEVEL]   = { 850, 740, 640, 550, 470 };

// Each mole additionally gets its own random "speed" on top of the level's
// base timing, so how long any given mole stays up is unpredictable rather
// than a fixed range per level - some flash by fast, some linger a bit.
#define WAM_SPEED_FACTOR_MIN_PCT 60
#define WAM_SPEED_FACTOR_MAX_PCT 150
#define WAM_MIN_VISIBLE_MS 220

// Wooden HUD sign drawn with primitives at the bottom (replaces the baked-in
// sign that used to live in the full-screen background).
#define WAM_HUD_SIGN_X 4
#define WAM_HUD_SIGN_Y 274
#define WAM_HUD_SIGN_W (SCREENWIDTH - 8)
#define WAM_HUD_SIGN_H 42
#define WAM_HUD_LABEL_CY 283
// Digit-field centers/widths, laid out under the sign labels.
#define WAM_HUD_DIGIT_CY 299
#define WAM_HUD_FIELD_H 20
#define WAM_HUD_TIME_CX 49
#define WAM_HUD_TIME_W 28
#define WAM_HUD_SCORE_CX 116
#define WAM_HUD_SCORE_W 40
#define WAM_HUD_HITS_CX 179
#define WAM_HUD_HITS_W 28

#define WAM_BANNER_POOL 12

enum WhackAMoleState {
  WAM_STATE_READY,
  WAM_STATE_PLAYING,
  WAM_STATE_ENDED
};

struct MoleSlot {
  Avatar *avatar;
  bool active;
  unsigned long hideAtMs;
};

class Scene_WhackAMole : public GameScene {
  public:
    Scene_WhackAMole(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      const GameInput &input = Input::current();
      unsigned long now = millis();

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = 0;
        return;
      }

      if (state == WAM_STATE_ENDED) {
        if (isTouching && !wasTouching) {
          resetRound();
        }
        wasTouching = isTouching;
        return;
      }

      if (state == WAM_STATE_READY) {
        if (now - stateStartMs > 800) {
          beginPlay(now);
        }
        wasTouching = isTouching;
        return;
      }

      updateMoles(now);
      updateLevelUpBanner(now);
      trySpawnMole(now);

      if (now >= roundEndMs) {
        endRound();
        wasTouching = isTouching;
        return;
      }

      if (isTouching && !wasTouching && millis() > suppressTouchUntilMs) {
        uint16_t touchX = 0;
        uint16_t touchY = 0;
        if (getTouchPoint(_tft, &touchX, &touchY)) {
          whackAt(touchX, touchY, now);
        }
      }

      int timeLeft = (roundEndMs > now) ? (int)((roundEndMs - now) / 1000) : 0;
      if (timeLeft != lastHudTimeLeft) {
        drawHudField(WAM_HUD_TIME_CX, WAM_HUD_TIME_W, timeLeft, 2);
        lastHudTimeLeft = timeLeft;
      }

      wasTouching = isTouching;
    }

    void render() {
      renderScene();
    }

    void initScene() {
      setBackgroundTile(grass_tile, GRASS_TILE_WIDTH, GRASS_TILE_HEIGHT);

      SpriteSheet moleSheet = sootSheet();
      for (int i = 0; i < WAM_MAX_ACTIVE_MOLES; i++) {
        SpriteSheetRegion region = SpriteSheet::readRegion(sprite_soot_moleRegions, 0);
        slots[i].avatar = moleSheet.createAvatar(-100, -100, region);
        slots[i].active = false;
        slots[i].hideAtMs = 0;
        appendAvatar(slots[i].avatar);
      }

      initBannerPool();

      wasTouching = false;
      suppressTouchUntilMs = millis() + 400;
      resetRound();
    }

    void destroyScene() {
      for (int i = 0; i < WAM_MAX_ACTIVE_MOLES; i++) {
        slots[i].avatar = NULL;
      }
      for (int i = 0; i < WAM_BANNER_POOL; i++) {
        bannerGlyphs[i] = NULL;
      }
      wasTouching = false;
      GameScene::destroyScene();
    }

  private:
    MoleSlot slots[WAM_MAX_ACTIVE_MOLES];
    Avatar *bannerGlyphs[WAM_BANNER_POOL];
    int bannerActiveCount = 0;

    WhackAMoleState state = WAM_STATE_READY;
    unsigned long stateStartMs = 0;
    unsigned long roundEndMs = 0;
    unsigned long nextSpawnMs = 0;

    int score = 0;
    int hits = 0;
    int level = 1;
    bool levelUpVisible = false;
    unsigned long levelUpClearAtMs = 0;

    int lastHudTimeLeft = -1;
    boolean wasTouching = false;
    unsigned long suppressTouchUntilMs = 0;

    uint16_t colorWoodBg() const { return rgb565(147, 100, 37); }
    uint16_t colorGold() const { return rgb565(214, 163, 80); }

    SpriteSheet sootSheet() const {
      return SpriteSheet(sprite_soot_mole, sprite_soot_moleMask, SPRITE_SOOT_MOLE_WIDTH, SPRITE_SOOT_MOLE_HEIGHT);
    }

    void initBannerPool() {
      SpriteSheet sheet = SpriteText::letterSheet();
      SpriteSheetRegion placeholder = SpriteSheet::readRegion(sprite_lettersRegions, 0);
      for (int i = 0; i < WAM_BANNER_POOL; i++) {
        bannerGlyphs[i] = sheet.createAvatar(-100, -100, placeholder);
        appendAvatar(bannerGlyphs[i]);
      }
      bannerActiveCount = 0;
    }

    void showBanner(const char *text, int y) {
      SpriteSheet sheet = SpriteText::letterSheet();
      int width = SpriteText::measureWidth(text);
      int cursor = (SCREENWIDTH - width) / 2;
      int count = 0;

      for (const char *p = text; *p != '\0' && count < WAM_BANNER_POOL; ++p) {
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

      for (int i = count; i < WAM_BANNER_POOL; i++) {
        if (bannerGlyphs[i]->x >= 0) {
          bannerGlyphs[i]->setPos(-100, -100);
        }
      }

      bannerActiveCount = count;
      requestRender();
    }

    void hideBanner() {
      for (int i = 0; i < WAM_BANNER_POOL; i++) {
        bannerGlyphs[i]->setPos(-100, -100);
      }
      bannerActiveCount = 0;
      requestRender();
    }

    void hideAllMoles() {
      for (int i = 0; i < WAM_MAX_ACTIVE_MOLES; i++) {
        slots[i].active = false;
        slots[i].hideAtMs = 0;
        slots[i].avatar->setPos(-100, -100);
      }
    }

    void resetRound() {
      score = 0;
      hits = 0;
      level = 1;
      levelUpVisible = false;
      levelUpClearAtMs = 0;
      state = WAM_STATE_READY;
      stateStartMs = millis();
      roundEndMs = 0;
      nextSpawnMs = 0;
      lastHudTimeLeft = -1;

      hideAllMoles();
      hideBanner();
      renderFullScreen();
      drawHudSign();

      drawHudField(WAM_HUD_TIME_CX, WAM_HUD_TIME_W, WAM_ROUND_MS / 1000, 2);
      drawHudField(WAM_HUD_SCORE_CX, WAM_HUD_SCORE_W, 0, 3);
      drawHudField(WAM_HUD_HITS_CX, WAM_HUD_HITS_W, 0, 2);

      showBanner("GET READY", 130);
      requestRender();
    }

    void beginPlay(unsigned long now) {
      state = WAM_STATE_PLAYING;
      stateStartMs = now;
      roundEndMs = now + WAM_ROUND_MS;
      nextSpawnMs = now + 400;
      lastHudTimeLeft = -1;
      hideBanner();
      addSound(NOTE_C5, noteDurationMs(8, 800));
    }

    int levelForHits(int forHits) const {
      int lvl = 1 + forHits / WAM_HITS_PER_LEVEL;
      return lvl > WAM_MAX_LEVEL ? WAM_MAX_LEVEL : lvl;
    }

    void endRound() {
      state = WAM_STATE_ENDED;
      hideAllMoles();
      showBanner("TIME UP", 130);

      char buf[40];
      _tft->fillRoundRect(20, 188, SCREENWIDTH - 40, 48, 6, colorWoodBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorGold(), colorWoodBg());
      snprintf(buf, sizeof(buf), "Score %d  Hits %d  Lv %d", score, hits, level);
      _tft->drawString(buf, SCREENWIDTH / 2, 204, 2);
      _tft->drawString("Tap to play again", SCREENWIDTH / 2, 224, 2);
      _tft->setTextDatum(TL_DATUM);

      addSound(NOTE_G4, noteDurationMs(8, 700));
      addSound(NOTE_E4, noteDurationMs(8, 700));
      requestRender();
    }

    void updateMoles(unsigned long now) {
      for (int i = 0; i < WAM_MAX_ACTIVE_MOLES; i++) {
        if (slots[i].active && now >= slots[i].hideAtMs) {
          slots[i].active = false;
          slots[i].avatar->setPos(-100, -100);
          requestRender();
        }
      }
    }

    void updateLevelUpBanner(unsigned long now) {
      if (levelUpVisible && now >= levelUpClearAtMs) {
        levelUpVisible = false;
        hideBanner();
      }
    }

    bool overlapsActiveMole(int16_t x, int16_t y, int exceptIndex) {
      for (int i = 0; i < WAM_MAX_ACTIVE_MOLES; i++) {
        if (i == exceptIndex || !slots[i].active) {
          continue;
        }
        if (x < slots[i].avatar->x + WAM_MOLE_SIZE && x + WAM_MOLE_SIZE > slots[i].avatar->x &&
            y < slots[i].avatar->y + WAM_MOLE_SIZE && y + WAM_MOLE_SIZE > slots[i].avatar->y) {
          return true;
        }
      }
      return false;
    }

    void trySpawnMole(unsigned long now) {
      if (now < nextSpawnMs || levelUpVisible) {
        return;
      }

      int slotIndex = -1;
      for (int i = 0; i < WAM_MAX_ACTIVE_MOLES; i++) {
        if (!slots[i].active) {
          slotIndex = i;
          break;
        }
      }
      if (slotIndex < 0) {
        return;
      }

      int lvl = level - 1;
      int16_t x = 0, y = 0;
      for (int attempt = 0; attempt < 6; attempt++) {
        x = random(0, SCREENWIDTH - WAM_MOLE_SIZE);
        y = random(WAM_PLAY_TOP_Y, WAM_PLAY_BOTTOM_Y - WAM_MOLE_SIZE);
        if (!overlapsActiveMole(x, y, slotIndex)) {
          break;
        }
      }

      int variant = random(0, WAM_MOLE_VARIANTS);
      SpriteSheetRegion region = SpriteSheet::readRegion(sprite_soot_moleRegions, variant);
      SpriteSheet moleSheet = sootSheet();
      moleSheet.applyRegion(slots[slotIndex].avatar, region);
      slots[slotIndex].avatar->setPos(x, y);
      slots[slotIndex].avatar->requestRedraw();
      slots[slotIndex].active = true;

      int baseVisibleMs = random(WAM_VISIBLE_MIN_MS[lvl], WAM_VISIBLE_MAX_MS[lvl]);
      int speedPct = random(WAM_SPEED_FACTOR_MIN_PCT, WAM_SPEED_FACTOR_MAX_PCT + 1);
      int visibleMs = (baseVisibleMs * speedPct) / 100;
      if (visibleMs < WAM_MIN_VISIBLE_MS) {
        visibleMs = WAM_MIN_VISIBLE_MS;
      }
      slots[slotIndex].hideAtMs = now + visibleMs;
      requestRender();

      nextSpawnMs = now + random(WAM_SPAWN_MIN_MS[lvl], WAM_SPAWN_MAX_MS[lvl]);
    }

    void whackAt(uint16_t x, uint16_t y, unsigned long now) {
      for (int i = 0; i < WAM_MAX_ACTIVE_MOLES; i++) {
        if (!slots[i].active) {
          continue;
        }
        if (!slots[i].avatar->contains(x, y)) {
          continue;
        }

        slots[i].active = false;
        slots[i].avatar->setPos(-100, -100);
        requestRender();

        hits++;
        score += WAM_POINTS_PER_HIT * level;
        drawHudField(WAM_HUD_SCORE_CX, WAM_HUD_SCORE_W, score > 999 ? 999 : score, 3);
        drawHudField(WAM_HUD_HITS_CX, WAM_HUD_HITS_W, hits > 99 ? 99 : hits, 2);
        addSound(NOTE_E5, noteDurationMs(16, 900));
        addSound(NOTE_G5, noteDurationMs(32, 900));

        int newLevel = levelForHits(hits);
        if (newLevel > level) {
          level = newLevel;
          showBanner("LEVEL UP", 130);
          levelUpVisible = true;
          levelUpClearAtMs = now + WAM_LEVEL_UP_MS;
          addSound(NOTE_C5, noteDurationMs(16, 900));
          addSound(NOTE_C6, noteDurationMs(8, 900));
        }
        return;
      }

      addSound(NOTE_A3, noteDurationMs(32, 700));
    }

    // Wooden sign + TIME/SCORE/HITS labels, drawn once after a full repaint.
    // Moles are constrained above it (WAM_PLAY_BOTTOM_Y), so the dirty-rect
    // renderer never repaints over it during play.
    void drawHudSign() {
      uint16_t wood = colorWoodBg();
      uint16_t border = rgb565(90, 60, 20);
      _tft->fillRoundRect(WAM_HUD_SIGN_X, WAM_HUD_SIGN_Y, WAM_HUD_SIGN_W, WAM_HUD_SIGN_H, 6, wood);
      _tft->drawRoundRect(WAM_HUD_SIGN_X, WAM_HUD_SIGN_Y, WAM_HUD_SIGN_W, WAM_HUD_SIGN_H, 6, border);
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorGold(), wood);
      _tft->drawString("TIME", WAM_HUD_TIME_CX, WAM_HUD_LABEL_CY, 1);
      _tft->drawString("SCORE", WAM_HUD_SCORE_CX, WAM_HUD_LABEL_CY, 1);
      _tft->drawString("HITS", WAM_HUD_HITS_CX, WAM_HUD_LABEL_CY, 1);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawHudField(int centerX, int width, int value, int digits) {
      char buf[8];
      if (digits == 3) {
        snprintf(buf, sizeof(buf), "%03d", value);
      } else {
        snprintf(buf, sizeof(buf), "%02d", value);
      }

      _tft->fillRect(centerX - width / 2, WAM_HUD_DIGIT_CY - WAM_HUD_FIELD_H / 2, width, WAM_HUD_FIELD_H, colorWoodBg());
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(colorGold(), colorWoodBg());
      _tft->drawString(buf, centerX, WAM_HUD_DIGIT_CY, 2);
      _tft->setTextDatum(TL_DATUM);
    }
};

#endif
