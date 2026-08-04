#ifndef _PETSIM_H_
#define _PETSIM_H_

#include <Arduino.h>

// Real-time (wall-clock) decay model, used for "catch-up" when the game boots
// after being powered off. This is only meaningful with an RTC; without one
// (PET_USE_RTC == 0) it is never invoked and powered-time decay in the pet scene
// handles everything.
//
// Rates are expressed per hour on the 0..100 stat scale.
#define PET_DECAY_HUNGER_PER_HOUR 12
#define PET_DECAY_CLEAN_PER_HOUR 8
#define PET_DECAY_HAPPINESS_PER_HOUR 3
// Health drains only while the pet is starving or filthy.
#define PET_DECAY_HEALTH_PER_HOUR 10

class PetSim {
  public:
    // Apply `seconds` of elapsed real time to the pet's stats. The caller is
    // responsible for clamping `seconds` (see PET_OFFLINE_CAP_SECONDS).
    static void applyElapsedSeconds(uint32_t seconds);
};

#endif
