#ifndef _GAMESCENE_H_
#define _GAMESCENE_H_
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Avatar.h"
#include "SpriteAsset.h"
#include "SoundPlayer.h"
#include "pitches.h"
#define MAX_AVATAR 50
#define SCREENWIDTH 240
#define SCREENHEIGHT 320
#define SPEAKER_PIN 16 //D0 - GPIO16

// Both the logging and the inference build need the gesture sampler running. Derived
// here rather than in GameSceneManager.h so that GameScene.cpp sees it too - the
// renderer has to cooperate with the sampler to keep it on schedule.
#if defined(TINYML_GESTURE_LOG) || defined(TINYML_GESTURE_INFERENCE)
#define GESTURE_TOUCH_SAMPLER 1
#endif

class GameScene {
  public:
    virtual void update(boolean isTouching, boolean* needChangeScene, int* nextSceneIndex) = 0;  //function to update Game logic
    virtual void render() = 0; //function to render the Scene
    virtual void initScene() =  0;
    void destroyScene();

    void appendAvatar(Avatar * avatar);

    void setBackground(const uint16_t* background);
    void setBackground(const uint16_t* background, const uint32_t backgroundWidth);
    void setBackgroundAsset(const SpriteAsset *asset);
    void setBackgroundAsset(const SpriteAsset *asset, uint32_t assetWidth);
    // Use a small bitmap as a repeating (tiled) background: it is wrapped with
    // modulo indexing across the whole screen in both X and Y, so a tiny tile
    // (e.g. 50x50) can fill 240x320 without a full-screen PROGMEM image.
    void setBackgroundTile(const uint16_t* tile, uint16_t tileWidth, uint16_t tileHeight);
    void setBackgroundTileAsset(const SpriteAsset *tile);
    void setBackgroundColor(const uint16_t bgColor);
    void setBackgroundOffset(uint16_t imageXOffset);
    static uint16_t rgb565(float r, float g, float b) {
      uint16_t red = ceil(r / 255.0 * 31.0);
      uint16_t green = ceil(g / 255.0 * 63.0);
      uint16_t blue = ceil(b / 255.0 * 31.0);
      byte high = 0;
      byte low = 0;
      high = red << 3;
      high = high | green >> 3;
      green = ceil(g / 255.0 * 63.0);
      low = green << 5;
      low = low | blue;
      uint16_t result = (high << 8) | low;
      return result;
    }

    
    // SOUND
    void addSound(int soundTone, int soundDuration);
    void requestRender() { _renderRequested = true; }
    bool consumeRenderRequest() {
      if (!_renderRequested) {
        return false;
      }
      _renderRequested = false;
      return true;
    }
    // noteDivisor: 1=whole, 2=half, 4=quarter, 8=eighth, 16=sixteenth, etc.
    static int noteDurationMs(int noteDivisor, int tempoBpm = 160) {
      if (noteDivisor <= 0) {
        return 0;
      }
      return (60000 * 4) / tempoBpm / noteDivisor;
    }

  protected:
    TFT_eSPI *_tft;
    Avatar* avatars[MAX_AVATAR] = {};
    int numAvatar = 0;
    uint16_t renderbuf[2][SCREENWIDTH];
    const uint16_t *background = NULL;
    const SpriteAsset *backgroundAsset = NULL;
    uint32_t backgroundWidth;
    uint16_t backgroundXOffset;
    uint16_t bgColor;
    bool backgroundTiled = false;
    uint16_t backgroundTileWidth = 0;
    uint16_t backgroundTileHeight = 0;
    boolean _renderRequested = false;

    void drawBackground(const uint16_t* bitmap) {
      _tft->pushImage( 0, 0, SCREENWIDTH, SCREENHEIGHT, bitmap);
    }
    void drawBackgroundAsset(const SpriteAsset *asset);
    uint32_t getBackgoundMemoryPosition( uint16_t x, uint16_t y);
    //Support Extra wide background image, with horizontal cropping
    void drawBackground(const uint16_t* bitmap, uint16_t imageWidth, uint16_t imageXOffset);
    void drawBackground(const uint16_t* bitmap, uint16_t imageXOffset);
    
    void renderScene() ;
    void renderScene(boolean refreshBackground);
    void renderFullScreen();
    void drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr);

    void drawAvatar2Buffer(Avatar *avatar, uint16_t* destPtr, uint16_t y, uint16_t maxWidth, uint16_t srcStartX = 0);
    
    boolean isDebugEnabled = false;
    void enableDebug() { isDebugEnabled = true; }
    void disableDebug() {isDebugEnabled = false; }
  private:
    // Pushing a frame holds the SPI bus for tens of milliseconds, which is long
    // enough for a whole tap to begin and end unseen. Called between rows so the
    // gesture sampler keeps its own clock through a repaint.
    void serviceTouchSampler();

    int collectRowRedrawSpans(int16_t screenY, int16_t clipMinx, int16_t clipMaxx,
                              int16_t unionDirtyMinx, int16_t unionDirtyMaxx,
                              int16_t unionDirtyMiny, int16_t unionDirtyMaxy,
                              Avatar** shortlist, const bool* fullRedraw, int shortlistCount,
                              int16_t* spanStarts, int16_t* spanEnds, int maxSpans);
};

#endif
