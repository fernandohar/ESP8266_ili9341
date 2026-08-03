#include "PetClock.h"

#if PET_USE_RTC
#include <Wire.h>
#include <RTClib.h>
static RTC_DS3231 rtc;
static bool rtcReady = false;
#endif

void PetClock::begin() {
#if PET_USE_RTC
  Wire.begin();
  rtcReady = rtc.begin();
  // If the RTC lost power its time is invalid; the caller sets it at "birth".
#endif
}

bool PetClock::available() {
#if PET_USE_RTC
  return rtcReady;
#else
  return false;
#endif
}

uint32_t PetClock::nowEpoch() {
#if PET_USE_RTC
  if (rtcReady) {
    return rtc.now().unixtime();
  }
#endif
  return 0;
}

void PetClock::setEpoch(uint32_t epoch) {
#if PET_USE_RTC
  if (rtcReady) {
    rtc.adjust(DateTime(epoch));
  }
#else
  (void)epoch;
#endif
}
