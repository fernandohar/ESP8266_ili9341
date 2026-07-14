#include "GameScene.h"

//#define DEBUG_RENDERSCENE

void GameScene :: destroyScene() {
  numAvatar = 0;
  for (int i = 0; i < MAX_AVATAR; i++) {
    delete avatars[i];
    avatars[i] = NULL;
  }
}

void GameScene :: appendAvatar(Avatar * avatar) {
  avatars[numAvatar++] = avatar;
}
    
void GameScene :: setBackground(const uint16_t* background) {
  setBackground(background, SCREENWIDTH);
}
void GameScene :: setBackground(const uint16_t* background, const uint32_t backgroundWidth) {
  this->background = background;
  this->backgroundWidth = backgroundWidth;
  this->backgroundXOffset = 0;
}
void GameScene :: setBackgroundColor(const uint16_t bgColor) {
  this->bgColor = bgColor;
}
    
void GameScene :: drawBackground(const uint16_t* bitmap, uint16_t imageXOffset) {
  drawBackground(bitmap, backgroundWidth, imageXOffset);
}

uint32_t GameScene :: getBackgoundMemoryPosition( uint16_t x, uint16_t y) {
//     return  backgroundXOffset + (backgroundWidth * y) + x;
      return  (uint32_t)backgroundXOffset + ((uint32_t)backgroundWidth * (uint32_t) y) + (uint32_t)x;
}

void GameScene :: setBackgroundOffset(uint16_t imageXOffset) {
  backgroundXOffset = imageXOffset;
}


void GameScene :: drawBackground(const uint16_t* bitmap, uint16_t imageWidth, uint16_t imageXOffset) {
  backgroundXOffset = imageXOffset;
  int8_t bufIdx = 0;
  uint16_t *destPtr;

  _tft->endWrite();
  _tft->startWrite();

  _tft->setAddrWindow(0, 0, SCREENWIDTH, SCREENHEIGHT);
  if (isDebugEnabled) {
    Serial.printf ("\n[DEBUG] drawBackground( uint16_t, uint16_t, uint16_t) \n");
  }
  uint16_t y, x;
  for (y = 0; y < SCREENHEIGHT; y++) {
    destPtr = &renderbuf[bufIdx][0];
    
    if (isDebugEnabled ) {
      Serial.printf("\n y=[%d]", y); // Each "Row" will be printed on its own line  
    }
    
    for (x = 0; x < SCREENWIDTH; x++) {
      uint32_t pos = getBackgoundMemoryPosition(x, y);
      
      if (isDebugEnabled) {
        if (x == 0) {
          Serial.printf("&background: %hu ", (&background));
          //Serial.println(*((uint16_t*)background));
          Serial.printf("backgroundMemoryPosition: %d \t\t", pos);
        }
        
        *destPtr = pgm_read_word_far(pos + background);
        if (isDebugEnabled) {
          Serial.printf(" %x ", (*destPtr )); //Print the content of the pointer, which is the data retrieved by pgm_read_word_far
          //Serial.printf(" %hu ", (&destPtr));  //each address should be increased by the pointer sized
        }
        destPtr = destPtr + 1;
      } else {
        *destPtr++ = pgm_read_word_near(pos + background);
      }
    }
    _tft->pushPixels(&renderbuf[bufIdx][0], 240);
    bufIdx = 1 - bufIdx; //alternate our renderbuffer (0, 1)
  }
}

void GameScene::drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr) {
  uint16_t c;
  uint32_t pos = getBackgoundMemoryPosition(x, y);
  if (isDebugEnabled) {
    Serial.printf("\n[DEBUG] drawBg2Buffer(uint16_t %d, uint16_t %d, uint16_t %d, uint16_t)\n", x, y, width);
    Serial.printf("pos: %d\n", pos);
  }
  for (uint32_t i = 0; i < width; ++i) {
    //c = pgm_read_word_near(background + pos + i);  
    c = pgm_read_word_far(background + pos + i);  
    *destPtr++ = c;
    if (isDebugEnabled) {
       Serial.printf(" %hu ", c);
    }
    
  }
}
void GameScene::fillBufferWithColor(uint16_t width, uint16_t color, uint16_t * destPtr) {
  for (int i = 0; i < width; ++i) {
    *destPtr++ = color;
  }
}

