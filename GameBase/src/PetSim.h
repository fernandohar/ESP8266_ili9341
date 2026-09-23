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

// Baseline happiness drift, applied every tick so care has to be repeated
// instead of banked once. It is a fraction of the square root of what is left,
// which makes the slide steepest while the pet is still happy and gentler as it
// nears the bottom (h traces a (1 - t/T)^2 curve) while, unlike a plain
// proportional decay, still reaching 0 in finite time.
//
// The rate is calibrated so an untouched pet bottoms out after ~48 h - a baby
// at 48.0 h, an adult at 48.8 h, since it gets hungry more slowly. Escaping
// hangs off that same 0, so losing the Totoro costs two days of total neglect.
#define PET_HAPPINESS_DECAY_RATE 0.0052f

// Each unmet need steepens that same slide by an eighth rather than adding a
// flat penalty, which near the bottom would dwarf the drift and straighten the
// curve back out.
#define PET_NEED_HAPPY_RATE (PET_HAPPINESS_DECAY_RATE / 8.0f)

// Happiness tick thresholds (0..100 scale). The boost tops the pet up while
// every stat is high; keep it small so it counteracts the drift rather than
// snapping straight back to the PET_STAT_MAX clamp.
#define PET_HAPPINESS_BOOST 1
#define PET_HUNGRY_HAPPY_THRESHOLD 15
#define PET_DIRTY_HAPPY_THRESHOLD 10
#define PET_LONELY_PET_MINUTES (24 * 60)
#define PET_ALL_GOOD_HUNGER 80
#define PET_ALL_GOOD_CLEAN 80
#define PET_ALL_GOOD_EXCITEMENT 80
#define PET_ALL_GOOD_PET_WITHIN_MIN 60

// Mini-game side effects (applied in applyGameReward).
#define PET_GAME_WIN_EXCITEMENT 5
#define PET_GAME_LOSE_EXCITEMENT 10
#define PET_GAME_CASUAL_EXCITEMENT 1   // 2P Tic-Tac-Toe (no win/lose verdict)
#define PET_GAME_PLAY_HUNGER_COST 8
#define PET_GAME_PLAY_CLEAN_COST 6

// Totoro leaves once happiness has bottomed out and stayed there for the grace
// period; the banner warns well before that (~34 h into a neglect run).
#define PET_ESCAPE_HAPPINESS 0
#define PET_UNHAPPY_BANNER 15

class PetSim {
  public:
    static void statusUpdateTick();
    // Apply `seconds` of elapsed real time (offline catch-up): one tick per minute.
    static void applyElapsedSeconds(uint32_t seconds);
    // Advance the simulation off a free-running millis() clock. Called every game
    // tick from the scene manager, so minutes accrue no matter which scene is on
    // screen or whether a modal menu is open.
    static void service(unsigned long nowMs);
};

#endif
