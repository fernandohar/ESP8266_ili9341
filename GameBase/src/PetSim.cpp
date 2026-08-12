#include "PetSim.h"
#include "PetTotoroState.h"

#include <math.h>

static void applyNegativeFloat(float amount, float &accum, void (*applyInt)(int)) {
  if (amount <= 0.0f) {
    return;
  }
  accum += amount;
  while (accum >= 1.0f) {
    applyInt(-1);
    accum -= 1.0f;
  }
}

static void applyPositiveFloat(float amount, float &accum, void (*applyInt)(int)) {
  if (amount <= 0.0f) {
    return;
  }
  accum += amount;
  while (accum >= 1.0f) {
    applyInt(1);
    accum -= 1.0f;
  }
}

void PetSim::statusUpdateTick() {
  if (!PetTotoroState::isAlive()) {
    return;
  }

  const bool isBaby = (PetTotoroState::stage() == PET_STAGE_BABY);
  const float hungerDecay = isBaby ? PET_BABY_HUNGER_DECAY_PER_MIN : PET_ADULT_HUNGER_DECAY_PER_MIN;

  applyNegativeFloat(hungerDecay, PetTotoroState::hungerDecayAccum(),
                     PetTotoroState::adjustHunger);
  applyNegativeFloat(PET_CLEAN_DECAY_PER_MIN, PetTotoroState::cleanDecayAccum(),
                     PetTotoroState::adjustCleanness);

  int excitement = PetTotoroState::excitement();
  if (excitement > 0) {
    float drop = (float)excitement * PET_EXCITEMENT_DECAY_FRAC;
    applyNegativeFloat(drop, PetTotoroState::excitementDecayAccum(),
                       PetTotoroState::adjustExcitement);
  }

  PetTotoroState::tickMinutesSinceLastPet();

  const PetTotoroStats &s = PetTotoroState::stats();
  if (s.happiness > PET_STAT_MIN) {
    float rate = PET_HAPPINESS_DECAY_RATE;
    if (s.hunger < PET_HUNGRY_HAPPY_THRESHOLD) {
      rate += PET_NEED_HAPPY_RATE;
    }
    if (s.cleanness < PET_DIRTY_HAPPY_THRESHOLD) {
      rate += PET_NEED_HAPPY_RATE;
    }
    if (PetTotoroState::minutesSinceLastPet() > PET_LONELY_PET_MINUTES) {
      rate += PET_NEED_HAPPY_RATE;
    }
    applyNegativeFloat(sqrtf((float)s.happiness) * rate,
                       PetTotoroState::happyDecayAccum(),
                       PetTotoroState::adjustHappiness);
  }

  if (s.hunger > PET_ALL_GOOD_HUNGER && s.cleanness > PET_ALL_GOOD_CLEAN &&
      s.excitement > PET_ALL_GOOD_EXCITEMENT &&
      PetTotoroState::minutesSinceLastPet() < PET_ALL_GOOD_PET_WITHIN_MIN) {
    applyPositiveFloat((float)PET_HAPPINESS_BOOST, PetTotoroState::happyBoostAccum(),
                       PetTotoroState::adjustHappiness);
  }
}

void PetSim::applyElapsedSeconds(uint32_t seconds) {
  if (seconds == 0) {
    return;
  }
  uint32_t minutes = seconds / 60;
  for (uint32_t i = 0; i < minutes; i++) {
    statusUpdateTick();
  }
}

static unsigned long s_nextTickMs = 0;
static bool s_clockRunning = false;

void PetSim::service(unsigned long nowMs) {
  if (!s_clockRunning) {
    s_nextTickMs = nowMs + PET_STATUS_TICK_MS;
    s_clockRunning = true;
    return;
  }
  // Signed difference so the millis() rollover at ~49 days does not stall the
  // clock. The deadline advances by whole ticks, so a slow frame catches up.
  while ((int32_t)(nowMs - s_nextTickMs) >= 0) {
    s_nextTickMs += PET_STATUS_TICK_MS;
    statusUpdateTick();
  }
}
