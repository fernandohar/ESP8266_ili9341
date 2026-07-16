#ifndef _PETTOTOROSTATE_H_
#define _PETTOTOROSTATE_H_

#include <Arduino.h>

#define PET_STAT_MAX 4
#define PET_STAT_MIN 0

struct PetTotoroStats {
  int health;
  int hunger;
  int happiness;
  int cleanness;
};

class PetTotoroState {
  public:
    static void reset();
    static bool isAlive();
    static bool isGameOver();
    static const PetTotoroStats &stats();

    static void setHealth(int value);
    static void setHunger(int value);
    static void setHappiness(int value);
    static void setCleanness(int value);

    static void adjustHealth(int delta);
    static void adjustHunger(int delta);
    static void adjustHappiness(int delta);
    static void adjustCleanness(int delta);

    static void markDead();
    static void reviveIfNeeded();

  private:
    static int clampStat(int value);
    static PetTotoroStats current;
    static bool alive;
};

#endif
