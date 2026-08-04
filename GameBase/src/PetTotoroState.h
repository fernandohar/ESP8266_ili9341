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

// Care-based growth: careXP only ever grows (feeding, petting, cleaning, game
// wins) and drives the visible stage. Decoupled from the clock so a wrong/reset
// RTC can never rewind the pet's life.
#define PET_STAGE_JUNIOR_XP 150
#define PET_STAGE_ADULT_XP 500

enum PetLifeState {
  PET_LIFE_ALIVE = 0,
  PET_LIFE_SICK = 1,     // health bottomed out; recoverable grace period
  PET_LIFE_ESCAPED = 2   // neglected too long; gone until a factory reset
};

enum PetStage {
  PET_STAGE_BABY = 0,
  PET_STAGE_JUNIOR = 1,
  PET_STAGE_ADULT = 2
};

struct PetTotoroStats {
  int health;
  int hunger;
  int happiness;
  int cleanness;
};

class PetTotoroState {
  public:
    static void reset();

    static bool isAlive();       // present (not escaped)
    static bool isGameOver();    // escaped
    static bool isSick();
    static bool hasEscaped();
    static PetLifeState life();
    static void setLife(PetLifeState value);

    static const PetTotoroStats &stats();

    static void setHealth(int value);
    static void setHunger(int value);
    static void setHappiness(int value);
    static void setCleanness(int value);

    static void adjustHealth(int delta);
    static void adjustHunger(int delta);
    static void adjustHappiness(int delta);
    static void adjustCleanness(int delta);

    // Care-based growth.
    static uint32_t careXP();
    static void setCareXP(uint32_t value);
    static void addCareXP(uint32_t amount);
    static int stage();          // PET_STAGE_*

    static void reviveIfNeeded();

  private:
    static int clampStat(int value);
    static PetTotoroStats current;
    static PetLifeState lifeState;
    static uint32_t careXpValue;
};

#endif
