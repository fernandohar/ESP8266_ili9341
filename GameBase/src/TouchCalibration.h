#ifndef _TOUCHCALIBRATION_H_
#define _TOUCHCALIBRATION_H_

#include <Arduino.h>
#include <TFT_eSPI.h>

// Persists and runs XPT2046 (resistive) touch calibration.
// Calibration is stored in ESP32 NVS so it survives reboots.
class TouchCalibration {
  public:
    // Loads saved calibration into cal[5]. Returns true when valid data exists.
    static bool load(uint16_t cal[5]);

    // Persists calibration data.
    static void save(const uint16_t cal[5]);

    // Runs the interactive corner-tap calibration (blocking). Stores the result
    // in cal[5], applies it to the panel via setTouch(), and saves it to NVS.
    static void run(TFT_eSPI *tft, uint16_t cal[5]);
};

#endif
