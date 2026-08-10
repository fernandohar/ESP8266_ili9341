#ifndef _PETSIM_H_
#define _PETSIM_H_

#include <Arduino.h>

// One simulation step per minute (on-device tick or offline catch-up).
#define PET_STATUS_TICK_MS 60000

// Hunger decay per minute (0..100 scale).
#define PET_BABY_HUNGER_DECAY_PER_MIN 0.26f
#define PET_ADULT_HUNGER_DECAY_PER_MIN 0.10f
// Cleanness reaches 0 in ~24 h if never bathed.
#define PET_CLEAN_DECAY_PER_MIN (100.0f / (24.0f * 60.0f))
// Excitement proportional decay per minute (fraction of current value).
#define PET_EXCITEMENT_DECAY_FRAC 0.015f

// Happiness tick thresholds (0..100 scale).
#define PET_HAPPINESS_BOOST 10
#define PET_HUNGRY_HAPPY_THRESHOLD 15
#define PET_HUNGRY_HAPPY_PENALTY 0.1f
#define PET_DIRTY_HAPPY_THRESHOLD 10
#define PET_DIRTY_HAPPY_PENALTY 0.1f
#define PET_LONELY_PET_MINUTES (24 * 60)
#define PET_LONELY_HAPPY_PENALTY 0.5f
#define PET_ALL_GOOD_HUNGER 80
#define PET_ALL_GOOD_CLEAN 80
#define PET_ALL_GOOD_EXCITEMENT 80
#define PET_ALL_GOOD_PET_WITHIN_MIN 60

// Mini-game side effects (applied in applyGameReward).
#define PET_GAME_WIN_EXCITEMENT 50
#define PET_GAME_PLAY_HUNGER_COST 8
#define PET_GAME_PLAY_CLEAN_COST 6

// Very low happiness -> Totoro leaves after a grace period.
#define PET_ESCAPE_HAPPINESS 5
#define PET_UNHAPPY_BANNER 15

class PetSim {
  public:
    static void statusUpdateTick();
    // Apply `seconds` of elapsed real time (offline catch-up): one tick per minute.
    static void applyElapsedSeconds(uint32_t seconds);
};

#endif
