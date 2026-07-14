#include "GameScene.h"

#define min(X, Y) (((X)<(Y))?(X):(Y))
#define DEBUG

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
//      if (backgroundWidth != 512) {
//        isDebugEnabled = true;
//        backgroundWidth = 512;
//      }
//     return  backgroundXOffset + (backgroundWidth * y) + x;
      uint32_t widthTimesHeightOffset = (uint32_t)backgroundWidth * (uint32_t) y;
      uint32_t computed = (uint32_t)backgroundXOffset + widthTimesHeightOffset + (uint32_t)x;
      if (isDebugEnabled && backgroundWidth != 512) {
        Serial.printf("\n[DEBUG] getBackgroundMemoryPosition(uint16_t %u, uint16_t %u) \n", x, y);
        Serial.printf("backgroundWidth: %u widthTimesHeightOffset: %u backgrundXOffset: %u computed: %u \n", backgroundWidth, widthTimesHeightOffset, backgroundXOffset, computed);
      }
      return  computed;
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
          Serial.printf("background: %u ", background);
          Serial.printf("%X ", background);
          //Serial.println(*((uint16_t*)background));
          Serial.printf("backgroundMemoryPosition: %d \t\t", pos);
        }
        
        *destPtr = pgm_read_word_far(pos + background);
        if (isDebugEnabled) {
          Serial.printf(" %hu ", (*destPtr )); //Print the content of the pointer, which is the data retrieved by pgm_read_word_far
          //Serial.printf(" %hu ", (&destPtr));  //each address should be increased by the pointer sized
        }
        destPtr = destPtr + 1;
      } else {
        *destPtr++ = pgm_read_word_near(pos + background);
      }
    }
    isDebugEnabled = false;
    _tft->pushPixels(&renderbuf[bufIdx][0], SCREENWIDTH);
    bufIdx = 1 - bufIdx; //alternate our renderbuffer (0, 1)
  }
}

