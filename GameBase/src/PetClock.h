#ifndef _PETCLOCK_H_
#define _PETCLOCK_H_

#include <Arduino.h>

// Set to 1 once a DS3231 RTC module is wired on I2C. While this is 0 there is
// no real-time source: offline / wall-clock decay is disabled and the game
// relies solely on powered-time decay. Flipping this to 1 also requires adding
// the RTClib dependency (see the note in platformio.ini) and DS3231 wiring.
#define PET_USE_RTC 0

// Never age/starve a pet by more than this many seconds of offline time in one
// catch-up, so a long unplug (or a clock jump) can't wipe it out instantly.
#define PET_OFFLINE_CAP_SECONDS (48UL * 3600UL)

class PetClock {
  public:
    static void begin();
    static bool available();        // true only when an RTC is present & running
    static uint32_t nowEpoch();     // unix seconds, or 0 when unavailable
    static void setEpoch(uint32_t epoch);
};

#endif
