#include "PetTotoroState.h"

PetTotoroStats PetTotoroState::current = {PET_STAT_MAX, PET_STAT_MAX, PET_STAT_MAX, PET_STAT_MAX};
bool PetTotoroState::alive = true;

void PetTotoroState::reset() {
  current.health = PET_STAT_MAX;
  current.hunger = PET_STAT_MAX;
  current.happiness = PET_STAT_MAX;
  current.cleanness = PET_STAT_MAX;
  alive = true;
}

bool PetTotoroState::isAlive() {
  return alive;
}

bool PetTotoroState::isGameOver() {
  return !alive;
}

const PetTotoroStats &PetTotoroState::stats() {
  return current;
}

int PetTotoroState::clampStat(int value) {
  if (value < PET_STAT_MIN) {
    return PET_STAT_MIN;
  }
  if (value > PET_STAT_MAX) {
    return PET_STAT_MAX;
  }
  return value;
}

void PetTotoroState::setHealth(int value) {
  current.health = clampStat(value);
}

void PetTotoroState::setHunger(int value) {
  current.hunger = clampStat(value);
}

void PetTotoroState::setHappiness(int value) {
  current.happiness = clampStat(value);
}

void PetTotoroState::setCleanness(int value) {
  current.cleanness = clampStat(value);
}

void PetTotoroState::adjustHealth(int delta) {
  setHealth(current.health + delta);
}

void PetTotoroState::adjustHunger(int delta) {
  setHunger(current.hunger + delta);
}

void PetTotoroState::adjustHappiness(int delta) {
  setHappiness(current.happiness + delta);
}

void PetTotoroState::adjustCleanness(int delta) {
  setCleanness(current.cleanness + delta);
}

void PetTotoroState::markDead() {
  alive = false;
  current.health = PET_STAT_MIN;
}

void PetTotoroState::reviveIfNeeded() {
  if (!alive) {
    reset();
  }
}
