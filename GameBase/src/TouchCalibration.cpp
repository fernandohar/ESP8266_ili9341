#include "TouchCalibration.h"

// Sanity marker so we never load a half-written / uninitialised blob.
static const uint16_t CAL_MAGIC = 0xCA1B;

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>

static const char *NVS_NAMESPACE = "touchcal";
static const char *NVS_KEY_DATA = "cal";
static const char *NVS_KEY_MAGIC = "magic";

bool TouchCalibration::load(uint16_t cal[5]) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) {
    return false;
  }

  bool ok = false;
  if (prefs.getUShort(NVS_KEY_MAGIC, 0) == CAL_MAGIC &&
      prefs.getBytesLength(NVS_KEY_DATA) == sizeof(uint16_t) * 5) {
    prefs.getBytes(NVS_KEY_DATA, cal, sizeof(uint16_t) * 5);
    ok = true;
  }

  prefs.end();
  return ok;
}

void TouchCalibration::save(const uint16_t cal[5]) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
    return;
  }
  prefs.putBytes(NVS_KEY_DATA, cal, sizeof(uint16_t) * 5);
  prefs.putUShort(NVS_KEY_MAGIC, CAL_MAGIC);
  prefs.end();
}
#else
// Non-ESP32 targets have no NVS here; calibration is not persisted.
bool TouchCalibration::load(uint16_t cal[5]) {
  (void)cal;
  return false;
}

void TouchCalibration::save(const uint16_t cal[5]) {
  (void)cal;
}
#endif

void TouchCalibration::run(TFT_eSPI *tft, uint16_t cal[5]) {
  int16_t w = tft->width();
  int16_t h = tft->height();

  tft->fillScreen(TFT_BLACK);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("Touch Calibration", w / 2, h / 2 - 30, 4);
  tft->drawString("Tap each corner arrow", w / 2, h / 2 + 10, 2);
  tft->drawString("as it appears", w / 2, h / 2 + 30, 2);
  delay(1500);

  tft->fillScreen(TFT_BLACK);
  tft->calibrateTouch(cal, TFT_MAGENTA, TFT_BLACK, 15);

  tft->setTouch(cal);
  save(cal);

  tft->fillScreen(TFT_BLACK);
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("Calibration saved!", w / 2, h / 2, 4);
  delay(1200);

  tft->setTextDatum(TL_DATUM);
}
