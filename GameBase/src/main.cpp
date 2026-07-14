#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#if defined(WOKWI_CAP_TOUCH)
#include <Adafruit_FT6206.h>
#include <Wire.h>
#endif

#include "Avatar.h"
#include "GameScene.h"
#include "GameSceneManager.h"
#include "Scene_GameStart.h"
#include "Scene_PorkHome.h"

#if defined(ARDUINO_ARCH_ESP32)
#define TOUCH_IRQ 21
#else
#define TOUCH_IRQ 5
#endif

TFT_eSPI tft = TFT_eSPI();

#if defined(WOKWI_CAP_TOUCH)
Adafruit_FT6206 capTouch = Adafruit_FT6206();
static uint16_t currentTouchX = 0;
static uint16_t currentTouchY = 0;
static bool currentTouchValid = false;

bool readTouchPoint(uint16_t *x, uint16_t *y) {
  if (!currentTouchValid) {
    return false;
  }

  *x = currentTouchX;
  *y = currentTouchY;
  return true;
}

bool isTouching() {
  if (!capTouch.touched()) {
    currentTouchValid = false;
    return false;
  }

  static int16_t lastX = -1;
  static int16_t lastY = -1;
  TS_Point point = capTouch.getPoint();

  int16_t x = map(point.x, 0, 240, 240, 0);
  int16_t y = map(point.y, 0, 320, 320, 0);
  currentTouchX = x;
  currentTouchY = y;
  currentTouchValid = true;

  if (x != lastX || y != lastY) {
    Serial.printf("Touch x=%d y=%d\n", x, y);
    lastX = x;
    lastY = y;
  }

  return true;
}

GameSceneManager manager = GameSceneManager(&tft, TOUCH_IRQ, isTouching);
#else
GameSceneManager manager = GameSceneManager(&tft, TOUCH_IRQ);
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("Start up");

#if defined(WOKWI_CAP_TOUCH)
  Wire.begin();
#endif

  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(0);
  tft.setSwapBytes(true);

#if defined(WOKWI_CAP_TOUCH)
  if (!capTouch.begin(40)) {
    Serial.println("Couldn't start FT6206 touchscreen controller");
  }
#endif

  // Touch screen calibration.
  uint16_t calData[5] = { 316, 3563, 424, 3491, 6 };
  tft.setTouch(calData);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  Scene_GameStart *startGame = new Scene_GameStart(&tft);
  manager.appendScene(startGame); // Scene index = 0
  Serial.println("Start up Completed");
}

void loop() {
  manager.update();
}
