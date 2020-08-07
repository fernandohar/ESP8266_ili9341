#include "SPI.h"
#include <Adafruit_ILI9341.h>
#include "background.h"
#include "shrimp.h"
// ESP8266:
#define TFT_DC 15
#define TFT_CS 0

#define SCREENWIDTH  240
#define SCREENHEIGHT  320
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define MINSPEED 1
#define MAXSPEED 10
#define NUMBACKGROUND 2
const uint16_t *background [NUMBACKGROUND]; //pointer to the background bitmaps
int16_t shrimpX = 0;//SCREENWIDTH / 2;
int16_t shrimpY = SCREENHEIGHT / 2;
int16_t shrimpdX = 10;
int16_t shrimpdY = 0;

int bumps = 0; //counter variable for changing background image
int currentBackground = 0;
//Buffer for rendering, 2 Scanlines that alters
//One is rendered to while the other is transferred via DMA
uint16_t renderbuf[2][SCREENWIDTH];

void setup() {
  Serial.begin(9600);
     #if defined USE_SPI_DMA
      Serial.println("USE_SPI_DMA");
    #endif 
  // put your setup code here, to run once:
  tft.begin();
  shrimpdX = random(MINSPEED , MAXSPEED);
  shrimpdY = random(MINSPEED , MAXSPEED);
  background[0] = japanBGBitmap;
  background[1] = greenBGBitmap;

  tft.drawRGBBitmap(
        0,
        0,
        background[currentBackground],
        GREENBG_WIDTH, GREENBG_HEIGHT);  
}


void moveShrimp(){
  shrimpX = shrimpX + shrimpdX;
  
  if ( (shrimpX <= 0) || ((shrimpX + SHRIMP_WIDTH) >= SCREENWIDTH)){
    shrimpdX = random(MINSPEED , MAXSPEED) * ((shrimpdX > 0) ? -1 : 1 );  //Change the movement direction
    shrimpX = (shrimpX <= 0) ? 0 : SCREENWIDTH - SHRIMP_WIDTH ; //Clip the image within the screen
  }
  
  shrimpY = shrimpY + shrimpdY;
  if ((shrimpY <= 0) || ((shrimpY + SHRIMP_HEIGHT) >= SCREENHEIGHT)){
     shrimpdY = random(MINSPEED , MAXSPEED) *( (shrimpdY > 0) ? -1 : 1 );  //Change the movement direction
     shrimpY = (shrimpY <= 0) ? 0 : SCREENHEIGHT - SHRIMP_HEIGHT; //clip the image within the screen
     bumps++;
  }
}

//oldX and oldY are the previous position of the image on the Screen
//newX and newY are the upcoming position of the image on the Screen
//imageWidth and imageHeight are the width and height of the image
void renderShrimp(int16_t oldX, int16_t oldY, int16_t newX, int16_t newY, int16_t imageWidth, int16_t imageHeight){
  //Found out minimum the screen area to update. (to erase the old image and draw new image)
  //The area is combined bounds of previous and current shrimp position
  int16_t minx, miny, maxx, maxy, renderWidth, renderHeight;
  minx = (oldX < newX)? oldX : newX;
  miny = (oldY < newY) ? oldY : newY;
  maxx = ((oldX < newX)? newX : oldX) + imageWidth - 1;
  maxy = ((oldY < newY)? newY : oldY) + imageHeight - 1;
  renderWidth = maxx - minx + 1;
  renderHeight = maxy - miny + 1;
  
  tft.dmaWait();  // Wait for last line from prior call to complete
  tft.endWrite();
  tft.startWrite();
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
  for(y=0; y < renderHeight; y++){
    destPtr = &renderbuf[bufIdx][0]; 
    
    bx1 = bx;
    bgx1 = bgx;
    //Serial.println(" Y");
    //Serial.print(y);
    //String logStr = "";
    for(x=0; x < renderWidth; x++){
      if( (bx1 >= 0) &&  (bx1 < imageWidth )    //Check if current pixel is inside the image
         && (by >= 0) &&  (by < imageHeight)  
         ){
        //Check mask to see if we should draw bitmap or background
        if (bx1 & 7){
          byte <<= 1;
        }else{
          byte = pgm_read_byte(&ShrimpTailmask[by * bw + bx1 / 8]);
          //logStr = logStr + "    " + byte;
          
        } 
        if (byte & 0x80) {
          c= ShrimpTailBitmap[( by * SHRIMP_WIDTH ) + bx1 ];
          //logStr = logStr + " C";
        }else{
          c = background[currentBackground][ (bgy * SCREENWIDTH) + x + minx];
          //logStr = logStr + " M";
        }

      }else{ //outside the image area, draw background
        c = background[currentBackground][ (bgy * SCREENWIDTH) + x + minx];
        //logStr = logStr + " B";
      }
      bx1++;
      bgx++;
      *destPtr++ = c; // Store pixel color
    }
    //Serial.print(logStr);
    //delay(1);
    tft.dmaWait(); // Wait for prior line to complete
    tft.writePixels(&renderbuf[bufIdx][0], renderWidth, false); // Non-blocking write
    
    bufIdx = 1 - bufIdx; //change our renderbuffer (0, 1)
    by++;
    bgy++;
  }
}

void changeBackground(){
//not working
//  if (bumps == 3){
//    bumps = 0;
//    if (++currentBackground >= NUMBACKGROUND){
//      currentBackground = 0;
//    }
// 
//    Serial.println("wait for tft to finish");
//    tft.dmaWait();  // Wait for last line from prior call to complete
//    tft.endWrite();
//    Serial.println("changeBackground start");
//    tft.drawRGBBitmap(
//        0,
//        0,
//        background[0],
//        GREENBG_WIDTH, GREENBG_HEIGHT);
//    
//    Serial.println("changeBackground Done");
//  }
}
void loop() {
  // put your main code here, to run repeatedly:
    int16_t old_shrimpX = shrimpX;
    int16_t old_shrimpY = shrimpY;
    moveShrimp();
    changeBackground();
    Serial.println("renderShrimp");
    renderShrimp(old_shrimpX, old_shrimpY, shrimpX, shrimpY, SHRIMP_WIDTH, SHRIMP_HEIGHT);
    
}
