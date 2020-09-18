//#include "Adafruit_ILI9341esp.h"
#include <TFT_eSPI.h>
//#include "XPT2046.h"
#include <SPI.h>

#include "background.h"
#include "shrimp.h"
#include "pork.h"
#include "dragon.h"
#include "numbers.h"
#include "cat.h"
//TFT display
#define SCREENWIDTH  240
#define SCREENHEIGHT  320
#define TFT_DC 2
#define TFT_CS 15
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
TFT_eSPI tft = TFT_eSPI();
//Touch module
//XPT2046 touch(/*cs=*/ 4, /*irq=*/ 5);

//Game setting
#define MINSPEED 1
#define MAXSPEED 4
#define NUMBACKGROUND 2
#define NUMGAMEMODES 2

#define SPEAKER 16 //D0
#define _irq_pin 5
unsigned long renderTimer = 0;
unsigned long gameModeTimer = 0;
uint8_t gameMode = 0; //Mode 0 = Setup, Mode 1 = Yellow Room, Mode 2 = Red Room

const uint16_t *background [NUMBACKGROUND]; //pointer to the background bitmaps
const uint16_t *numberBitmap[10] = {ZeroBitmap, OneBitmap, TwoBitmap, ThreeBitmap, FourBitmap, FiveBitmap, SixBitmap, SevenBitmap, EightBitmap, NineBitmap};
const uint8_t *numberMask[10] = {ZeroMask, OneMask, TwoMask, ThreeMask, FourMask, FiveMask, SixMask, SevenMask, EightMask, NineMask};
int16_t avatar1X = 0;
int16_t avatar1Y = 0;
int16_t avatar1vX = 0;
int16_t avatar1vY = 0;
int16_t avatar2X = 0;
int16_t avatar2Y = 0;
int16_t avatar2vX = 0;
int16_t avatar2vY = 0;
int16_t avatar3X = 0;
int16_t avatar3Y = 0;
int16_t avatar3vX = 0;
int16_t avatar3vY = 0;
int16_t avatar4X = 0;
int16_t avatar4Y = 0;
int16_t old_avatar1X = avatar1X;
int16_t old_avatar1Y = avatar1Y;
int16_t old_avatar2X = avatar2X;
int16_t old_avatar2Y = avatar2Y;
int16_t old_avatar3X = avatar3X;
int16_t old_avatar3Y = avatar3Y;
int16_t old_avatar4X = avatar4X;
int16_t old_avatar4Y = avatar4Y;


uint32_t frame, startTime = 0 ;
int currentBackground = 0;
//Buffer for rendering, 2 Scanlines that alters
//One is rendered to while the other is transferred via DMA
static uint16_t renderbuf[2][SCREENWIDTH];
static uint16_t currentScreenScanLineBuffer[SCREENWIDTH];

void setup() {
  Serial.begin(115200);
  SPI.begin();  
  SPI.setFrequency(40000000);
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(2);

  tft.setRotation(0);
  tft.setSwapBytes(true);
  background[0] = whiteBearHome;
  background[1] = friedPorkHome;
  initAvatarPosition();
  setGameMode(gameMode);


  //Setup Touch Screen Module
  uint16_t calData[5] = { 273, 3564, 475, 3430, 6 };
  tft.setTouch(calData);
  pinMode(_irq_pin, INPUT_PULLUP);
  
  startTime = millis();
}

uint8_t shakeseed = 0;
unsigned long shaketimer = 0;
void shakeCharacters() {
  if (millis() - shaketimer > 300) { //Control shaking speed
    shaketimer = millis();
    shakeseed = 1 - shakeseed;
    int8_t dx = (shakeseed > 0) ? -2 : 2;
    avatar1X += dx;
    avatar1Y += dx * -1; //rock left and right
    avatar2Y += (dx * 4); //Jump up and down
    avatar3X += (dx * -1); //rock right and left
  }
}

