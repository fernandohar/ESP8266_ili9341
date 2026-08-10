#include "PetSim.h"
#include "PetTotoroState.h"

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
  if (s.hunger > PET_ALL_GOOD_HUNGER && s.cleanness > PET_ALL_GOOD_CLEAN &&
      s.excitement > PET_ALL_GOOD_EXCITEMENT &&
      PetTotoroState::minutesSinceLastPet() < PET_ALL_GOOD_PET_WITHIN_MIN) {
    applyPositiveFloat((float)PET_HAPPINESS_BOOST, PetTotoroState::happyBoostAccum(),
                       PetTotoroState::adjustHappiness);
  } else {
    if (s.hunger < PET_HUNGRY_HAPPY_THRESHOLD) {
      applyNegativeFloat(PET_HUNGRY_HAPPY_PENALTY, PetTotoroState::happyPenaltyAccum(),
                         PetTotoroState::adjustHappiness);
    }
    if (s.cleanness < PET_DIRTY_HAPPY_THRESHOLD) {
      applyNegativeFloat(PET_DIRTY_HAPPY_PENALTY, PetTotoroState::happyPenaltyAccum(),
                         PetTotoroState::adjustHappiness);
    }
    if (PetTotoroState::minutesSinceLastPet() > PET_LONELY_PET_MINUTES) {
      applyNegativeFloat(PET_LONELY_HAPPY_PENALTY, PetTotoroState::happyPenaltyAccum(),
                         PetTotoroState::adjustHappiness);
    }
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