void GameScene::drawAvatar2Buffer(Avatar *avatar, uint16_t* destPtr, uint16_t y) {
  avatar->updateFrameIndex(millis());

  uint16_t renderwidth = (avatar->x < 0) ? (avatar->width + avatar->x) : avatar->width; //note the sign of x is -'ve in first condition.
  uint16_t c;
  int16_t bw = (avatar->width + 7) / 8; //number of bytes for each row of mask

  boolean maskRead = true;
  uint16_t maskoffset = 0;
  uint16_t bitmapoffset = 0;
  //uint16_t nextMaskOffset = 0;
  uint8_t maskByte = 0; //variable to hold the Byte read from mask;
  if (avatar->x < 0) {
    
    maskRead  = false;
    
    const uint8_t* mask = avatar->getMask();

    //For every 8 pixels the avatar X is negative, the mask need to be shifted 1 byte to the right
    bitmapoffset = abs(avatar->x);
    maskByte = pgm_read_byte(&(mask[ bw * y +  bitmapoffset / 8  ]));

    maskoffset = uint16_t(abs(avatar->x)) % 8; //for image falls less than 0 in x axis, ad
    maskByte <<= maskoffset;
    
  }

  for (uint16_t x = 0 ; x < renderwidth; x++) {
    if ((x + bitmapoffset) & 7) {   //for x = 1 ... 7, (x & 7) is true
      if (maskRead) {
        maskByte <<= 1;
      }
      maskRead = true;
    } else { //x = 0
      const uint8_t* mask = avatar->getMask();
      maskByte = pgm_read_byte( &(mask[ y * bw + (x + bitmapoffset) / 8]));      
    }
    if (maskByte & 0x80) { //maskByte & 10000000b
      const uint16_t* bitmap = avatar->getBitmap();
       c = pgm_read_word_near(bitmap + ( y * avatar->width ) + x + bitmapoffset);
      *destPtr++ = c;
    } else {
      *destPtr++;
    }
  }
}

void GameScene :: renderScene() {
  renderScene(false);
}
void GameScene  :: renderScene(boolean refreshBackground) {

  Avatar *renderableAvatar[numAvatar]; //for shortlisting of avatar that will affect the screen display
  int16_t renderableMinx[numAvatar]; //for calculation of discrete drawing area
  int16_t renderableMiny[numAvatar];
  int16_t renderableMaxx[numAvatar];
  int16_t renderableMaxy[numAvatar];
  const uint16_t RENDERABLEWIDTH = SCREENWIDTH - 1;
  const uint16_t RENDERABLEHEIGHT = SCREENHEIGHT - 1;
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

    if (avatarMinx > RENDERABLEWIDTH || avatarMaxx < 0 || avatarMiny > RENDERABLEHEIGHT || avatarMaxy < 0) { //No renderable pixels (old and new location falls outside screen
      continue;
    }
    renderableAvatar[numRenderableAvatar] = avatar; //Shortlist renderable Avatar
    renderableMinx[numRenderableAvatar] = avatarMinx;
    renderableMiny[numRenderableAvatar] = avatarMiny;
    renderableMaxx[numRenderableAvatar] = avatarMaxx;
    renderableMaxy[numRenderableAvatar] = avatarMaxy;

    numRenderableAvatar++;

  }

  //Find a list of Discrete drawing area
  bool rendered[numRenderableAvatar]; //keep track of which avatar has been rendered
  for (int i = 0 ; i < numRenderableAvatar; ++i) {
    rendered[i] = false;
  }

  //Find Min / max area to draw
  //minx, miny, maxX, maxY are in Screen coordinates
  int16_t minx = SCREENWIDTH + 1;
  int16_t miny = SCREENHEIGHT + 1;
  int16_t maxx = -1;
  int16_t maxy = -1;

  Avatar* toBeRendered[numRenderableAvatar]; //working array for i loop
  int toBeRendered2RenderableMap[numRenderableAvatar];
  int toBeRenderedIndex;

  for (int i = 0; i < numRenderableAvatar; ++i) {
    toBeRenderedIndex = 0;
    minx = SCREENWIDTH + 1;
    miny = SCREENHEIGHT + 1;
    maxx = -1;
    maxy = -1;
    if (!rendered[i]) {
      toBeRendered2RenderableMap[toBeRenderedIndex] = i;
      toBeRendered[toBeRenderedIndex++] = renderableAvatar[i];
      minx = renderableMinx[i];
      miny = renderableMiny[i];
      maxx = renderableMaxx[i];
      maxy = renderableMaxy[i];
      rendered[i] = true;
    }

    for (int j = i + 1; j < numRenderableAvatar; ++j) {
      if (rendered[j]) {

        continue;
      }
      if (maxx < renderableMinx[j] || minx > renderableMaxx[j] ||
          maxy < renderableMiny[j] || miny > renderableMaxy[j]) {
        //not overlap, check next
        continue;
      } else {
        //add up the renderable area
        toBeRendered2RenderableMap[toBeRenderedIndex] = j;
        toBeRendered[toBeRenderedIndex++] = renderableAvatar[j];
        rendered[j] = true;
        minx = (minx < renderableMinx[j]) ? minx : renderableMinx[j];
        miny = (miny < renderableMiny[j]) ? miny : renderableMiny[j];
        maxx = (maxx > renderableMaxx[j]) ? maxx : renderableMaxx[j];
        maxy = (maxy > renderableMaxy[j]) ? maxy : renderableMaxy[j];

        //Render area has changed, we need to check previous layers, starting at (j + i) if they are now overlapped
        j = i; //will add one by the for loop.
      }
    }
    //Finished checking i with j, render
    //If toBeRenderedIndex > 0, do rendering of the area: minx,miny,maxx,maxy (Screen coordinates)
    if (toBeRenderedIndex > 0) {

      //cap the drawing window to the screen
      minx = (minx < 0) ? 0 : minx;
      miny = (miny < 0) ? 0 : miny;
      maxx = (maxx > RENDERABLEWIDTH) ? RENDERABLEWIDTH : maxx;
      maxy = (maxy > RENDERABLEHEIGHT) ? RENDERABLEHEIGHT : maxy;
      uint16_t renderWidth = maxx - minx + 1;
      uint16_t renderHeight = maxy - miny + 1;

#ifdef DEBUG_RENDERSCENE

      Serial.print("render minx ");
      Serial.print(minx);
      Serial.print(" miny ");
      Serial.print(miny);
      Serial.print(" maxx ");
      Serial.print(maxx);
      Serial.print(" maxy ");
      Serial.print( maxy);
      Serial.print(" renderWidth ");
      Serial.print( renderWidth);
      Serial.print(" renderHeight ");
      Serial.println( renderHeight);
#endif

      uint16_t *destPtr;
      int8_t bufIdx = 0;

      _tft->endWrite();
      _tft->startWrite();

      _tft->setAddrWindow(minx, miny, renderWidth, renderHeight);

      for (int y = 0; y < renderHeight; y++) {
        destPtr =  &renderbuf[bufIdx][0];
        if (background == NULL) {
          fillBufferWithColor(renderWidth, bgColor, destPtr);
        } else {
          drawBg2Buffer(minx, miny + y, renderWidth, destPtr);
        }

        //minx = position WRT BG bitmap
        //miny + y = y position WRT BG bitmap
        //renderWidth = how many pixel to render
        //destPtr = Buffer's pointer

        int minLayerIndex = -1;
        for (int i = 0; i < toBeRenderedIndex; ++i) {
          minLayerIndex = getNextRenderAvatar(minLayerIndex, toBeRendered2RenderableMap, toBeRenderedIndex);
          Avatar* toRender = renderableAvatar[minLayerIndex];

          //renderBuf[][0] --> minx in Screen Coordinate
          //first pixel to draw should be Avatar's x (screen Coordinte)
          int16_t pos = toRender->x - minx;   //  |0    [10 (minx)    renderx (20)  --> pos of buffer = 10

          //  renderx -10   |0[0 (minx)   pos --> 0
          pos = (pos > 0) ? pos : 0;
          destPtr =  &renderbuf[bufIdx][pos]; //Move the pointer of renderBuf so that it matches avatar's position for immediate writing to
          int16_t bitmapY = (miny + y) - toRender->y;
          if (bitmapY >= 0 &&  bitmapY < toRender->height) {
            if (toRender->isBreathingEnabled() && toRender->isBreathingDown) {
              if (bitmapY > toRender->breathAmount) {
                int16_t tempY =  (bitmapY <= toRender->height - toRender->_breathPosition) ? bitmapY - toRender->breathAmount : bitmapY;
                drawAvatar2Buffer(toRender, destPtr, tempY);
              }
            } else {
              drawAvatar2Buffer(toRender, destPtr, bitmapY);
            }

          }
          //toBeRendered[i]->savePreviousRenderPos();
          toRender->savePreviousRenderPos();
        }

        //Finished drawing to Buffer, flush buffer to TFT
        _tft->pushPixels(&renderbuf[bufIdx][0], renderWidth);

        bufIdx = 1 - bufIdx; //change our renderbuffer (0, 1)
      }

    /* Debugging */
        _tft->drawRect(minx + 1, miny + 1, renderWidth - 1 , renderHeight - 1, TFT_MAGENTA);
    /* Debugging */
    }
  }

}