void moveCharacters() {
  avatar1X = avatar1X + avatar1vX;
  avatar1Y = avatar1Y + avatar1vY;

  avatar2X = avatar2X + avatar2vX;
  avatar2Y = avatar2Y + avatar2vY;

  avatar3X = avatar3X + avatar3vX;
  avatar3Y = avatar3Y + avatar3vY;

  bool collide1_2 = false;
  bool collide1_3 = false;
  bool collide2_3 = false;

  //Collision Detection using Circle
  int16_t dx = avatar1X - avatar2X;
  int16_t dy = avatar1Y - avatar2Y;
  double distance = sqrt( dx * dx + dy * dy );
  if (distance <  SHRIMP_WIDTH) {
    collide1_2 = true;
  }

  dx = avatar1X - avatar3X;
  dy = avatar1Y - avatar3Y;
  distance = sqrt( dx * dx + dy * dy );
  if (distance <  DRAGON_WIDTH) {
    collide1_3 = true;
  }

  dx = avatar2X - avatar3X;
  dy = avatar2Y - avatar3Y;
  distance = sqrt( dx * dx + dy * dy );
  if (distance <  DRAGON_WIDTH) {
    collide2_3 = true;
  }
  //  //Collision Detection using bounding box
  //  int16_t Xmax1, Xmax2, Ymax1, Ymax2;
  //  Xmax1 = avatar1X + SHRIMP_WIDTH;
  //  Xmax2 = avatar2X + SHRIMP_WIDTH;
  //  Ymax1 = avatar1Y + SHRIMP_HEIGHT;
  //  Ymax2 = avatar2Y + SHRIMP_HEIGHT;
  //  collide =  (Xmax1 >= avatar2X && avatar1X <= Xmax2  && Ymax1 >= avatar2Y && avatar1Y <= Ymax2);

  if (collide1_2 || collide1_3) {
    //they crashed and same direction
    avatar1vX = random(MINSPEED , MAXSPEED) * ((avatar1vX > 0) ? -1 : 1 );
    avatar1vY = random(MINSPEED , MAXSPEED) * ((avatar1vY > 0) ? -1 : 1 );
  }
  if (collide1_2 || collide2_3) {
    avatar2vX = random(MINSPEED , MAXSPEED) * ((avatar2vX > 0) ? -1 : 1 );
    avatar2vY = random(MINSPEED , MAXSPEED) * ((avatar2vY > 0) ? -1 : 1 );
  }
  if (collide1_3 || collide2_3) {
    avatar3vX = random(MINSPEED , MAXSPEED) * ((avatar3vX > 0) ? -1 : 1 );
    avatar3vY = random(MINSPEED , MAXSPEED) * ((avatar3vY > 0) ? -1 : 1 );
  }
  if ( (avatar1X <= 0) || ((avatar1X + SHRIMP_WIDTH) >= SCREENWIDTH)) {
    avatar1vX = random(MINSPEED , MAXSPEED) * ((avatar1vX > 0) ? -1 : 1 );  //Change the movement direction
    avatar1X = (avatar1X <= 0) ? 0 : SCREENWIDTH - SHRIMP_WIDTH ; //Clip the image within the screen
  }
  if ((avatar1Y <= 0) || ((avatar1Y + SHRIMP_HEIGHT) >= SCREENHEIGHT)) {
    avatar1vY = random(MINSPEED , MAXSPEED) * ( (avatar1vY > 0) ? -1 : 1 ); //Change the movement direction
    avatar1Y = (avatar1Y <= 0) ? 0 : SCREENHEIGHT - SHRIMP_HEIGHT; //clip the image within the screen
  }

  if ( (avatar2X <= 0) || ((avatar2X + PORK_WIDTH) >= SCREENWIDTH)) {
    avatar2vX = random(MINSPEED , MAXSPEED) * ((avatar2vX > 0) ? -1 : 1 );  //Change the movement direction
    avatar2X = (avatar2X <= 0) ? 0 : SCREENWIDTH - PORK_WIDTH ; //Clip the image within the screen
    // Serial.println("#2 Bounds Screen Left/Right Edge");
  }
  if ((avatar2Y <= 0) || ((avatar2Y + PORK_HEIGHT) >= SCREENHEIGHT)) {
    avatar2vY = random(MINSPEED , MAXSPEED) * ( (avatar2vY > 0) ? -1 : 1 ); //Change the movement direction
    avatar2Y = (avatar2Y <= 0) ? 0 : SCREENHEIGHT - PORK_HEIGHT; //clip the image within the screen
  }
  if ( (avatar3X <= 0) || ((avatar3X + DRAGON_WIDTH) >= SCREENWIDTH)) {
    avatar3vX = random(MINSPEED , MAXSPEED) * ((avatar3vX > 0) ? -1 : 1 );  //Change the movement direction
    avatar3X = (avatar3X <= 0) ? 0 : SCREENWIDTH - DRAGON_WIDTH ; //Clip the image within the screen
  }
  if ((avatar3Y <= 0) || ((avatar3Y + DRAGON_HEIGHT) >= SCREENHEIGHT)) {
    avatar3vY = random(MINSPEED , MAXSPEED) * ( (avatar3vY > 0) ? -1 : 1 ); //Change the movement direction
    avatar3Y = (avatar3Y <= 0) ? 0 : SCREENHEIGHT - DRAGON_HEIGHT; //clip the image within the screen
  }

}


