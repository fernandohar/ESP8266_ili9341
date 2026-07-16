#ifndef _GAMESCENE_H_
#define _GAMESCENE_H_
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Avatar.h"
#include "SoundPlayer.h"
#include "pitches.h"
#define MAX_AVATAR 50
#define SCREENWIDTH 240
#define SCREENHEIGHT 320
#define SPEAKER_PIN 16 //D0 - GPIO16
class GameScene {
  public:
    virtual void update(boolean isTouching, boolean* needChangeScene, int* nextSceneIndex) = 0;  //function to update Game logic
    virtual void render() = 0; //function to render the Scene
    virtual void initScene() =  0;
    void destroyScene();

    void appendAvatar(Avatar * avatar);

    void setBackground(const uint16_t* background);
    void setBackground(const uint16_t* background, const uint32_t backgroundWidth);
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
    

    boolean boundToScreen(Avatar* avatar) {
      boolean reversed = false;
      if ( (avatar->x <= 0) || ((avatar->x + avatar->width) >= SCREENWIDTH)) {
        avatar->velocity.x *= -1;//Change the movement direction
        avatar->x = (avatar->x <= 0) ? 0 : SCREENWIDTH - avatar->width ; //Clip the image within the screen
        reversed = true;
      }
      if (avatar->y <= 0) {
        avatar->y = 0;
        //
#ifdef PHYSICS
        avatar->velocity.y *= -0.8; //We hit the top, lost energy
#else
        avatar->velocity.y *= -1;
#endif
        reversed = true;
      } else if ((avatar->y + avatar->height) >= SCREENHEIGHT) {
        avatar->y = SCREENHEIGHT - avatar->height;

#ifdef PHYSICS
        avatar->velocity.y *= -0.85; //suppose we have energy lost in both x and y when hit the floor
        avatar->velocity.x *= 0.9;
#else
        avatar->velocity.y *= -1;
#endif
        reversed = true;
      }

      return reversed;
    }
  protected:
    TFT_eSPI *_tft;
    Avatar* avatars[MAX_AVATAR] = {};
    int numAvatar = 0;
    uint16_t renderbuf[2][SCREENWIDTH];
    const uint16_t *background = NULL;
    uint32_t backgroundWidth;
    uint16_t backgroundXOffset;
    uint16_t bgColor;
    boolean _renderRequested = false;

    void drawBackground(const uint16_t* bitmap) {
      _tft->pushImage( 0, 0, SCREENWIDTH, SCREENHEIGHT, bitmap);
    }
    uint32_t getBackgoundMemoryPosition( uint16_t x, uint16_t y);
    //Support Extra wide background image, with horizontal cropping
    void drawBackground(const uint16_t* bitmap, uint16_t imageWidth, uint16_t imageXOffset);
    void drawBackground(const uint16_t* bitmap, uint16_t imageXOffset);
    
    void renderScene() ;
    void renderScene(boolean refreshBackground);
    void renderFullScreen();
    void markAvatarsUnder(Avatar* mover);
    void drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr);
    
    void fillBufferWithColor(uint16_t width, uint16_t color, uint16_t * destPtr);

    void drawAvatar2Buffer(Avatar *avatar, uint16_t* destPtr, uint16_t y, uint16_t maxWidth, uint16_t srcStartX = 0);
    
    boolean isDebugEnabled = false;
    void enableDebug() { isDebugEnabled = true; }
    void disableDebug() {isDebugEnabled = false; }
  private:
    int getNextRenderAvatar(int previousMin, int toBeRendered2RenderableMap[], int toBeRenderedIndex);
    int collectRowRedrawSpans(int16_t screenY, int16_t clipMinx, int16_t clipMaxx,
                              int16_t unionDirtyMinx, int16_t unionDirtyMaxx,
                              int16_t unionDirtyMiny, int16_t unionDirtyMaxy,
                              Avatar** shortlist, const bool* fullRedraw, int shortlistCount,
                              int16_t* spanStarts, int16_t* spanEnds, int maxSpans);
    uint16_t debugColor = rgb565(230, 157, 132);

};

#endif
