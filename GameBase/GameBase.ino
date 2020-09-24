#include "GameScene.h"
#include  "Avatar.h"
#include "GameSceneManager.h"
#include "Scene_BearHome.h"
#include "Scene_PorkHome.h"
#include "Scene_GameStart.h"
#include "Bitmap_Background.h"


#include <TFT_eSPI.h>
#include <SPI.h>


#define GAMESPEED 25 //Constant Game speed

#define SCREENWIDTH 240
#define SCREENHEIGHT 320

#define TFT_DC 2
#define TFT_CS 15
#define SPEAKER_PIN 16 //D0
#define TOUCH_IRQ 5


TFT_eSPI tft = TFT_eSPI();

GameSceneManager manager = GameSceneManager(&tft, TOUCH_IRQ);


//Do not create Avatar at GameBase, it will be modified by other scenes
//Avatar* avatar1 = new Avatar(0, 0, CAT_WIDTH, CAT_HEIGHT, CatBitmap, CatMask);


void setup() {
  Serial.begin(115200);
  SPI.begin();
  SPI.setFrequency(40000000);
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(0);
  tft.setSwapBytes(true);

  //Setup Touch Screen Module
  uint16_t calData[5] = { 273, 3564, 475, 3430, 6 };
  tft.setTouch(calData);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);
  
  Scene_BearHome *bearHome = new Scene_BearHome(&tft);
  Scene_PorkHome *porkHome = new Scene_PorkHome(&tft);
  Scene_GameStart *startGame = new Scene_GameStart(&tft);
   //automatically change scene when first appendScene is called

  manager.appendScene(startGame); //Scene index = 0
  manager.appendScene(bearHome); 
  manager.appendScene(porkHome);

}

void loop() {
  //Let game update at 60Hz  

    manager.update();

}
