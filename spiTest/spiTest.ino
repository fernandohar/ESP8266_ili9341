#include "SPI.h"
#include <Adafruit_ILI9341.h>
#include "background.h"
#include "shrimp.h"
#include "pork.h"
#include "dragon.h"
// ESP8266:
#define TFT_DC 15
#define TFT_CS 0

#define SCREENWIDTH  240
#define SCREENHEIGHT  320
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define MINSPEED 1
#define MAXSPEED 4
#define NUMBACKGROUND 2
const uint16_t *background [NUMBACKGROUND]; //pointer to the background bitmaps
int16_t avatar1X = 0;
int16_t avatar1Y = 0;
int16_t avatar1vX = 0;
int16_t avatar1vY = 0;

int16_t avatar2X = SCREENWIDTH /2;
int16_t avatar2Y = SCREENHEIGHT / 2;
int16_t avatar2vX = 0;
int16_t avatar2vY = 0;

int16_t avatar3X = SCREENWIDTH - DRAGON_WIDTH;
int16_t avatar3Y = SCREENHEIGHT - DRAGON_HEIGHT;

int bumps = 0; //counter variable for changing background image
int currentBackground = 0;
//Buffer for rendering, 2 Scanlines that alters
//One is rendered to while the other is transferred via DMA
static uint16_t renderbuf[2][SCREENWIDTH];

void setup() {
  Serial.begin(9600);

  // put your setup code here, to run once:
  tft.begin();
  avatar1vX = random(MINSPEED , MAXSPEED);
  avatar1vY = random(MINSPEED , MAXSPEED);
  avatar2vX = random(MINSPEED , MAXSPEED);
  avatar2vY = random(MINSPEED , MAXSPEED);

  background[0] = whiteBearHome;
  background[1] = friedPorkHome;
  //Serial.println("drawRGBBitmap");
  tft.drawRGBBitmap(
    0,
    0,
    background[currentBackground],
    GREENBG_WIDTH, GREENBG_HEIGHT);
}


