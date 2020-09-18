#ifndef _GAMESCENE_H_
#define _GAMESCENE_H_
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Avatar.h"
#define MAX_AVATAR 50
#define SCREENWIDTH 240
#define SCREENHEIGHT 320

//#define DEBUG_RENDERAREA
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

    void renderScene() {
#ifdef DEBUG_RENDERAREA
      Serial.println("in RenderScrene");
#endif

      Avatar * renderableAvatar[numAvatar]; //for shortlisting of avatar that will affect the screen display
      int16_t renderableMinx[numAvatar]; //for calculation of discrete drawing area
      int16_t renderableMiny[numAvatar];
      int16_t renderableMaxx[numAvatar];
      int16_t renderableMaxy[numAvatar];


      int numRenderableAvatar = 0;

      //Calculate the renderable area for each avatar, and found out which should be drawn on screen
      for (int i  = 0; i < numAvatar;  ++i) {
        Avatar *avatar = avatars[i];
        //avatar->previousRenderedX, avatar->previousRenderedY, avatar->x, avatar->y, avatar->width, avatar->height, avatar->bitmap, avatar->mask, SCREENWIDTH
        float oldX = avatar->previousRenderedX;
        float oldY = avatar->previousRenderedY;
        float newX = avatar->x;
        float newY = avatar->y;

        float avatarMinx = (oldX < newX) ? oldX : newX;
        float avatarMiny = (oldY < newY) ? oldY : newY;
        float avatarMaxx = ((oldX < newX) ? newX : oldX) + avatar->width - 1;
        float avatarMaxy = ((oldY < newY) ? newY : oldY) + avatar->height - 1;

        if (avatarMinx > SCREENWIDTH || avatarMaxx < 0 || avatarMiny > SCREENHEIGHT || avatarMaxy < 0) { //No renderable pixels (old and new location falls outside screen
#ifdef DEBUG_RENDERAREA
          Serial.print("Avatar # ");
          Serial.print(i);
          Serial.println(" is outside screen");
#endif
          continue;
        }
        renderableAvatar[numRenderableAvatar] = avatar; //Shortlist renderable Avatar
        renderableMinx[numRenderableAvatar] = avatarMinx;
        renderableMiny[numRenderableAvatar] = avatarMiny;
        renderableMaxx[numRenderableAvatar] = avatarMaxx;
        renderableMaxy[numRenderableAvatar] = avatarMaxy;
        numRenderableAvatar++;
      }
#ifdef DEBUG_RENDERAREA
      Serial.print("Number of renderable avatar: ");
      Serial.println(numRenderableAvatar);
      for (int i = 0 ; i < numRenderableAvatar; ++i) {
        Serial.print("renderableMin/Max xy");
        Serial.print(renderableMinx[i]);
        Serial.print(",");
        Serial.print(renderableMiny[i]);
        Serial.print(",");
        Serial.print(renderableMaxx[i]);
        Serial.print(",");
        Serial.println(renderableMaxy[i]);
      }
#endif
      //Find a list of Discrete drawing area
      bool rendered[numRenderableAvatar]; //keep track of which avatar has been rendered
      for (int i = 0 ; i < numRenderableAvatar; ++i) {
        rendered[i] = false;
      }

      //Find Min / max area to draw
      //minx, miny, maxX, maxY are in Screen coordinates
      int16_t minx = 241;
      int16_t miny = 321;
      int16_t maxx = -1;
      int16_t maxy = -1;

      Avatar* toBeRendered[numRenderableAvatar]; //working array for i loop
      int toBeRenderedIndex;
      for (int i = 0; i < numRenderableAvatar; ++i) {
        toBeRenderedIndex = 0;
        minx = 241;
        miny = 321;
        maxx = -1;
        maxy = -1;
        if (!rendered[i]) {

#ifdef DEBUG_RENDERAREA
          Serial.print("i ");
          Serial.print(i);
          Serial.println("not rendered, save its min, max xy");
#endif

          toBeRendered[toBeRenderedIndex++] = renderableAvatar[i];
          minx = renderableMinx[i];
          miny = renderableMiny[i];
          maxx = renderableMaxx[i];
          maxy = renderableMaxy[i];
          rendered[i] = true;
        }

        for (int j = i + 1; j < numRenderableAvatar; ++j) {
#ifdef DEBUG_RENDERAREA
          Serial.print("j loop: checking renderable Avatar # ");
          Serial.println(j);
#endif
          if (rendered[j]) {
            
#ifdef DEBUG_RENDERAREA
            Serial.print("j ");
            Serial.print(j);
            Serial.println("rendered, continue next");
#endif
            continue;
          }
          if (maxx < renderableMinx[j] || minx > renderableMaxx[j] ||
              maxy < renderableMiny[j] || miny > renderableMaxy[j]) {
            //not overlap, check next
#ifdef DEBUG_RENDERAREA
            Serial.println("not overlap, continue next");
#endif
            continue;
          } else {
            //add up the renderable area
#ifdef DEBUG_RENDERAREA
            Serial.println("Overlap, calculate new area");
#endif
            toBeRendered[toBeRenderedIndex++] = renderableAvatar[j];
            rendered[j] = true;
            minx = (minx < renderableMinx[j]) ? minx : renderableMinx[j];
            miny = (miny < renderableMiny[j]) ? miny : renderableMiny[j];
            maxx = (maxx > renderableMaxx[j]) ? maxx : renderableMaxx[j];
            maxy = (maxy > renderableMaxy[j]) ? maxy : renderableMaxy[j];
          }
        }
        //Finished checking i with j, render
        //If toBeRenderedIndex > 0, do rendering of the area: minx,miny,maxx,maxy
        if (toBeRenderedIndex > 0) {
#ifdef DEBUG_RENDERAREA
          Serial.print("Rendering, number of Avatar:" );
          Serial.print(toBeRenderedIndex);
          Serial.print(" minx, miny: ");
          Serial.print(minx);
          Serial.print(", ");
          Serial.print(miny);

          Serial.print(" maxx, maxy: ");
          Serial.print(maxx);
          Serial.print(", ");
          Serial.println(maxy);
#endif
          //cap the drawing window to the screen
          minx = (minx < 0) ? 0 : minx;
          miny = (miny < 0) ? 0 : miny;
          maxx = (maxx > SCREENWIDTH) ? SCREENWIDTH : maxx;
          maxy = (maxy > SCREENHEIGHT) ? SCREENHEIGHT : maxy;
          uint16_t renderWidth = maxx - minx;
          uint16_t renderHeight = maxy - miny;

          uint16_t *destPtr;
          int8_t bufIdx = 0;
          for(int y = 0; y < renderHeight; y++){
            destPtr =  &renderbuf[bufIdx][0];
            drawBg2Buffer(minx, miny + y, renderWidth, destPtr);
            //minx = position WRT BG bitmap
            //miny + y = y position WRT BG bitmap
            //renderWidth = how many pixel to render
            //destPtr = Buffer's pointer
            
            for(int i = 0; i < toBeRenderedIndex; ++i){
              //if(miny + y > )
              //drawToBuffer(y, toBeRendered[i], )  
            }
            //Finished drawing to Buffer, flush buffer to TFT
             
            bufIdx = 1 - bufIdx; //change our renderbuffer (0, 1)
          }
        }
      }
      //Prepare to find the next renderable area
#ifdef DEBUG_RENDERAREA
      Serial.println("");
      delay(500);
#endif
    }

    //
    void drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr){
        
    }
    
    void renderCharacter(int16_t oldX, int16_t oldY, int16_t newX, int16_t newY, int16_t imageWidth, int16_t imageHeight, const uint16_t imageBitmap[],
                         const uint8_t imageMask[], int16_t backgroundWidth) {
      //Found out minimum the screen area to update. (to erase the old image and draw new image)
      //The area is combined bounds of previous and current position
      int16_t minx, miny, maxx, maxy, renderWidth, renderHeight;
      minx = (oldX < newX) ? oldX : newX;
      miny = (oldY < newY) ? oldY : newY;
      maxx = ((oldX < newX) ? newX : oldX) + imageWidth - 1;
      maxy = ((oldY < newY) ? newY : oldY) + imageHeight - 1;

      renderWidth = maxx - minx + 1;
      renderHeight = maxy - miny + 1;
      bool prefetchMask = false;
      int8_t maskOffset = 0;
      if (minx < 0) { //keep the rendering area within the screen, and reduce the renderarea if necessary
        renderWidth += minx;
        maskOffset = minx % 8;
        minx = 0;
        prefetchMask = true;
      }

      if (miny < 0) {
        renderHeight += miny;
        miny = 0 ;
      }

      if (maxx >= SCREENWIDTH) {
        renderWidth -= (maxx - SCREENWIDTH + 1);
        maxx = SCREENWIDTH - 1;
      }

      if (maxy >= SCREENHEIGHT) {
        renderHeight -= (maxy - SCREENHEIGHT + 1);
        maxy = SCREENHEIGHT - 1;
      }

      _tft->endWrite();
      _tft->startWrite();

      if (renderWidth > SCREENWIDTH) {
        return;
      }
      _tft->setAddrWindow(minx, miny, renderWidth, renderHeight);

      // Only the changed rectangle is drawn into the 'renderbuf' array...
      uint16_t c, *destPtr;
      int16_t bx = minx - newX, //position with respect to bitmap
              by = miny - newY,
              bgx = minx,
              bgy = miny;
      int16_t  x, y, bx1, bgx1;         // Loop counters and working vars
      int8_t bufIdx = 0;
      uint8_t p;

      int16_t bw = (imageWidth + 7) / 8; // Bitmask scanline pad = whole byte
      uint8_t byte = 0;

      //The following loop work with each row(x), and then moves downwards (y)
      for (y = 0; y < renderHeight; y++) {
        //_tft->readRect(0, y, SCREENWIDTH, 1, currentScreenScanLineBuffer);
        destPtr = &renderbuf[bufIdx][0];
        bx1 = bx;
        bgx1 = bgx;
        boolean maskread = false;
        for (x = 0; x < renderWidth; x++) {
          if ( (bx1 >= 0) &&  (bx1 < imageWidth )   //Check if current pixel is inside the image
               && (by >= 0) &&  (by < imageHeight)
             ) {
            if (prefetchMask && !maskread) { //to handle when image is moved less to offscreen to the left, and we need to pre-fetch the mask
              byte = pgm_read_byte(&imageMask[by * bw + bx1 / 8]);
              byte << maskOffset;
              maskread = true;
            }
            //Check mask to see if we should draw bitmap or background
            if (bx1 & 7) {
              byte <<= 1;
            } else {
              byte = pgm_read_byte(&imageMask[by * bw + bx1 / 8]);
            }
            if (byte & 0x80) {
              c = pgm_read_word_near(imageBitmap + ( by * imageWidth ) + bx1 );
            } else {
              c = pgm_read_word_near(background + (bgy * backgroundWidth) + x + minx);
            }

          } else { //outside the image area, draw background
            c = pgm_read_word_near(background + (bgy * backgroundWidth) + x + minx);

          }
          bx1++;
          bgx++;
          *destPtr++ = c; // Store pixel color
        }
        _tft->pushPixels(&renderbuf[bufIdx][0], renderWidth);

        bufIdx = 1 - bufIdx; //change our renderbuffer (0, 1)
        by++;
        bgy++;

      }
    }
};

#endif