void GameScene::drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr) {
  uint16_t c;
  uint32_t pos = getBackgoundMemoryPosition(x, y);
  if (isDebugEnabled) {
    Serial.printf("\n[DEBUG] drawBg2Buffer(uint16_t %d, uint16_t %d, uint16_t %d, uint16_t)\n", x, y, width);
    Serial.printf("pos: %d\n", pos);
    Serial.printf("background: %u\n", background);
  }
  for (uint32_t i = 0; i < width; ++i) {
    //c = pgm_read_word_near(background + pos + i);  
    c = pgm_read_word_far(background + pos + i);  
    *destPtr++ = c;
    if (isDebugEnabled) {
       Serial.printf(" %hu ", c); //Print the value retrived from pgm_read_word_far
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
  uint8_t maskByte = 0; //variable to hold the Byte read from mask;
  if (avatar->x < 0) {
    
    maskRead  = false;
    
    const uint8_t* mask = avatar->getMask();

    //For every 8 pixels the avatar X is negative, the mask need to be shifted 1 byte to the left
    bitmapoffset = abs(avatar->x);

    // (bw * y) -->  Mask Width * row
    // bitmapoffset --> the number of pixels the avater is to the Left of the screen.
    // divide by 8, since each byte in the mask corresponds to 8 pixels
    // Not that the divided value is "too small" i.e.  when avatar is 10 pixel Left of the screen
    // we really wish to shift the mask by 1Byte and 2 bit. divide by 8 will produce 1 (1 byte)
    // then get the 2 bit from % 8
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
    if (maskByte & 0x80) { //maskByte & 0b10000000
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

  //Step1: Find all Avatars that Was & Will be on the Screen
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
    avatar->savePreviousRenderPos();

    //find a shortlist of avatar to render. Only render those that falls on the screen either in previous screen or current screen
    if (avatarMinx > RENDERABLEWIDTH || avatarMaxx < 0 || avatarMiny > RENDERABLEHEIGHT || avatarMaxy < 0) { 
      continue;
    }
    renderableAvatar[numRenderableAvatar] = avatar; //Shortlist renderable Avatar
    renderableMinx[numRenderableAvatar] = (avatarMinx < 0) ? 0 : avatarMinx;
    renderableMiny[numRenderableAvatar] = (avatarMiny < 0) ? 0 : avatarMiny;
    renderableMaxx[numRenderableAvatar] = (avatarMaxx >= SCREENWIDTH) ? SCREENWIDTH - 1  : avatarMaxx;
    renderableMaxy[numRenderableAvatar] = (avatarMaxy >= SCREENHEIGHT) ? SCREENHEIGHT - 1  : avatarMaxy;
    
    //Serial.printf("\nAvatar #%d renderableMin/Max/x/y:%d %d %d %d\n", i, renderableMinx[numRenderableAvatar], renderableMiny[numRenderableAvatar], renderableMaxx[numRenderableAvatar] , renderableMaxy[numRenderableAvatar]);
    numRenderableAvatar++;
  }
  //Serial.printf("Total numRenderableAvatar %d\n\n", numRenderableAvatar);

  if (refreshBackground) {
    Serial.println("RefreshBackground implementation not completed");
  } else {
     //Step 2: Find a list of Discrete drawing area
    bool rendered[numRenderableAvatar]; //keep track of which avatar has been rendered
    for (int i = 0 ; i < numRenderableAvatar; ++i) {
      rendered[i] = false;
    }
    //Find Min / max area to draw. minx, miny, maxX, maxY are in Screen coordinates
    int16_t minx, miny, maxx, maxy;
    
    Avatar* toBeRendered[numRenderableAvatar]; 
    int toBeRendered2RenderableMap[numRenderableAvatar];
    int toBeRenderedIndex;
    
    //keep track of the bound of the Avatar Overlapping render area
    //This is used for refreshing background that are not covered by Avatarsß
    uint16_t renderAreaMinX[numRenderableAvatar]; 
    uint16_t renderAreaMinY[numRenderableAvatar];
    uint16_t renderAreaMaxX[numRenderableAvatar];
    uint16_t renderAreaMaxY[numRenderableAvatar];
    uint16_t renderAreaIndex = 0;

    //Get Discrete render area with Avatar overlapping inside
    for (int i = 0; i < numRenderableAvatar; ++i) {
      toBeRenderedIndex = 0;
      //If the Avatar's renderable area has been checked and overlaps previous avatar, we can skip checking this avatar
      if (rendered[i]) {
        continue;
      }
      rendered[i] = true;
      toBeRendered[toBeRenderedIndex++] = renderableAvatar[i];
      
  
      minx = renderableMinx[i];
      miny = renderableMiny[i];
      maxx = renderableMaxx[i];
      maxy = renderableMaxy[i];
  
     
      //Check for overlapping Avatar, and then find the aggregated renderarea
      for (int j = i + 1; j < numRenderableAvatar; ++j) {
        if (rendered[j]) {
          continue;
        }
        //Only if they overlap, then update the renderableMin/Max[x/y]
        if ( ( (renderableMinx[i] < renderableMinx[j] && renderableMaxx[i] > renderableMinx[j]) || (renderableMinx[j] < renderableMinx[i] && renderableMaxx[j] > renderableMinx[i]) ) //X overlap
             &&
             ( (renderableMiny[i] < renderableMiny[j] && renderableMaxy[i] > renderableMiny[j]) || (renderableMiny[j] < renderableMiny[i] && renderableMaxy[j] > renderableMiny[i]) ) //y overlap
           ) {
          minx = (minx < renderableMinx[j]) ? minx : renderableMinx[j];
          miny = (miny < renderableMiny[j]) ? miny : renderableMiny[j];
          maxx = (maxx > renderableMaxx[j]) ? maxx : renderableMaxx[j];
          maxy = (maxy > renderableMaxy[j]) ? maxy : renderableMaxy[j];
          rendered[j] = true;
          toBeRendered[toBeRenderedIndex++] = renderableAvatar[i];
          //Serial.printf("Overlap #%d, %d \n", i, j);
          
          /*
           * .  ......   ......
           *    | A  | . | B  |
           *    ---.------.....
           *       | .C .|
           *  When A and B are not overlapped, but they are overlapped by C, then all of A, B, C should be in the same render area
           *  now the render-area has changed, we need to check previous "un-rendered" avatar, whether they will now overlap
           */
           j = i + 1; //go back and check previous "non-overlap" & non-rendered avatar.
         }
      }//we have checked i-th avatar with all other avatars
      
      
    
      //Render the Avatar

      
          //cap the drawing window to the screen
          minx = (minx < 0) ? 0 : minx;
          miny = (miny < 0) ? 0 : miny;
          maxx = (maxx > RENDERABLEWIDTH) ? RENDERABLEWIDTH : maxx;
          maxy = (maxy > RENDERABLEHEIGHT) ? RENDERABLEHEIGHT : maxy;
          uint16_t renderWidth = maxx - minx + 1;
          uint16_t renderHeight = maxy - miny + 1;
//      
          uint16_t *destPtr;
          int8_t bufIdx = 0;
//      
          _tft->endWrite();
          _tft->startWrite();
    
          _tft->setAddrWindow(minx, miny, renderWidth, renderHeight);
//      
          for (int y = 0; y < renderHeight; y++) {
            destPtr =  &renderbuf[bufIdx][0];
//              if (background == NULL) {
//                fillBufferWithColor(renderWidth, bgColor, destPtr);
//              } else {
            drawBg2Buffer(minx, miny + y, renderWidth, destPtr);
//              }
//      
//              //minx = position WRT BG bitmap
//              //miny + y = y position WRT BG bitmap
//              //renderWidth = how many pixel to render
//              //destPtr = Buffer's pointer
//      
            for (int k = 0; k < numRenderableAvatar; ++k) {
              Avatar* toRender = renderableAvatar[k];

              if (renderableMaxx[k] < minx || renderableMinx[k] > maxx ||
                  renderableMaxy[k] < miny || renderableMiny[k] > maxy) {
                continue;
              }
    
              //renderBuf[][0] --> minx in Screen Coordinate
              //first pixel to draw should be Avatar's x (screen Coordinte)
              int16_t pos = toRender->x - minx;   //  |0    [10 (minx)    renderx (20)  --> pos of buffer = 10
    
              //  renderx -10   |0[0 (minx)   pos --> 0
              pos = (pos > 0) ? pos : 0;
              destPtr =  &renderbuf[bufIdx][pos]; //Move the pointer of renderBuf so that it matches avatar's position for immediate writing to
              int16_t bitmapY = (miny + y) - toRender->y;
              if (bitmapY >= 0 &&  bitmapY < toRender->height) {
//                  if (toRender->isBreathingEnabled() && toRender->isBreathingDown) {
//                    if (bitmapY > toRender->breathAmount) {
//                      int16_t tempY =  (bitmapY <= toRender->height - toRender->_breathPosition) ? bitmapY - toRender->breathAmount : bitmapY;
//                      drawAvatar2Buffer(toRender, destPtr, tempY);
//                    }
//                  } else {
                  drawAvatar2Buffer(toRender, destPtr, bitmapY);
//                  }
              }
            }
//      
//              //Finished drawing to Buffer, flush buffer to TFT
            _tft->pushPixels(&renderbuf[bufIdx][0], min(renderWidth, SCREENWIDTH) );
//
            bufIdx = 1 - bufIdx; //change our renderbuffer (0, 1)
          }
    
        /* Debugging */
            _tft->drawRect(minx + 1, miny + 1, renderWidth - 1 , renderHeight - 1, TFT_MAGENTA);
        /* Debugging */
          
        
      
    } // ends for i loop
    
  }
  disableDebug(); //always disable after rendering
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
    if (playingSound) {
      noTone(SPEAKER_PIN); //D0 - GPIO 16
      playingSound = false;
    }

    if (soundCount > 0) {
      int soundTone = soundToneArr[soundCountHead];
      int soundDuration = soundDurationArr[soundCountHead];
      soundStop = millis() + soundDuration;
      if (soundTone == 0) {
        if (playingSound) {
          noTone(SPEAKER_PIN);
          playingSound = false;
        }
      } else {
        tone(SPEAKER_PIN, soundTone, soundDuration);
        playingSound = true;
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
