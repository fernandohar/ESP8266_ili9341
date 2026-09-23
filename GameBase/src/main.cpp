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
#include "GameSceneIds.h"
#include "Input.h"
#include "SoundPlayer.h"
#include "Scene_PetTotoro.h"
#include "Scene_AcornCatch.h"
#include "Scene_Settings.h"
#include "Scene_TicTacToe.h"
#include "Scene_WhackAMole.h"
#include "Scene_Status.h"
#include "Scene_Grocery.h"
#include "Scene_CatBusCross.h"
#include "Scene_CoinReward.h"
#include "Scene_SlidePuzzle.h"
#include "Scene_Klotski.h"
#include "Scene_ConnectFour.h"
#if defined(TINYML_GESTURE_LOG)
#include "Scene_GestureCapture.h"
#endif
#include "TouchCalibration.h"
#include "PetSave.h"
#include "PetClock.h"
#include "PetSim.h"

#if defined(TINYML_DATA_LOG)
#include "ml/MLDataLogger.h"
#endif

#if defined(ARDUINO_ARCH_ESP32)
#define TOUCH_IRQ 21
#else
#define TOUCH_IRQ 5
#endif

// IMPORTANT: TFT_eSPI (and the scene manager that holds it) must NOT be created at
// global/static scope. The TFT_eSPI constructor calls pinMode()/digitalWrite(), which
// crash on ESP32 when executed during C++ static initialisation (before the Arduino
// core is ready), producing a rst:0xc (SW_CPU_RESET) boot loop. They are constructed
// inside setup() instead.
TFT_eSPI *tft = NULL;
GameSceneManager *manager = NULL;

#if defined(WOKWI_CAP_TOUCH)
Adafruit_FT6206 capTouch = Adafruit_FT6206();
static uint16_t currentTouchX = 0;
static uint16_t currentTouchY = 0;
static bool currentTouchValid = false;
static bool capTouchReady = false;

bool readTouchPoint(uint16_t *x, uint16_t *y) {
  if (!currentTouchValid) {
    return false;
  }

  *x = currentTouchX;
  *y = currentTouchY;
  return true;
}

bool isTouching() {
  // If the FT6206 wasn't detected, skip polling so we don't spam I2C errors
  // and stall the game loop every frame.
  if (!capTouchReady) {
    return false;
  }

  if (!capTouch.touched()) {
    currentTouchValid = false;
    return false;
  }

  TS_Point point = capTouch.getPoint();

  int16_t x = map(point.x, 0, 240, 240, 0);
  int16_t y = map(point.y, 0, 320, 320, 0);
  currentTouchX = x;
  currentTouchY = y;
  currentTouchValid = true;

  return true;
}

// The capacitive controller is already a single fast I2C read, so the gesture
// sampler shares the ordinary path and has no guards to report on.
bool sampleTouchFast(uint16_t *x, uint16_t *y) {
  if (!isTouching()) {
    return false;
  }
  return readTouchPoint(x, y);
}

void touchFastResetStats() {}

void touchFastStats(uint16_t *pressureDrops, uint16_t *jitterDrops) {
  *pressureDrops = 0;
  *jitterDrops = 0;
}
#else
// Real hardware: XPT2046 resistive touch on the shared SPI bus. TFT_eSPI polls the
// controller directly (no IRQ pin needed), so we read + cache the point once per frame.
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
  if (tft == NULL) {
    return false;
  }

  uint16_t x = 0, y = 0;
  if (!tft->getTouch(&x, &y)) {
    currentTouchValid = false;
    return false;
  }

  currentTouchX = x;
  currentTouchY = y;
  currentTouchValid = true;

  return true;
}