//oldX and oldY are the previous position of the character image on the Screen
//newX and newY are the upcoming position of the character image on the Screen
//imageWidth and imageHeight are the width and height of the image
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

  tft.endWrite();
  tft.startWrite();

  if (renderWidth > SCREENWIDTH) {
    return;
  }
  tft.setAddrWindow(minx, miny, renderWidth, renderHeight);

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
    //tft.readRect(0, y, SCREENWIDTH, 1, currentScreenScanLineBuffer);
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
          c = pgm_read_word_near(background[currentBackground] + (bgy * backgroundWidth) + x + minx);
        }

      } else { //outside the image area, draw background
        c = pgm_read_word_near(background[currentBackground] + (bgy * backgroundWidth) + x + minx);

      }
      bx1++;
      bgx++;
      *destPtr++ = c; // Store pixel color
    }
    tft.pushPixels(&renderbuf[bufIdx][0], renderWidth);

    bufIdx = 1 - bufIdx; //change our renderbuffer (0, 1)
    by++;
    bgy++;

  }
}

//drawRGBBitmap is modified from AdaFruit GFX libraray, improved rendering speed by  writing line by line instead of pixel by pixel
void drawRGBBitmap( const uint16_t *bitmap) {
  tft.pushImage( 0, 0, SCREENWIDTH, SCREENHEIGHT, bitmap);
  //  uint16_t c;
  //  uint16_t *destPtr;
  //  int8_t bufIdx = 0;
  //  tft.endWrite();
  //  tft.startWrite();
  //  tft.setAddrWindow(0, 0, SCREENWIDTH, SCREENHEIGHT);
  //  uint16_t h = SCREENHEIGHT;
  //  for (int16_t j = 0; j < h; j++) {
  //    destPtr = &renderbuf[bufIdx][0];
  //    for (int16_t i = 0; i < SCREENWIDTH; i++) {
  //      c = pgm_read_word(&bitmap[j * SCREENWIDTH + i]);
  //      *destPtr = c;
  //      destPtr++;
  //    }
  ////    tft.writePixels(&renderbuf[bufIdx][0], SCREENWIDTH);
  //tft.pushPixels(&renderbuf[bufIdx][0], SCREENWIDTH);
  //    bufIdx = 1 - bufIdx;
  //  }
  //  tft.endWrite();
}

//nextBackground is for testing purpose
void nextBackground() {
  if (++currentBackground >= NUMBACKGROUND) {
    currentBackground = 0;
  }
  setBackground(currentBackground);
}

void setBackground(int backgroundNum) {
  currentBackground = backgroundNum;
  drawRGBBitmap(background[currentBackground]);
}

