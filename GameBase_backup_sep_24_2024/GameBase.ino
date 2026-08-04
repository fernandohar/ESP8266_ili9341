#include "GameScene.h"
#include "Avatar.h"
#include "GameSceneManager.h"
#include "Scene_PorkHome.h"
#include "Scene_GameStart.h"

#define SCREENWIDTH 240
#define SCREENHEIGHT 320

#define TFT_DC 2
#define TFT_CS 15
//#define SPEAKER_PIN 16 //D0 define in GameScene.h
#define TOUCH_IRQ 5
#define TOUCH_CS 5

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
GameSceneManager manager = GameSceneManager(&tft, TOUCH_IRQ);


void setup() {
  Serial.begin(115200);
  SPI.begin();
  SPI.setFrequency(40000000);

  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(0);
  tft.setSwapBytes(true);

  //Touch Screen calibration
  uint16_t calData[5] = { 316, 3563, 424, 3491, 6 };
  tft.setTouch(calData);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);


  Scene_GameStart *startGame = new Scene_GameStart(&tft);
  //Scene_PorkHome *porkHome = new Scene_PorkHome(&tft);
  //automatically change scene when first appendScene is called

  manager.appendScene(startGame); //Scene index = 0
  //manager.appendScene(porkHome);  //Scene index = 2
}

void loop() {
  manager.update();
}