// Delay-free single-shot read for the gesture sampler. TFT_eSPI's getTouch()
// averages five validTouch() passes, each with its own settling delay()s, so one
// call costs tens of milliseconds - far too slow to sample a stroke at 60 Hz.
// This reads pressure once, takes two back-to-back positions, averages them, and
// rejects the pair only if they disagree wildly.
//
// The first capture session showed half the reads being rejected here, which
// halved the effective sample rate, so both guards are deliberately loose:
//
//  - The deadband is in raw ADC counts. The panel spans ~3300 counts over 240 px,
//    so ~14 counts per pixel; 120 counts is ~8 px of tolerated disagreement.
//    Back-to-back raw reads are noisier than TFT_eSPI's, because it lets the
//    panel settle between samples and this cannot afford to.
//  - Losing pressure keeps the contact "engaged" for the same window the
//    segmenter uses to end a stroke. Dropping it on the first dip meant a
//    momentary light spot mid-brush demanded a full re-press to resume.
#define TOUCH_FAST_Z_ENGAGE 600
#define TOUCH_FAST_Z_SUSTAIN 120
#define TOUCH_FAST_DEADBAND 120
#define TOUCH_FAST_ENGAGE_HOLD_MS 48

static bool fastTouchEngaged = false;
static unsigned long fastTouchLastAcceptMs = 0;
static uint16_t fastTouchRejectZ = 0;
static uint16_t fastTouchRejectJitter = 0;

// Rejections that happened while a stroke was in progress, so a future capture
// session reports which guard is costing samples instead of needing a guess.
void touchFastResetStats() {
  fastTouchRejectZ = 0;
  fastTouchRejectJitter = 0;
}

void touchFastStats(uint16_t *pressureDrops, uint16_t *jitterDrops) {
  *pressureDrops = fastTouchRejectZ;
  *jitterDrops = fastTouchRejectJitter;
}

