#include "GameScene.h"
#include "SoundPlayer.h"

#define min(X, Y) (((X)<(Y))?(X):(Y))

void GameScene :: destroyScene() {
  numAvatar = 0;
  for (int i = 0; i < MAX_AVATAR; i++) {
    delete avatars[i];
    avatars[i] = NULL;
  }
}

void GameScene :: appendAvatar(Avatar * avatar) {
  if (numAvatar >= MAX_AVATAR) {
    delete avatar;
    return;
  }
  avatars[numAvatar++] = avatar;
}
    
void GameScene :: setBackground(const uint16_t* background) {
  setBackground(background, SCREENWIDTH);
}
void GameScene :: setBackground(const uint16_t* background, const uint32_t backgroundWidth) {
  this->background = background;
  this->backgroundWidth = backgroundWidth;
  this->backgroundXOffset = 0;
  this->backgroundTiled = false;
}
void GameScene :: setBackgroundTile(const uint16_t* tile, uint16_t tileWidth, uint16_t tileHeight) {
  this->background = tile;
  this->backgroundWidth = tileWidth;
  this->backgroundXOffset = 0;
  this->backgroundTiled = true;
  this->backgroundTileWidth = tileWidth;
  this->backgroundTileHeight = tileHeight;
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

void GameScene :: renderFullScreen() {
  Avatar* layerAvatars[MAX_AVATAR];
  int numLayers = 0;
  unsigned long frameTime = millis();

  for (int i = 0; i < numAvatar; ++i) {
    Avatar* avatar = avatars[i];
    if (avatar == NULL) {
      continue;
    }
    if (avatar->x + avatar->width < 0 || avatar->x >= SCREENWIDTH ||
        avatar->y + avatar->height < 0 || avatar->y >= SCREENHEIGHT) {
      continue;
    }
    if (avatar->numOfFrames > 1) {
      avatar->updateFrameIndex(frameTime);
    }
    layerAvatars[numLayers++] = avatar;
  }

  int8_t bufIdx = 0;
  _tft->endWrite();
  _tft->startWrite();
  _tft->setAddrWindow(0, 0, SCREENWIDTH, SCREENHEIGHT);

  for (uint16_t y = 0; y < SCREENHEIGHT; y++) {
    uint16_t *destPtr = &renderbuf[bufIdx][0];
    drawBg2Buffer(0, y, SCREENWIDTH, destPtr);

    for (int k = 0; k < numLayers; ++k) {
      Avatar* toRender = layerAvatars[k];
      int16_t bitmapY = y - toRender->y;
      if (bitmapY < 0 || bitmapY >= toRender->height) {
        continue;
      }
      int16_t startX = (toRender->x < 0) ? 0 : (int16_t)toRender->x;
      int16_t endX = (int16_t)(toRender->x + toRender->width);
      if (endX > SCREENWIDTH) {
        endX = SCREENWIDTH;
      }
      if (endX > startX) {
        destPtr = &renderbuf[bufIdx][startX];
        drawAvatar2Buffer(toRender, destPtr, bitmapY, (uint16_t)(endX - startX));
      }
    }

    _tft->pushPixels(&renderbuf[bufIdx][0], SCREENWIDTH);
    bufIdx = 1 - bufIdx;
  }

  _tft->endWrite();

  for (int i = 0; i < numLayers; ++i) {
    layerAvatars[i]->forceRedraw = false;
    layerAvatars[i]->savePreviousRenderPos();
  }
}

void GameScene::drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr) {
  if (backgroundTiled && backgroundTileWidth > 0 && backgroundTileHeight > 0) {
    uint16_t tileY = y % backgroundTileHeight;
    uint32_t rowBase = (uint32_t)tileY * backgroundTileWidth;
    for (uint32_t i = 0; i < width; ++i) {
      uint16_t tileX = (uint16_t)((x + i) % backgroundTileWidth);
      *destPtr++ = pgm_read_word_far(background + rowBase + tileX);
    }
    return;
  }

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
void GameScene::drawAvatar2Buffer(Avatar *avatar, uint16_t* destPtr, uint16_t y, uint16_t maxWidth, uint16_t srcStartX) {
  if (maxWidth == 0) {
    return;
  }

  if (avatar->usesSheetSource()) {
    uint16_t renderwidth = maxWidth;
    uint16_t bitmapoffset = srcStartX;
    uint16_t sheetBw = (avatar->sheetWidth + 7) / 8;

    if (srcStartX == 0 && avatar->x < 0) {
      renderwidth = (avatar->width + avatar->x);
      if (renderwidth > maxWidth) {
        renderwidth = maxWidth;
      }
      if (renderwidth == 0) {
        return;
      }
      bitmapoffset = (uint16_t)abs(avatar->x);
    }

    for (uint16_t x = 0; x < renderwidth; x++) {
      uint16_t localX = x + bitmapoffset;
      uint16_t srcCol = avatar->flipX ? (avatar->width - 1 - localX) : localX;
      uint16_t sheetX = avatar->sheetSrcX + srcCol;
      uint16_t sheetY = avatar->sheetSrcY + y;
      uint8_t maskByte = pgm_read_byte(avatar->sheetMask + (uint32_t)sheetY * sheetBw + (sheetX >> 3));
      if (maskByte & (0x80 >> (sheetX & 7))) {
        uint16_t c = pgm_read_word_far(avatar->sheetBitmap + (uint32_t)sheetY * avatar->sheetWidth + sheetX);
        *destPtr++ = c;
      } else {
        destPtr++;
      }
    }
    return;
  }

  uint16_t renderwidth = maxWidth;
  int16_t bw = (avatar->width + 7) / 8;
  uint16_t bitmapoffset = srcStartX;

  boolean maskRead = true;
  uint16_t maskoffset = 0;
  uint8_t maskByte = 0;

  if (srcStartX == 0 && avatar->x < 0) {
    renderwidth = (avatar->width + avatar->x);
    if (renderwidth > maxWidth) {
      renderwidth = maxWidth;
    }
    if (renderwidth == 0) {
      return;
    }

    maskRead = false;
    bitmapoffset = (uint16_t)abs(avatar->x);
    const uint8_t* mask = avatar->getMask();
    maskByte = pgm_read_byte(&(mask[bw * y + bitmapoffset / 8]));
    maskoffset = (uint16_t)abs(avatar->x) % 8;
    maskByte <<= maskoffset;
  } else if (srcStartX > 0) {
    maskRead = false;
    const uint8_t* mask = avatar->getMask();
    maskByte = pgm_read_byte(&(mask[bw * y + srcStartX / 8]));
    maskoffset = srcStartX % 8;
    maskByte <<= maskoffset;
  }

  if (avatar->flipX) {
    // Mirrored: read the source column from the opposite edge. The fast
    // incremental mask walk below assumes left-to-right order, so sample the
    // mask bit directly here instead.
    const uint8_t* mask = avatar->getMask();
    const uint16_t* bitmap = avatar->getBitmap();
    for (uint16_t x = 0; x < renderwidth; x++) {
      uint16_t srcCol = avatar->width - 1 - (x + bitmapoffset);
      uint8_t mb = pgm_read_byte(&(mask[y * bw + (srcCol >> 3)]));
      if (mb & (0x80 >> (srcCol & 7))) {
        *destPtr++ = pgm_read_word_near(bitmap + (y * avatar->width) + srcCol);
      } else {
        destPtr++;
      }
    }
    return;
  }

  for (uint16_t x = 0; x < renderwidth; x++) {
    if ((x + bitmapoffset) & 7) {
      if (maskRead) {
        maskByte <<= 1;
      }
      maskRead = true;
    } else {
      const uint8_t* mask = avatar->getMask();
      maskByte = pgm_read_byte(&(mask[y * bw + (x + bitmapoffset) / 8]));
    }
    if (maskByte & 0x80) {
      const uint16_t* bitmap = avatar->getBitmap();
      uint16_t c = pgm_read_word_near(bitmap + (y * avatar->width) + x + bitmapoffset);
      *destPtr++ = c;
    } else {
      destPtr++;
    }
  }
}

void GameScene :: renderScene() {
  renderScene(false);
}
void GameScene  :: renderScene(boolean refreshBackground) {
  Avatar *renderableAvatar[numAvatar]; //for shortlisting of avatar that will affect the screen display
  bool renderableFullRedraw[numAvatar];
  int16_t renderableMinx[numAvatar]; //for calculation of discrete drawing area
  int16_t renderableMiny[numAvatar];
  int16_t renderableMaxx[numAvatar];
  int16_t renderableMaxy[numAvatar];
  const uint16_t RENDERABLEWIDTH = SCREENWIDTH - 1;
  const uint16_t RENDERABLEHEIGHT = SCREENHEIGHT - 1;
  int numRenderableAvatar = 0;
  bool anyPositionChange = false;
  int16_t unionDirtyMinx = SCREENWIDTH;
  int16_t unionDirtyMiny = SCREENHEIGHT;
  int16_t unionDirtyMaxx = -1;
  int16_t unionDirtyMaxy = -1;

  // Pass 1: find movement and the combined dirty area from position changes.
  for (int i = 0; i < numAvatar; ++i) {
    Avatar *avatar = avatars[i];
    float oldX = avatar->previousRenderedX;
    float oldY = avatar->previousRenderedY;
    float newX = avatar->x;
    float newY = avatar->y;

    if (oldX == newX && oldY == newY) {
      continue;
    }

    anyPositionChange = true;
    int16_t dirtyMinx = (int16_t)((oldX < newX) ? oldX : newX);
    int16_t dirtyMiny = (int16_t)((oldY < newY) ? oldY : newY);
    int16_t dirtyMaxx = (int16_t)(((oldX < newX) ? newX : oldX) + avatar->width - 1);
    int16_t dirtyMaxy = (int16_t)(((oldY < newY) ? newY : oldY) + avatar->height - 1);

    if (dirtyMinx < unionDirtyMinx) {
      unionDirtyMinx = dirtyMinx;
    }
    if (dirtyMiny < unionDirtyMiny) {
      unionDirtyMiny = dirtyMiny;
    }
    if (dirtyMaxx > unionDirtyMaxx) {
      unionDirtyMaxx = dirtyMaxx;
    }
    if (dirtyMaxy > unionDirtyMaxy) {
      unionDirtyMaxy = dirtyMaxy;
    }
  }

  //Step1: Find all Avatars that need redraw (moved, animated, or under a mover's path)
  unsigned long frameTime = millis();
  for (int i  = 0; i < numAvatar;  ++i) {
    Avatar *avatar = avatars[i];
    float oldX = avatar->previousRenderedX;
    float oldY = avatar->previousRenderedY;
    float newX = avatar->x;
    float newY = avatar->y;

    byte frameBefore = avatar->currentFrame;
    if (avatar->numOfFrames > 1) {
      avatar->updateFrameIndex(frameTime);
    }
    // forceRedraw (from requestRedraw()) counts as a content change: repaint the
    // avatar's current box, and if it also moved the old+new union below still
    // covers the full swept area so no trailing pixels are left behind.
    bool frameChanged = (avatar->numOfFrames > 1 && avatar->currentFrame != frameBefore) ||
                        avatar->forceRedraw;
    bool positionChanged = (oldX != newX || oldY != newY);
    int16_t avatarMinx;
    int16_t avatarMiny;
    int16_t avatarMaxx;
    int16_t avatarMaxy;

    if (positionChanged || frameChanged) {
      renderableFullRedraw[numRenderableAvatar] = true;
      avatarMinx = (int16_t)((oldX < newX) ? oldX : newX);
      avatarMiny = (int16_t)((oldY < newY) ? oldY : newY);
      avatarMaxx = (int16_t)(((oldX < newX) ? newX : oldX) + avatar->width - 1);
      avatarMaxy = (int16_t)(((oldY < newY) ? newY : oldY) + avatar->height - 1);
    } else if (anyPositionChange) {
      renderableFullRedraw[numRenderableAvatar] = false;
      avatarMinx = (int16_t)newX;
      avatarMiny = (int16_t)newY;
      avatarMaxx = (int16_t)(newX + avatar->width - 1);
      avatarMaxy = (int16_t)(newY + avatar->height - 1);

      if (avatarMaxx < unionDirtyMinx || avatarMinx > unionDirtyMaxx ||
          avatarMaxy < unionDirtyMiny || avatarMiny > unionDirtyMaxy) {
        continue;
      }

      if (avatarMinx < unionDirtyMinx) {
        avatarMinx = unionDirtyMinx;
      }
      if (avatarMiny < unionDirtyMiny) {
        avatarMiny = unionDirtyMiny;
      }
      if (avatarMaxx > unionDirtyMaxx) {
        avatarMaxx = unionDirtyMaxx;
      }
      if (avatarMaxy > unionDirtyMaxy) {
        avatarMaxy = unionDirtyMaxy;
      }
      avatar->renderTainted = true;
    } else if (avatar->renderTainted) {
      // One more full repair pass after the mover has left.
      renderableFullRedraw[numRenderableAvatar] = true;
      avatarMinx = (int16_t)newX;
      avatarMiny = (int16_t)newY;
      avatarMaxx = (int16_t)(newX + avatar->width - 1);
      avatarMaxy = (int16_t)(newY + avatar->height - 1);
    } else {
      continue;
    }

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
  } else if (numRenderableAvatar > 0) {
     //Step 2: Find a list of Discrete drawing area
    bool rendered[numRenderableAvatar]; //keep track of which avatar has been rendered
    for (int i = 0 ; i < numRenderableAvatar; ++i) {
      rendered[i] = false;
    }
    //Find Min / max area to draw. minx, miny, maxX, maxY are in Screen coordinates
    int16_t minx, miny, maxx, maxy;

    _tft->endWrite();
    _tft->startWrite();

    //Get Discrete render area with Avatar overlapping inside
    for (int i = 0; i < numRenderableAvatar; ++i) {
      //If the Avatar's renderable area has been checked and overlaps previous avatar, we can skip checking this avatar
      if (rendered[i]) {
        continue;
      }
      rendered[i] = true;

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

          Avatar* clusterLayers[MAX_AVATAR];
          int numClusterLayers = 0;
          Avatar* clusterShortlist[MAX_AVATAR];
          bool clusterFullRedraw[MAX_AVATAR];
          int numClusterShortlist = 0;
          for (int k = 0; k < numAvatar; ++k) {
            Avatar* toRender = avatars[k];
            if (toRender == NULL) {
              continue;
            }
            int16_t avatarMinX = (int16_t)toRender->x;
            int16_t avatarMinY = (int16_t)toRender->y;
            int16_t avatarMaxX = (int16_t)(toRender->x + toRender->width - 1);
            int16_t avatarMaxY = (int16_t)(toRender->y + toRender->height - 1);
            if (avatarMaxX < minx || avatarMinX > maxx ||
                avatarMaxY < miny || avatarMinY > maxy) {
              continue;
            }
            clusterLayers[numClusterLayers++] = toRender;
          }
          for (int k = 0; k < numRenderableAvatar; ++k) {
            if (renderableMaxx[k] < minx || renderableMinx[k] > maxx ||
                renderableMaxy[k] < miny || renderableMiny[k] > maxy) {
              continue;
            }
            clusterShortlist[numClusterShortlist] = renderableAvatar[k];
            clusterFullRedraw[numClusterShortlist] = renderableFullRedraw[k];
            numClusterShortlist++;
          }

          int16_t spanStarts[16];
          int16_t spanEnds[16];
          uint16_t *destPtr;
          int8_t bufIdx = 0;

          for (int y = 0; y < renderHeight; y++) {
            int16_t screenY = miny + y;
            int numSpans = collectRowRedrawSpans(screenY, minx, maxx,
                                                 unionDirtyMinx, unionDirtyMaxx,
                                                 unionDirtyMiny, unionDirtyMaxy,
                                                 clusterShortlist, clusterFullRedraw,
                                                 numClusterShortlist,
                                                 spanStarts, spanEnds, 16);
            if (numSpans == 0) {
              continue;
            }

            for (int s = 0; s < numSpans; ++s) {
              int16_t spanMinx = spanStarts[s];
              int16_t spanMaxx = spanEnds[s];
              uint16_t spanWidth = (uint16_t)(spanMaxx - spanMinx + 1);

              _tft->setAddrWindow(spanMinx, screenY, spanWidth, 1);
              destPtr = &renderbuf[bufIdx][0];
              drawBg2Buffer((uint16_t)spanMinx, (uint16_t)screenY, spanWidth, destPtr);

              for (int k = 0; k < numClusterLayers; ++k) {
                Avatar* toRender = clusterLayers[k];
                int16_t avatarMinX = (int16_t)toRender->x;
                int16_t avatarMaxX = (int16_t)(toRender->x + toRender->width - 1);
                if (avatarMaxX < spanMinx || avatarMinX > spanMaxx) {
                  continue;
                }

                int16_t overlapStart = spanMinx > avatarMinX ? spanMinx : avatarMinX;
                int16_t overlapEnd = spanMaxx < avatarMaxX ? spanMaxx : avatarMaxX;
                uint16_t bufPos = (uint16_t)(overlapStart - spanMinx);
                uint16_t srcStartX = (uint16_t)(overlapStart - avatarMinX);
                uint16_t drawWidth = (uint16_t)(overlapEnd - overlapStart + 1);
                destPtr = &renderbuf[bufIdx][bufPos];
                int16_t bitmapY = screenY - toRender->y;
                if (bitmapY >= 0 && bitmapY < toRender->height) {
                  drawAvatar2Buffer(toRender, destPtr, (uint16_t)bitmapY, drawWidth, srcStartX);
                }
              }

              _tft->pushPixels(&renderbuf[bufIdx][0], min(spanWidth, SCREENWIDTH));
              bufIdx = 1 - bufIdx;
            }
          }
    
        if (isDebugEnabled) {
            _tft->drawRect(minx + 1, miny + 1, renderWidth - 1 , renderHeight - 1, TFT_MAGENTA);
        }
          
        
      
    } // ends for i loop

    for (int i = 0; i < numRenderableAvatar; ++i) {
      if (renderableFullRedraw[i]) {
        renderableAvatar[i]->renderTainted = false;
      }
      renderableAvatar[i]->forceRedraw = false;
      renderableAvatar[i]->savePreviousRenderPos();
    }

    _tft->endWrite();
    
  }
  disableDebug(); //always disable after rendering
}

int GameScene::collectRowRedrawSpans(int16_t screenY, int16_t clipMinx, int16_t clipMaxx,
                                     int16_t unionDirtyMinx, int16_t unionDirtyMaxx,
                                     int16_t unionDirtyMiny, int16_t unionDirtyMaxy,
                                     Avatar** shortlist, const bool* fullRedraw, int shortlistCount,
                                     int16_t* spanStarts, int16_t* spanEnds, int maxSpans) {
  int numSpans = 0;
  for (int i = 0; i < shortlistCount; ++i) {
    Avatar* avatar = shortlist[i];
    float oldX = avatar->previousRenderedX;
    float oldY = avatar->previousRenderedY;
    float newX = avatar->x;
    float newY = avatar->y;
    bool positionChanged = (oldX != newX || oldY != newY);

    if (fullRedraw[i]) {
      int16_t curMinY = (int16_t)newY;
      int16_t curMaxY = (int16_t)(newY + avatar->height - 1);
      if (screenY >= curMinY && screenY <= curMaxY) {
        int16_t spanMinx = (int16_t)newX;
        int16_t spanMaxx = (int16_t)(newX + avatar->width - 1);
        if (numSpans < maxSpans) {
          if (spanMinx < clipMinx) spanMinx = clipMinx;
          if (spanMaxx > clipMaxx) spanMaxx = clipMaxx;
          if (spanMinx <= spanMaxx) {
            spanStarts[numSpans] = spanMinx;
            spanEnds[numSpans] = spanMaxx;
            numSpans++;
          }
        }
      }

      if (positionChanged) {
        int16_t dirtyMinY = (int16_t)((oldY < newY) ? oldY : newY);
        int16_t dirtyMaxY = (int16_t)(((oldY < newY) ? newY : oldY) + avatar->height - 1);
        if (screenY >= dirtyMinY && screenY <= dirtyMaxY) {
          int16_t spanMinx = (int16_t)((oldX < newX) ? oldX : newX);
          int16_t spanMaxx = (int16_t)(((oldX < newX) ? newX : oldX) + avatar->width - 1);
          if (numSpans < maxSpans) {
            if (spanMinx < clipMinx) spanMinx = clipMinx;
            if (spanMaxx > clipMaxx) spanMaxx = clipMaxx;
            if (spanMinx <= spanMaxx) {
              spanStarts[numSpans] = spanMinx;
              spanEnds[numSpans] = spanMaxx;
              numSpans++;
            }
          }
        }
      }
    } else {
      int16_t curMinY = (int16_t)newY;
      int16_t curMaxY = (int16_t)(newY + avatar->height - 1);
      if (screenY < curMinY || screenY > curMaxY) {
        continue;
      }
      if (screenY < unionDirtyMiny || screenY > unionDirtyMaxy) {
        continue;
      }

      int16_t spanMinx = (int16_t)newX;
      int16_t spanMaxx = (int16_t)(newX + avatar->width - 1);
      if (spanMinx < unionDirtyMinx) {
        spanMinx = unionDirtyMinx;
      }
      if (spanMaxx > unionDirtyMaxx) {
        spanMaxx = unionDirtyMaxx;
      }
      if (numSpans < maxSpans) {
        if (spanMinx < clipMinx) {
          spanMinx = clipMinx;
        }
        if (spanMaxx > clipMaxx) {
          spanMaxx = clipMaxx;
        }
        if (spanMinx <= spanMaxx) {
          spanStarts[numSpans] = spanMinx;
          spanEnds[numSpans] = spanMaxx;
          numSpans++;
        }
      }
    }
  }

  if (numSpans <= 1) {
    return numSpans;
  }

  for (int i = 1; i < numSpans; ++i) {
    int16_t start = spanStarts[i];
    int16_t end = spanEnds[i];
    int j = i;
    while (j > 0 && spanStarts[j - 1] > start) {
      spanStarts[j] = spanStarts[j - 1];
      spanEnds[j] = spanEnds[j - 1];
      --j;
    }
    spanStarts[j] = start;
    spanEnds[j] = end;
  }

  int merged = 0;
  for (int i = 0; i < numSpans; ++i) {
    if (merged == 0 || spanStarts[i] > spanEnds[merged - 1] + 1) {
      spanStarts[merged] = spanStarts[i];
      spanEnds[merged] = spanEnds[i];
      merged++;
    } else if (spanEnds[i] > spanEnds[merged - 1]) {
      spanEnds[merged - 1] = spanEnds[i];
    }
  }
  return merged;
}

void GameScene :: addSound(int soundTone, int soundDuration) {
  SoundPlayer::enqueue(soundTone, soundDuration);
}
