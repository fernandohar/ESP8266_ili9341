#ifndef _TOUCHINPUT_H_
#define _TOUCHINPUT_H_

#include <Arduino.h>
#include <TFT_eSPI.h>

// Defined in main.cpp for both the Wokwi (FT6206) and real-hardware (XPT2046) builds.
// isTouching() polls + caches the point each frame; this returns the cached value.
extern bool readTouchPoint(uint16_t *x, uint16_t *y);

inline bool getTouchPoint(TFT_eSPI *tft, uint16_t *touchX, uint16_t *touchY) {
  (void)tft;
  return readTouchPoint(touchX, touchY);
}

#endif