int GameScene::getNextRenderAvatar(int previousMin, int toBeRendered2RenderableMap[], int toBeRenderedIndex) {
  int minLayerIndex = 999;
  for (int i = 0; i < toBeRenderedIndex; ++i) {
    if (toBeRendered2RenderableMap[i] <= previousMin) {
      continue;
    }
    minLayerIndex = min(minLayerIndex, toBeRendered2RenderableMap[i]);
  }
  return minLayerIndex;
}


void GameScene :: addSound(int soundTone, int soundDuration) {
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
void GameScene::playSound() {
  if (millis() >= soundStop) { //check if need to turn off current sound
#ifdef DEBUG
    Serial.println ("no tone");
#endif
    noTone(SPEAKER_PIN); //D0 - GPIO 16

    if (soundCount > 0) {
      int soundTone = soundToneArr[soundCountHead];
      int soundDuration = soundDurationArr[soundCountHead];
      soundStop = millis() + soundDuration;
      if (soundTone == 0) {
        noTone(SPEAKER_PIN);
      } else {
        tone(SPEAKER_PIN, soundTone, soundDuration);
      }
#ifdef DEBUG
      Serial.print("Play Tone ");
      Serial.print(soundTone);
      Serial.print(" Duration  ");
      Serial.print(soundDuration);
      Serial.print(" stop at ");

      Serial.println(soundStop);
#endif
      soundCount--;
      if (++soundCountHead >= MAX_SOUND_TONE_SIZE) {
        soundCountHead = 0;
      }
    }
  }
}