void moveCharacters() {
  avatar1X = avatar1X + avatar1vX;
  avatar1Y = avatar1Y + avatar1vY;


  avatar2X = avatar2X + avatar2vX;
  avatar2Y = avatar2Y + avatar2vY;

  bool collide = false;
  //Collision Detection using Circle
  int16_t dx = avatar1X - avatar2X;
  int16_t dy = avatar1Y - avatar2Y;
  double distance = sqrt( dx * dx + dy * dy );
  if(distance <  SHRIMP_WIDTH){
    collide = true;
  }
//  //Collision Detection using bounding box
//  int16_t Xmax1, Xmax2, Ymax1, Ymax2;
//  Xmax1 = avatar1X + SHRIMP_WIDTH;
//  Xmax2 = avatar2X + SHRIMP_WIDTH;
//  Ymax1 = avatar1Y + SHRIMP_HEIGHT;
//  Ymax2 = avatar2Y + SHRIMP_HEIGHT;
//  collide =  (Xmax1 >= avatar2X && avatar1X <= Xmax2  && Ymax1 >= avatar2Y && avatar1Y <= Ymax2); 
  
  if (collide){
     //they crashed and same direction
    avatar1vX = random(MINSPEED , MAXSPEED) * ((avatar1vX > 0) ? -1 : 1 );
    avatar2vX = random(MINSPEED , MAXSPEED) * ((avatar2vX > 0) ? -1 : 1 );
    avatar1vY = random(MINSPEED , MAXSPEED) * ((avatar1vY > 0) ? -1 : 1 );
    avatar2vY = random(MINSPEED , MAXSPEED) * ((avatar2vY > 0) ? -1 : 1 );
   // Serial.println("Collide");
  }else{ //Clip the image within the screen
    if ( (avatar1X <= 0) || ((avatar1X + SHRIMP_WIDTH) >= SCREENWIDTH)) {
      avatar1vX = random(MINSPEED , MAXSPEED) * ((avatar1vX > 0) ? -1 : 1 );  //Change the movement direction
      avatar1X = (avatar1X <= 0) ? 0 : SCREENWIDTH - SHRIMP_WIDTH ; //Clip the image within the screen
    //  Serial.println("#1 Bounds Screen Left/Right Edge");
    }


    if ((avatar1Y <= 0) || ((avatar1Y + SHRIMP_HEIGHT) >= SCREENHEIGHT)) {
      avatar1vY = random(MINSPEED , MAXSPEED) * ( (avatar1vY > 0) ? -1 : 1 ); //Change the movement direction
      avatar1Y = (avatar1Y <= 0) ? 0 : SCREENHEIGHT - SHRIMP_HEIGHT; //clip the image within the screen
      //Serial.println("#1 Bounds Screen Top Bottom Edge");
    }

    if ( (avatar2X <= 0) || ((avatar2X + SHRIMP_WIDTH) >= SCREENWIDTH)) {
      avatar2vX = random(MINSPEED , MAXSPEED) * ((avatar2vX > 0) ? -1 : 1 );  //Change the movement direction
      avatar2X = (avatar2X <= 0) ? 0 : SCREENWIDTH - SHRIMP_WIDTH ; //Clip the image within the screen
     // Serial.println("#2 Bounds Screen Left/Right Edge");
    }


    if ((avatar2Y <= 0) || ((avatar2Y + SHRIMP_HEIGHT) >= SCREENHEIGHT)) {
      avatar2vY = random(MINSPEED , MAXSPEED) * ( (avatar2vY > 0) ? -1 : 1 ); //Change the movement direction
      avatar2Y = (avatar2Y <= 0) ? 0 : SCREENHEIGHT - SHRIMP_HEIGHT; //clip the image within the screen
     // Serial.println("#2 Bounds Screen Top Bottom Edge");
    }
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
          c= pgm_read_word_near(imageBitmap + ( by * imageWidth ) + bx1 );
          //logStr = logStr + " C";
        } else {
          //          c = background[currentBackground][ (bgy * SCREENWIDTH) + x + minx];
          c= pgm_read_word_near(background[currentBackground] + (bgy * backgroundWidth) + x + minx);
          //logStr = logStr + " M";
        }

      } else { //outside the image area, draw background
        //c = backgroundBitmap[ (bgy * backgroundWidth) + x + minx];
        c= pgm_read_word_near(background[currentBackground] + (bgy * backgroundWidth) + x + minx);
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
void changeBackground() {
  if (++currentBackground >= NUMBACKGROUND){
    currentBackground = 0;
    
  }
  Serial.print("CurrentBackground: ");
  Serial.println(currentBackground);
  drawRGBBitmap(background[currentBackground]);
}
unsigned long renderTimer = 0;
unsigned long gameModeTimer = 0;
uint8_t gameMode = 0;

void loop() {
  if(millis() - gameModeTimer >= 10000){
    gameMode++;
    gameModeTimer = millis();
    changeBackground();
    if(gameMode >= 3){
      gameMode = 0;
    }
  }  

  if(gameMode == 0){
      //33 / 1000 = 30 fps
      if ( millis() - renderTimer >= 16) { //Control the game speed 
        renderTimer = millis();
        int16_t old_avatar1X = avatar1X;
        int16_t old_avatar1Y = avatar1Y;
        int16_t old_avatar2X = avatar2X;
        int16_t old_avatar2Y = avatar2Y;
        moveCharacters();
        renderCharacter(old_avatar1X, old_avatar1Y, avatar1X, avatar1Y, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask, SCREENWIDTH);
        renderCharacter(old_avatar2X, old_avatar2Y, avatar2X, avatar2Y, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask, SCREENWIDTH);
      }
  }else if (gameMode == 1){
   
        int16_t old_avatar1X = avatar1X;
        int16_t old_avatar1Y = avatar1Y;
        int16_t old_avatar2X = avatar2X;
        int16_t old_avatar2Y = avatar2Y;
        moveCharacters();
        renderCharacter(old_avatar1X, old_avatar1Y, avatar1X, avatar1Y, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask, SCREENWIDTH);
        renderCharacter(old_avatar2X, old_avatar2Y, avatar2X, avatar2Y, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask, SCREENWIDTH);
  }else if (gameMode == 2){
        int16_t old_avatar1X = avatar1X;
        int16_t old_avatar1Y = avatar1Y;
        int16_t old_avatar2X = avatar2X;
        int16_t old_avatar2Y = avatar2Y;
        moveCharacters();
        renderCharacter(old_avatar1X, old_avatar1Y, avatar1X, avatar1Y, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask, SCREENWIDTH);
        renderCharacter(old_avatar2X, old_avatar2Y, avatar2X, avatar2Y, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask, SCREENWIDTH);
        renderCharacter(avatar3X, avatar3Y, avatar3X, avatar3Y, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask, SCREENWIDTH);
  }
  
}
