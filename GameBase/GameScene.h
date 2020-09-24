#ifndef _GAMESCENE_H_
#define _GAMESCENE_H_
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Avatar.h"
#define MAX_AVATAR 50
#define SCREENWIDTH 240
#define SCREENHEIGHT 320


class GameScene {
  public:
    virtual void update(boolean isTouching, uint16_t touchX, uint16_t touchY, boolean* needChangeScene, int* nextSceneIndex) = 0;  //function to update Game logic
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
    void setBackgroundColor(const uint16_t bgColor){
      this->bgColor = bgColor;
    }
    uint16_t rgb565(float r, float g, float b){
      uint16_t red = ceil(r / 255.0 * 31.0);
//      Serial.println(r);
//      Serial.println(red);
//      Serial.println(red, BIN);
      uint16_t green = ceil(g / 255.0 * 63.0);
//      Serial.println(g);
//      Serial.println(green);
//      Serial.println(green, BIN);
      uint16_t blue = ceil(b / 255.0 * 31.0);
//      Serial.println(b);
//      Serial.println(blue);
//      Serial.println(blue, BIN);
      byte high = 0;
      byte low = 0;

      high = red << 3;
      //Serial.println(high, BIN);
      high = high | green >> 3;
      //Serial.println(high, BIN);
      green = ceil(g / 255.0 * 63.0);
      low = green << 5;
      low = low | blue;
      //Serial.println(low, BIN);
      
      uint16_t result = (high << 8) | low;
//      Serial.println(result, BIN);
//      Serial.println(0xAFE5, BIN);
      return result;
    }
  protected:
    TFT_eSPI *_tft;
    Avatar* avatars[MAX_AVATAR];
    int numAvatar = 0;
    uint16_t renderbuf[2][SCREENWIDTH];
    const uint16_t *background = NULL;
    uint16_t bgColor;
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
};

#endif