void nextGameMode() {
  if (++gameMode >= NUMGAMEMODES) {
    gameMode = 0 ;
  }
  setGameMode(gameMode);
}
int rotation = 0;
void initAvatarPosition(){
  avatar1X = SCREENWIDTH - SHRIMP_WIDTH - PORK_WIDTH - DRAGON_WIDTH - 3;
  avatar1Y = 10;
  avatar2X = SCREENWIDTH - PORK_WIDTH - DRAGON_WIDTH - 3;
  avatar2Y = SCREENHEIGHT - PORK_HEIGHT - 42;
  avatar3X = SCREENWIDTH - DRAGON_WIDTH - 3;
  avatar3Y = SCREENHEIGHT - DRAGON_HEIGHT - 42;
  avatar4X = 0;
  avatar4Y = 0;
}
void setGameMode(uint8_t gameMode) {
  Serial.print("Change Game Mode to: "); Serial.println(gameMode);
  if (gameMode == 0) {
    shakeseed = 0;
    setBackground(0);
  } else if (gameMode == 1) {
    setBackground(1);
  } else {
    Serial.print("ERROR with Change Game Mode, gameMode not handled :");
    Serial.println(gameMode);
  }
}

//bool showColon = true;
void displayNum(uint32_t num) {
  //  //timeClient.update();
  //  uint8_t hour = timeClient.getHours();
  //  uint8_t minute = timeClient.getMinutes();

  uint8_t x1,  x2, x3, x4, x5, y;
  x1 = 20;
  y = 20;
  x2 = x1 + NUMBER_WIDTH;
  x3 = x2 + NUMBER_WIDTH;
  x4 = x3 + NUMBER_WIDTH;
  x5 = x4 + NUMBER_WIDTH;
  byte tempDigit1  = (num / 10000);
  byte tempDigit2 = (num % 10000) / 1000;
  byte tempDigit3 = (num % 1000) / 100;
  byte tempDigit4 = (num % 100) / 10;
  byte tempDigit5 = num % 10;

  renderCharacter(x1, 20, x1, 20, NUMBER_WIDTH, FONT_HEIGHT, numberBitmap[tempDigit1], numberMask[tempDigit1], SCREENWIDTH);
  renderCharacter(x2, 20, x2, 20, NUMBER_WIDTH, FONT_HEIGHT, numberBitmap[tempDigit2], numberMask[tempDigit2], SCREENWIDTH);
  renderCharacter(x3, 20, x3, 20, NUMBER_WIDTH, FONT_HEIGHT, numberBitmap[tempDigit3], numberMask[tempDigit3], SCREENWIDTH);
  renderCharacter(x4, 20, x4, 20, NUMBER_WIDTH, FONT_HEIGHT, numberBitmap[tempDigit4], numberMask[tempDigit4], SCREENWIDTH);
  renderCharacter(x5, 20, x5, 20, NUMBER_WIDTH, FONT_HEIGHT, numberBitmap[tempDigit5], numberMask[tempDigit5], SCREENWIDTH);
}

boolean ClickedOnAvatar(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t touchX, uint16_t touchY) {
  return touchX >= x && touchX <= (x + width) && touchY >= y && touchY <= (y + height);
}


uint32_t fps = 0;
uint16_t touchX = 0;
uint16_t touchY = 0;
boolean isTouched = false;
boolean isContinuousTouch = false;
int8_t characterClicked = -1;