bool sampleTouchFast(uint16_t *x, uint16_t *y) {
  if (tft == NULL) {
    return false;
  }

  unsigned long now = millis();

  // Hysteresis: a firm press starts a stroke, a lighter one sustains it, so
  // pressure dips mid-brush don't chop one stroke into several.
  uint16_t z = tft->getTouchRawZ();
  if (z <= (fastTouchEngaged ? TOUCH_FAST_Z_SUSTAIN : TOUCH_FAST_Z_ENGAGE)) {
    if (fastTouchEngaged) {
      if ((now - fastTouchLastAcceptMs) > TOUCH_FAST_ENGAGE_HOLD_MS) {
        fastTouchEngaged = false;
      } else {
        fastTouchRejectZ++;
      }
    }
    return false;
  }

  uint16_t rawX1 = 0, rawY1 = 0, rawX2 = 0, rawY2 = 0;
  tft->getTouchRaw(&rawX1, &rawY1);
  tft->getTouchRaw(&rawX2, &rawY2);
  if (abs((int)rawX1 - (int)rawX2) > TOUCH_FAST_DEADBAND ||
      abs((int)rawY1 - (int)rawY2) > TOUCH_FAST_DEADBAND) {
    if (fastTouchEngaged) {
      fastTouchRejectJitter++;
    }
    return false;
  }

  // Averaging the pair costs nothing and halves the read noise.
  uint16_t screenX = (uint16_t)(((uint32_t)rawX1 + rawX2) / 2);
  uint16_t screenY = (uint16_t)(((uint32_t)rawY1 + rawY2) / 2);
  tft->convertRawXY(&screenX, &screenY);
  // convertRawXY subtracts the calibration origin in unsigned arithmetic, so an
  // off-panel read wraps to a huge value rather than going negative.
  if (screenX >= SCREENWIDTH || screenY >= SCREENHEIGHT) {
    return false;
  }

  fastTouchEngaged = true;
  fastTouchLastAcceptMs = now;
  *x = screenX;
  *y = screenY;
  return true;
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("Start up");
#if defined(TINYML_DATA_LOG)
  MLDataLogger::printCsvHeader();
  Serial.println("TinyML data logging enabled (TINYML_DATA_LOG)");
#endif

#if defined(WOKWI_CAP_TOUCH)
  Wire.begin();
#endif

  tft = new TFT_eSPI();
  tft->begin();
  tft->fillScreen(ILI9341_BLACK);
  tft->setRotation(0);
  tft->setSwapBytes(true);

#if defined(WOKWI_CAP_TOUCH)
  capTouchReady = capTouch.begin(40);
  if (!capTouchReady) {
    Serial.println("FT6206 not found - touch input disabled");
  }
#endif

  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  SoundPlayer::begin(SPEAKER_PIN);
  Input::begin();

#if defined(WOKWI_CAP_TOUCH)
  uint16_t calData[5] = { 316, 3563, 424, 3491, 6 };
  tft->setTouch(calData);
#else
  // Load saved calibration; (re)calibrate if none exists, or if Left+Right are
  // held at boot. The boot-combo is a hardware escape hatch: it works even when
  // touch is so miscalibrated that the on-screen button can't be tapped.
  uint16_t calData[5] = { 316, 3563, 424, 3491, 6 };
  bool haveCal = TouchCalibration::load(calData);

  Input::update();
  const GameInput &bootInput = Input::current();
  bool forceCal = bootInput.left && bootInput.right;

  if (!haveCal || forceCal) {
    TouchCalibration::run(tft, calData);
  } else {
    tft->setTouch(calData);
  }
#endif

  // Restore saved coins + pet stats before any scene initialises so the pet
  // home draws with the persisted state.
  PetSave::load();

  // Real-time clock + offline decay catch-up. With no RTC wired
  // (PET_USE_RTC == 0) PetClock::available() is always false, so this whole
  // block is skipped and the pet only decays while powered on. Once a DS3231 is
  // wired and PET_USE_RTC is set to 1, boots apply a capped amount of the time
  // that elapsed while the device was off.
  PetClock::begin();
  if (PetClock::available()) {
    uint32_t nowEpoch = PetClock::nowEpoch();
    uint32_t lastSeen = PetSave::lastSeenEpoch();
    if (PetSave::bornEpoch() == 0) {
      PetSave::setBornEpoch(nowEpoch);
    }
    if (lastSeen != 0 && nowEpoch > lastSeen && PetTotoroState::isAlive()) {
      uint32_t elapsed = nowEpoch - lastSeen;
      if (elapsed > PET_OFFLINE_CAP_SECONDS) {
        elapsed = PET_OFFLINE_CAP_SECONDS;
      }
      PetSim::applyElapsedSeconds(elapsed);
    }
    PetSave::setLastSeenEpoch(nowEpoch);
  }

  manager = new GameSceneManager(tft, TOUCH_IRQ, isTouching);

  // The pet's forest home is scene 0 and the central hub of the game. Mini-games
  // and Settings are reached from the radial menu (tap Totoro) and return here.
  manager->appendScene(new Scene_PetTotoro(tft));           // 0 - Pet Totoro
  manager->appendScene(new Scene_AcornCatch(tft));          // 1 - Acorn Catch
  manager->appendScene(new Scene_Settings(tft));            // 2 - Settings
  manager->appendScene(new Scene_TicTacToe(tft));           // 3 - Tic Tac Toe
  manager->appendScene(new Scene_WhackAMole(tft));          // 4 - Whack-a-Mole
  manager->appendScene(new Scene_Status(tft));              // 5 - Status
  manager->appendScene(new Scene_Grocery(tft));             // 6 - Grocery (Eat)
  manager->appendScene(new Scene_CatBusCross(tft));         // 7 - Cat Bus Cross
  manager->appendScene(new Scene_CoinReward(tft));          // 8 - Coin reward
  manager->appendScene(new Scene_SlidePuzzle(tft));         // 9 - Slide Puzzle
  manager->appendScene(new Scene_Klotski(tft));             // 10 - Klotski
  manager->appendScene(new Scene_ConnectFour(tft));         // 11 - Four in a Row
#if defined(TINYML_GESTURE_LOG)
  manager->appendScene(new Scene_GestureCapture(tft));      // 12 - Gesture capture
#endif

  manager->startScene(SCENE_PET_TOTORO);

  Serial.println("Start up Completed");
#if !defined(WOKWI_CAP_TOUCH)
  Serial.println("Touch recovery: send 'c' over serial to recalibrate, or hold Left+Right at boot.");
#endif
}

void loop() {
#if !defined(WOKWI_CAP_TOUCH)
  // Anti-lockout escape hatch: recalibrate on demand over USB serial, even if
  // touch is unusable and no buttons are wired. Reboot afterwards so the current
  // scene redraws cleanly with the freshly loaded calibration.
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 'C') {
      uint16_t cal[5];
      TouchCalibration::run(tft, cal);
      ESP.restart();
    }
  }
#endif

  // Input is polled inside manager->update() on the fixed game tick so that
  // edge-triggered presses (e.g. Home) aren't cleared before a scene reads them.
  manager->update();
}
