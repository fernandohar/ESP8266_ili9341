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

  protected:
    TFT_eSPI *_tft;
    Avatar* avatars[MAX_AVATAR];
    int numAvatar = 0;
    uint16_t renderbuf[2][SCREENWIDTH];
    const uint16_t *background;

    void drawBackground(const uint16_t* bitmap) {
      _tft->pushImage( 0, 0, SCREENWIDTH, SCREENHEIGHT, bitmap);
    }

    void renderScene() ;

    void drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr);

    void drawAvatar2Buffer(Avatar *avatar, uint16_t* destPtr, uint16_t y);

    //renderCharacter is depreciated, overlapping object are not rendered correctly. use renderScene() instead 
    void renderCharacter(int16_t oldX, int16_t oldY, int16_t newX, int16_t newY, int16_t imageWidth, int16_t imageHeight, const uint16_t imageBitmap[],
                            const uint8_t imageMask[], int16_t backgroundWidth);
  private:
   int getNextRenderAvatar(int previousMin, int toBeRendered2RenderableMap[], int toBeRenderedIndex);
};

#endif