unsigned long toneStart = 0;
void loop() {
  //Auto change game mode is only for Demo
  //  if (millis() - gameModeTimer >= 100000) {
  //
  //    gameModeTimer = millis();
  //    nextGameMode();
  //  }
  if(millis() - toneStart >= 50){
    noTone(SPEAKER) ; //Turn off the pin attached to the tone()
  }
  
  //Process User Inputs
  isTouched = (digitalRead(_irq_pin) == LOW) && tft.getTouch(&touchX, &touchY);
  if (isTouched) { //Always process the userinputs
    if(!isContinuousTouch){ //find which avatar being clicked
      Serial.println("First touch ");
      isContinuousTouch = true;
      if ( ClickedOnAvatar(avatar1X, avatar1Y, CAT_WIDTH , CAT_HEIGHT, touchX, touchY)) {
        characterClicked = 1;
        tone(SPEAKER, 523) ; //DO note 523 Hz
        toneStart = millis();
      } else if (ClickedOnAvatar(avatar2X, avatar2Y, PORK_WIDTH , PORK_HEIGHT, touchX, touchY)) {
        characterClicked = 2;
        tone(SPEAKER, 587) ; //RE note ...
        toneStart = millis();
      } else if (ClickedOnAvatar(avatar3X, avatar3Y, DRAGON_WIDTH , DRAGON_HEIGHT, touchX, touchY)) {
        characterClicked = 3;
        tone(SPEAKER, 659) ; //MI note ...
        toneStart = millis();
      } else if (ClickedOnAvatar(avatar4X, avatar4Y, SHRIMP_WIDTH , SHRIMP_HEIGHT, touchX, touchY)) {
        characterClicked = 4;
        tone(SPEAKER, 783) ; //FA note ...
        toneStart = millis();
      }else{
        characterClicked = -1;
        noTone(SPEAKER) ; //Turn off the pin attached to the tone()
      }
      Serial.println(characterClicked);
    }else{
       Serial.println("continuous touch ");
       toneStart = millis();
    }
    
    if ( characterClicked == 1) {
      avatar1X = touchX - CAT_WIDTH / 2;
      avatar1Y = touchY - CAT_HEIGHT / 2;
    } else if (characterClicked == 2) {
      avatar2X = touchX - PORK_WIDTH / 2;
      avatar2Y = touchY - PORK_HEIGHT / 2;
    } else if (characterClicked == 3) {
      avatar3X = touchX - DRAGON_WIDTH / 2;
      avatar3Y = touchY - DRAGON_HEIGHT / 2;
    } else if (characterClicked == 4) {
      avatar4X = touchX - SHRIMP_WIDTH / 2;
      avatar4Y = touchY - SHRIMP_HEIGHT / 2;
      //nextGameMode();
    } else if (characterClicked == -1){
      
    }
    
  } else {
    isContinuousTouch = false;
    //Process game logic (character movement)
    if (gameMode == 0) {
      shakeCharacters();
      //moveCharacters();
    } else {
      shakeCharacters();
    }
  }

  if (millis() - renderTimer >= 50) { //50 / 1000 = 20 fps
    renderTimer = millis();
    renderCharacter(old_avatar1X, old_avatar1Y, avatar1X, avatar1Y, CAT_WIDTH, CAT_HEIGHT, CatBitmap, CatMask, SCREENWIDTH);
    renderCharacter(old_avatar2X, old_avatar2Y, avatar2X, avatar2Y, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask, SCREENWIDTH);
    renderCharacter(old_avatar3X, old_avatar3Y, avatar3X, avatar3Y, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask, SCREENWIDTH);
    renderCharacter(old_avatar4X, old_avatar4Y, avatar4X, avatar4Y, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask, SCREENWIDTH);
    old_avatar1X = avatar1X; //Save the rendered position for next rendering
    old_avatar1Y = avatar1Y;
    old_avatar2X = avatar2X;
    old_avatar2Y = avatar2Y;
    old_avatar3X = avatar3X;
    old_avatar3Y = avatar3Y;
    old_avatar4X = avatar4X;
    old_avatar4Y = avatar4Y;

    //displayNum(frame);
  }
  // Show approximate frame rate
//  if (!(++frame & 255)) { // Every 256 frames...
//    uint32_t elapsed = (millis() - startTime) / 1000; // Seconds
//    if (elapsed) {
//      Serial.print(frame / elapsed);
//      Serial.println(" fps");
//      fps = frame / elapsed;
//    }
//  }
}
