#include "PetSim.h"
#include "PetTotoroState.h"

static int decayFor(uint32_t seconds, int perHour) {
  // Round to nearest unit so short gaps still register some decay.
  return (int)(((uint64_t)seconds * perHour + 1800UL) / 3600UL);
}

void PetSim::applyElapsedSeconds(uint32_t seconds) {
  if (seconds == 0) {
    return;
  }

  PetTotoroState::adjustHunger(-decayFor(seconds, PET_DECAY_HUNGER_PER_HOUR));
  PetTotoroState::adjustCleanness(-decayFor(seconds, PET_DECAY_CLEAN_PER_HOUR));
  PetTotoroState::adjustHappiness(-decayFor(seconds, PET_DECAY_HAPPINESS_PER_HOUR));

  const PetTotoroStats &s = PetTotoroState::stats();
  if (s.hunger <= PET_STAT_MIN || s.cleanness <= PET_STAT_MIN) {
    PetTotoroState::adjustHealth(-decayFor(seconds, PET_DECAY_HEALTH_PER_HOUR));
  }
}
