#include "GameScene.h"

//#define DEBUG_RENDERSCENE
//#define DEBUG_DRAWAVATAR
void GameScene  :: renderScene() {
  //_tft->fillScreen(debugColor);
  
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
#ifdef DEBUG_RENDERSCENE
  Serial.print("renderableAvatar # ");
  Serial.print(numRenderableAvatar);
  Serial.print(" minx ");
  Serial.print(renderableMinx[numRenderableAvatar]);
  Serial.print(" miny ");
  Serial.print(renderableMiny[numRenderableAvatar]);
  Serial.print(" maxxx ");
  Serial.print(renderableMaxx[numRenderableAvatar]);
  Serial.print(" maxy ");
  Serial.println(renderableMaxy[numRenderableAvatar]);
#endif
    
    numRenderableAvatar++;
  }
#ifdef DEBUG_RENDERSCENE
  Serial.print("Search for Discrete drawing area; total # of renderable:");
  Serial.println(numRenderableAvatar);
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
  int toBeRendered2RenderableMap[numRenderableAvatar];
  int toBeRenderedIndex;

  for (int i = 0; i < numRenderableAvatar; ++i) {
    toBeRenderedIndex = 0;
    minx = 241;
    miny = 321;
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
#ifdef DEBUG_RENDERSCENE
          Serial.print("Renderable # ");
          Serial.print( j );
          Serial.print(" not overlap, check next");
          Serial.println(numRenderableAvatar);
#endif
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
#ifdef DEBUG_RENDERSCENE
        Serial.print("drawBg2Buffer, y");
        Serial.println(miny + y);
#endif
        if(background == NULL){
          fillBufferWithColor(renderWidth, bgColor, destPtr);
        }else{
          drawBg2Buffer(minx, miny + y, renderWidth, destPtr);
        }
          
        //minx = position WRT BG bitmap
        //miny + y = y position WRT BG bitmap
        //renderWidth = how many pixel to render
        //destPtr = Buffer's pointer

        int minLayerIndex = -1;
        for (int i = 0; i < toBeRenderedIndex; ++i){
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
#ifdef DEBUG_RENDERSCENE
            Serial.print("         drawAvatar,#[");
            Serial.print(i);
            Serial.print("] ");
            Serial.print("bitmapY");
            Serial.println(bitmapY);
#endif
//            drawAvatar2Buffer(toBeRendered[i], destPtr, bitmapY);
            if(toRender->isBreathingEnabled() && toRender->isBreathingDown){
              if(bitmapY > toRender->breathAmount){
                int16_t tempY =  (bitmapY <= toRender->height - toRender->_breathPosition) ? bitmapY - toRender->breathAmount : bitmapY;
                drawAvatar2Buffer(toRender, destPtr, tempY);
              }
            }else{
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
      
      //  _tft->drawRect(minx + 1, miny + 1, renderWidth - 1 , renderHeight - 1, TFT_MAGENTA);
    }
  }
#ifdef DEBUG_RENDERSCENE
  delay(200); //so that Serial monitor is not jammed
#endif
}

int GameScene::getNextRenderAvatar(int previousMin, int toBeRendered2RenderableMap[], int toBeRenderedIndex){
   int minLayerIndex = 999;
   for (int i = 0; i < toBeRenderedIndex; ++i){
        if(toBeRendered2RenderableMap[i] <= previousMin){
          continue;
        }
        minLayerIndex = min(minLayerIndex, toBeRendered2RenderableMap[i]);  
   }
   return minLayerIndex;  
}
void GameScene::drawBg2Buffer(uint16_t x, uint16_t y, uint16_t width, uint16_t *destPtr) {
  uint16_t c;
  for (int i = 0; i < width; ++i) {
    c = pgm_read_word_near(background + (y * SCREENWIDTH) + x + i);
    *destPtr++ = c;
  }
}
void GameScene::fillBufferWithColor(uint16_t width, uint16_t color, uint16_t * destPtr){
  for (int i = 0; i < width; ++i) {
    *destPtr++ = color;
  }
}

void GameScene::drawAvatar2Buffer(Avatar *avatar, uint16_t* destPtr, uint16_t y) {
#ifdef DEBUG_DRAWAVATAR
  Serial.println("drawAvatar2Buffer");
#endif
  int renderwidth = (avatar->x < 0) ? (avatar->width + avatar->x) : avatar->width; //note the sign of x is -'ve in first condition.
  uint16_t c;
  int16_t bw = (avatar->width + 7) / 8; //number of bytes for each row of mask


  boolean prefetchMask = false;
  boolean maskRead = true;
  uint16_t maskoffset = 0;
  uint16_t bitmapoffset = 0;
  uint16_t nextMaskOffset = 0;
  uint8_t maskByte = 0; //variable to hold the Byte read from mask;
  if (avatar->x < 0) {
    prefetchMask = true;
    maskoffset = abs(avatar->x) % 8; //for image falls less than 0 in x axis, ad
    bitmapoffset = abs(avatar->x);
    nextMaskOffset = 8 - bitmapoffset;
    maskRead  = false;
#ifdef DEBUG_DRAWAVATAR
    Serial.print("PrefetchMask = true ");
    Serial.print(" renderwidth ");
    Serial.print(renderwidth);
    Serial.print(" y " );
    Serial.print( y);
    Serial.print("bitmapoffset ");
    Serial.print(bitmapoffset);
    Serial.print(" maskoffset ");
    Serial.println(maskoffset);

    Serial.print(" Prefetch Mask at Address");
    Serial.print (bw * y +  bitmapoffset / 8);

    Serial.print(" << # bits");
    Serial.println(maskoffset);
#endif
    maskByte = pgm_read_byte(&(avatar->mask[ bw * y +  bitmapoffset / 8  ]));
    maskByte <<= maskoffset;
  }

  for (int x = 0 ; x < renderwidth; x++) {
    if ((x + bitmapoffset) & 7) {   //for x = 1 ... 7, (x & 7) is true
      if (maskRead) {
        maskByte <<= 1;
#ifdef DEBUG_DRAWAVATAR_1
        Serial.print(" << ");
#endif
      }
      maskRead = true;
    } else { //x = 0
      maskByte = pgm_read_byte( &(avatar->mask[ y * bw + (x + bitmapoffset) / 8]));
#ifdef DEBUG_DRAWAVATAR_1
      Serial.print(" Read Mask at Address");
      Serial.println(y * bw + (x + bitmapoffset) / 8);
#endif
    }
    if (maskByte & 0x80) { //maskByte & 10000000b
      c = pgm_read_word_near(avatar->bitmap + ( y * avatar->width ) + x + bitmapoffset);
      *destPtr++ = c;
    } else {
      *destPtr++;
    }
 #ifdef DEBUG_DRAWAVATAR
        delay(10); //so that Serial monitor is not jammed
#endif   
  }
}

//Depreciated
void GameScene::renderCharacter(int16_t oldX, int16_t oldY, int16_t newX, int16_t newY, int16_t imageWidth, int16_t imageHeight, const uint16_t imageBitmap[],
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
