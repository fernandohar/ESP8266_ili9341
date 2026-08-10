#ifndef _PETTOTOROSTATE_H_
#define _PETTOTOROSTATE_H_

#include <Arduino.h>

// Stats are tracked on a 0..100 scale for fine-grained simulation (decay,
// food/pet deltas of varying strength). The on-screen HUD renders them as a
// small number of pips, where one pip = PET_STAT_PER_PIP units.
#define PET_STAT_MAX 100
#define PET_STAT_MIN 0
#define PET_STAT_PIPS 4
#define PET_STAT_PER_PIP (PET_STAT_MAX / PET_STAT_PIPS)

#define PET_STAGE_ADULT_XP 150

enum PetLifeState {
  PET_LIFE_ALIVE = 0,
  PET_LIFE_SICK = 1,     // legacy slot; unused in the happiness model
  PET_LIFE_ESCAPED = 2
};

enum PetStage {
  PET_STAGE_BABY = 0,
  PET_STAGE_ADULT = 1
};

struct PetTotoroStats {
  int health;       // legacy (kept for old saves; not used in care sim)
  int hunger;
  int happiness;
  int cleanness;
  int excitement;
};

class PetTotoroState {
  public:
    static void reset();

    static bool isAlive();
    static bool isGameOver();
    static bool isSick();        // true when very unhappy (UI banner)
    static bool hasEscaped();
    static PetLifeState life();
    static void setLife(PetLifeState value);

    static const PetTotoroStats &stats();

    static void setHealth(int value);
    static void setHunger(int value);
    static void setHappiness(int value);
    static void setCleanness(int value);
    static void setExcitement(int value);

    static void adjustHealth(int delta);
    static void adjustHunger(int delta);
    static void adjustHappiness(int delta);
    static void adjustCleanness(int delta);
    static void adjustExcitement(int delta);

    static int excitement();
    static uint32_t minutesSinceLastPet();
    static void setMinutesSinceLastPet(uint32_t minutes);
    static void recordPetting();

    // Fractional decay accumulators (PetSim statusUpdateTick).
    static float &hungerDecayAccum();
    static float &cleanDecayAccum();
    static float &excitementDecayAccum();
    static float &happyBoostAccum();
    static float &happyPenaltyAccum();
    static void tickMinutesSinceLastPet();

    static uint32_t careXP();
    static void setCareXP(uint32_t value);
    static void addCareXP(uint32_t amount);
    static int stage();

    static void reviveIfNeeded();

  private:
    static int clampStat(int value);
    static PetTotoroStats current;
    static PetLifeState lifeState;
    static uint32_t careXpValue;
    static uint32_t minutesSinceLastPetValue;
    static float hungerDecayAccumValue;
    static float cleanDecayAccumValue;
    static float excitementDecayAccumValue;
    static float happyBoostAccumValue;
    static float happyPenaltyAccumValue;
};

#endif
