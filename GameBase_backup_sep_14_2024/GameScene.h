#ifndef _GAMESCENE_H_
#define _GAMESCENE_H_
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Avatar.h"
#include "pitches.h"
#define MAX_AVATAR 50
#define SCREENWIDTH 240
#define SCREENHEIGHT 320
#define SPEAKER_PIN 16 //D0 - GPIO16
#define MAX_SOUND_TONE_SIZE 200 //the maximum number of notes in the sound
class GameScene {
  public:
    virtual void update(boolean isTouching, boolean* needChangeScene, int* nextSceneIndex) = 0;  //function to update Game logic
    virtual void render() = 0; //function to render the Scene
    virtual void initScene() =  0;
    void destroyScene() {
      numAvatar = 0;
      for (int i = 0; i < MAX_AVATAR; i++) {
        delete avatars[i];
        avatars[i] = NULL;
      }
    }

    void appendAvatar(Avatar * avatar) {
      avatars[numAvatar++] = avatar;
    }

    void setBackground(const uint16_t* background) {
      this->background = background;
    }
    void setBackgroundColor(const uint16_t bgColor) {
      this->bgColor = bgColor;
    }
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
    void addSound(int soundTone, int soundDuration) {
      if (soundCount >= MAX_SOUND_TONE_SIZE) {
        Serial.println("sound dropped");
        return;
      }
      soundToneArr[soundCountTail] = soundTone;
      soundDurationArr[soundCountTail] = soundDuration;
      if (++soundCountTail >= MAX_SOUND_TONE_SIZE) {
        soundCountTail = 0;
      }
      soundCount++;
    }
    void playSound();
    

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
    Avatar* avatars[MAX_AVATAR];
    int numAvatar = 0;
    uint16_t renderbuf[2][SCREENWIDTH];
    const uint16_t *background = NULL;
    uint16_t bgColor;

    // notes in the melody:
    int soundToneArr[MAX_SOUND_TONE_SIZE];  // = { NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4 };
    int soundDurationArr[MAX_SOUND_TONE_SIZE]; //milliseconds
    int soundCount = 0;
    int soundCountHead = 0;
    int soundCountTail = 0;
    boolean playingSound = false;
    unsigned long soundStop = 0;

    void drawBackground(const uint16_t* bitmap) {
      _tft->pushImage( 0, 0, SCREENWIDTH, SCREENHEIGHT, bitmap);
    }

    void renderScene() ;

    void drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr);

    void fillBufferWithColor(uint16_t width, uint16_t color, uint16_t * destPtr);

    void drawAvatar2Buffer(Avatar *avatar, uint16_t* destPtr, uint16_t y);

    //renderCharacter is depreciated, overlapping object are not rendered correctly. use renderScene() instead
    void renderCharacter(int16_t oldX, int16_t oldY, int16_t newX, int16_t newY, int16_t imageWidth, int16_t imageHeight, const uint16_t imageBitmap[],
                         const uint8_t imageMask[], int16_t backgroundWidth);
  private:
    int getNextRenderAvatar(int previousMin, int toBeRendered2RenderableMap[], int toBeRenderedIndex);
    uint16_t debugColor = rgb565(230, 157, 132);

};

#endif
