#include "SPI.h"
#include <Adafruit_ILI9341.h>
#include "background.h"
#include "shrimp.h"
#include "pork.h"
#include "dragon.h"
#include "numbers.h"
// ESP8266:
#define TFT_DC 15
#define TFT_CS 0

#define SCREENWIDTH  240
#define SCREENHEIGHT  320
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define MINSPEED 1
#define MAXSPEED 4
#define NUMBACKGROUND 2
#define NUMGAMEMODES 2
unsigned long renderTimer = 0;
unsigned long gameModeTimer = 0;
uint8_t gameMode = 0;

const uint16_t *background [NUMBACKGROUND]; //pointer to the background bitmaps
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

int bumps = 0; //counter variable for changing background image
int currentBackground = 0;
//Buffer for rendering, 2 Scanlines that alters
//One is rendered to while the other is transferred via DMA
static uint16_t renderbuf[2][SCREENWIDTH];

void setup() {
  Serial.begin(9600);
  tft.begin();


  background[0] = whiteBearHome;
  background[1] = friedPorkHome;

//  tft.drawRGBBitmap(
//    0,
//    0,
//    background[currentBackground],
//    SCREENWIDTH, SCREENHEIGHT);
 changeGameMode(gameMode);
}
uint8_t shakeseed = 0;
unsigned long shaketimer = 0;
void shakeCharacters() {
  if(millis() - shaketimer > 300){ //prevemt shaking too fast
    shaketimer = millis();
  shakeseed = 1 - shakeseed;
  int8_t dx = (shakeseed > 0) ? -2 : 2;
  avatar1X += dx;
  avatar1Y += dx * -1;
  //avatar2X += dx;
  avatar2Y += (dx * 4);
  avatar3X += (dx * -1);
  //avatar3Y += dx;
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
  //The area is combined bounds of previous and current shrimp position
  int16_t minx, miny, maxx, maxy, renderWidth, renderHeight;
  minx = (oldX < newX) ? oldX : newX;
  miny = (oldY < newY) ? oldY : newY;
  maxx = ((oldX < newX) ? newX : oldX) + imageWidth - 1;
  maxy = ((oldY < newY) ? newY : oldY) + imageHeight - 1;
  renderWidth = maxx - minx + 1;
  renderHeight = maxy - miny + 1;

  tft.dmaWait();  // Wait for last line from prior call to complete
  tft.endWrite();
  tft.startWrite();
  tft.setAddrWindow(minx, miny, renderWidth, renderHeight);
  //Serial.println("setAddrWindow");
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
  //String logStr = "";
  //The following loop work with each row(x), and then moves downwards (y)
  for (y = 0; y < renderHeight; y++) {
    //Serial.println("Y");
    destPtr = &renderbuf[bufIdx][0];

    bx1 = bx;
    bgx1 = bgx;
    //Serial.println("");
    //Serial.print(y);
    //logStr = "";
    for (x = 0; x < renderWidth; x++) {
      if ( (bx1 >= 0) &&  (bx1 < imageWidth )   //Check if current pixel is inside the image
           && (by >= 0) &&  (by < imageHeight)
         ) {
        //Check mask to see if we should draw bitmap or background
        if (bx1 & 7) {

          byte <<= 1;
        } else {

          byte = pgm_read_byte(&imageMask[by * bw + bx1 / 8]);
          //logStr = logStr + "    " + byte;
        }
        if (byte & 0x80) {
          //c = imageBitmap[( by * imageWidth ) + bx1 ];
          c = pgm_read_word_near(imageBitmap + ( by * imageWidth ) + bx1 );
          //logStr = logStr + " C";
        } else {
          //          c = background[currentBackground][ (bgy * SCREENWIDTH) + x + minx];
          c = pgm_read_word_near(background[currentBackground] + (bgy * backgroundWidth) + x + minx);
          //logStr = logStr + " M";
        }

      } else { //outside the image area, draw background
        //c = backgroundBitmap[ (bgy * backgroundWidth) + x + minx];
        c = pgm_read_word_near(background[currentBackground] + (bgy * backgroundWidth) + x + minx);
        //logStr = logStr + " B";
      }
      bx1++;
      bgx++;
      *destPtr++ = c; // Store pixel color
    }
    //Serial.print(logStr);
    //
    //tft.dmaWait(); // Wait for prior line to complete
    //Serial.println("Write Pixel");
    tft.writePixels(&renderbuf[bufIdx][0], renderWidth, true); // blocking write
    //delay(1);
    bufIdx = 1 - bufIdx; //change our renderbuffer (0, 1)
    by++;
    bgy++;

  }
}
bool bgNeedChange = false;
void drawRGBBitmap( const uint16_t *bitmap) {
  Serial.println ("draw background Start ");
  uint16_t c;
  uint16_t *destPtr;
  int8_t bufIdx = 0;
  tft.endWrite();
  tft.startWrite();
  tft.setAddrWindow(0, 0, SCREENWIDTH, SCREENHEIGHT);
  uint16_t h = SCREENHEIGHT;
  for (int16_t j = 0; j < h; j++) {
    destPtr = &renderbuf[bufIdx][0];
    for (int16_t i = 0; i < SCREENWIDTH; i++) {
      c = pgm_read_word(&bitmap[j * SCREENWIDTH + i]);
      *destPtr = c;
      destPtr++;
    }
    //Serial.println("WritePixels");
    tft.writePixels(&renderbuf[bufIdx][0], SCREENWIDTH, true);
    bufIdx = 1 - bufIdx;
  }
  tft.endWrite();
  Serial.println ("draw background End ");
}
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



void changeGameMode(uint8_t gameMode) {
  if (gameMode == 0) {
    Serial.println("Init gameMode 0");
    avatar1X = SCREENWIDTH - SHRIMP_WIDTH - PORK_WIDTH - DRAGON_WIDTH - 3;
    avatar1Y = SCREENHEIGHT - SHRIMP_HEIGHT - 42;
    avatar2X = SCREENWIDTH - PORK_WIDTH - DRAGON_WIDTH - 3;
    avatar2Y = SCREENHEIGHT - PORK_HEIGHT - 42;
    avatar3X = SCREENWIDTH - DRAGON_WIDTH - 3;
    avatar3Y = SCREENHEIGHT - DRAGON_HEIGHT - 42;
    shakeseed = 0;
    setBackground(0);
  } else {
    Serial.println("Init gameMode 1");
    avatar1X = SCREENWIDTH - SHRIMP_WIDTH - PORK_WIDTH - DRAGON_WIDTH - 3;
    avatar1Y = SCREENHEIGHT - SHRIMP_HEIGHT - 42;
    avatar2X = SCREENWIDTH - PORK_WIDTH - DRAGON_WIDTH - 3;
    avatar2Y = SCREENHEIGHT - PORK_HEIGHT - 42;
    avatar3X = SCREENWIDTH - DRAGON_WIDTH - 3;
    avatar3Y = SCREENHEIGHT - DRAGON_HEIGHT - 42;
    
   
    avatar1vX = random(MINSPEED , MAXSPEED);
    avatar1vY = random(MINSPEED , MAXSPEED);
   
    avatar2vX = random(MINSPEED , MAXSPEED);
    avatar2vY = random(MINSPEED , MAXSPEED);
  
    avatar3vX = random(MINSPEED , MAXSPEED);
    avatar3vY = random(MINSPEED , MAXSPEED);
    setBackground(1);
  }
}
void displayClock(){
  uint8_t x1,  x2, x3, x4, x5, y;
  x1 = 20;
  y = 20;
  x2 = x1 + NUMBER_WIDTH;
  x3 = x2 + NUMBER_WIDTH;
  x4 = x3 + NUMBER_WIDTH;
  x5 = x4 + NUMBER_WIDTH;
  renderCharacter(x1, 20, x1, 20, NUMBER_WIDTH, FONT_HEIGHT, ZeroBitmap, ZeroMask, SCREENWIDTH);
  renderCharacter(x2, 20, x2, 20, NUMBER_WIDTH, FONT_HEIGHT, OneBitmap, OneMask, SCREENWIDTH);
  renderCharacter(x3, 20, x3, 20, NUMBER_WIDTH, FONT_HEIGHT, SlashBitmap, SlashMask, SCREENWIDTH);
  renderCharacter(x4, 20, x4, 20, NUMBER_WIDTH, FONT_HEIGHT, ThreeBitmap, ThreeMask, SCREENWIDTH);
  renderCharacter(x5, 20, x5, 20, NUMBER_WIDTH, FONT_HEIGHT, FourBitmap, FourMask, SCREENWIDTH);
}
void loop() {
  if (millis() - gameModeTimer >= 10000) {

    gameModeTimer = millis();
    if (++gameMode >= NUMGAMEMODES) { //
      gameMode = 0;
    }
    changeGameMode(gameMode);
  }
  if (millis() - renderTimer >= 33) { //33 / 1000 = 30 fps
    renderTimer = millis();
    if (gameMode == 0) {
      //
      int16_t old_avatar1X = avatar1X;
      int16_t old_avatar1Y = avatar1Y;
      int16_t old_avatar2X = avatar2X;
      int16_t old_avatar2Y = avatar2Y;
      int16_t old_avatar3X = avatar3X;
      int16_t old_avatar3Y = avatar3Y;
      shakeCharacters();
      //Serial.println("shakeCharacters");
      renderCharacter(old_avatar1X, old_avatar1Y, avatar1X, avatar1Y, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask, SCREENWIDTH);
      renderCharacter(old_avatar2X, old_avatar2Y, avatar2X, avatar2Y, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask, SCREENWIDTH);
      renderCharacter(old_avatar3X, old_avatar3Y, avatar3X, avatar3Y, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask, SCREENWIDTH);

    } else if (gameMode == 1) {
      
      int16_t old_avatar1X = avatar1X;
      int16_t old_avatar1Y = avatar1Y;
      int16_t old_avatar2X = avatar2X;
      int16_t old_avatar2Y = avatar2Y;
      int16_t old_avatar3X = avatar3X;
      int16_t old_avatar3Y = avatar3Y;
      shakeCharacters();
      renderCharacter(old_avatar1X, old_avatar1Y, avatar1X, avatar1Y, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask, SCREENWIDTH);
      renderCharacter(old_avatar2X, old_avatar2Y, avatar2X, avatar2Y, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask, SCREENWIDTH);
      renderCharacter(old_avatar3X, old_avatar3Y, avatar3X, avatar3Y, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask, SCREENWIDTH);
      //    }else if (gameMode == 2){
      //          int16_t old_avatar1X = avatar1X;
      //          int16_t old_avatar1Y = avatar1Y;
      //          int16_t old_avatar2X = avatar2X;
      //          int16_t old_avatar2Y = avatar2Y;
      //          moveCharacters();
      //          renderCharacter(old_avatar1X, old_avatar1Y, avatar1X, avatar1Y, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask, SCREENWIDTH);
      //          renderCharacter(old_avatar2X, old_avatar2Y, avatar2X, avatar2Y, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask, SCREENWIDTH);
      //          renderCharacter(avatar3X, avatar3Y, avatar3X, avatar3Y, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask, SCREENWIDTH);
    }

    displayClock();
  }
  
}
