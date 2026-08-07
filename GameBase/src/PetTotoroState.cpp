#include "PetTotoroState.h"

PetTotoroStats PetTotoroState::current = {PET_STAT_MAX, PET_STAT_MAX, PET_STAT_MAX, PET_STAT_MAX};
PetLifeState PetTotoroState::lifeState = PET_LIFE_ALIVE;
uint32_t PetTotoroState::careXpValue = 0;

void PetTotoroState::reset() {
  current.health = PET_STAT_MAX;
  current.hunger = PET_STAT_MAX;
  current.happiness = PET_STAT_MAX;
  current.cleanness = PET_STAT_MAX;
  lifeState = PET_LIFE_ALIVE;
  careXpValue = 0;
}

bool PetTotoroState::isAlive() {
  return lifeState != PET_LIFE_ESCAPED;
}

bool PetTotoroState::isGameOver() {
  return lifeState == PET_LIFE_ESCAPED;
}

bool PetTotoroState::isSick() {
  return lifeState == PET_LIFE_SICK;
}

bool PetTotoroState::hasEscaped() {
  return lifeState == PET_LIFE_ESCAPED;
}

PetLifeState PetTotoroState::life() {
  return lifeState;
}

void PetTotoroState::setLife(PetLifeState value) {
  lifeState = value;
  if (value == PET_LIFE_ESCAPED) {
    current.health = PET_STAT_MIN;
  }
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

uint32_t PetTotoroState::careXP() {
  return careXpValue;
}

void PetTotoroState::setCareXP(uint32_t value) {
  careXpValue = value;
}

void PetTotoroState::addCareXP(uint32_t amount) {
  careXpValue += amount;
}

int PetTotoroState::stage() {
  return (careXpValue >= PET_STAGE_ADULT_XP) ? PET_STAGE_ADULT : PET_STAGE_BABY;
}

void PetTotoroState::reviveIfNeeded() {
  if (lifeState == PET_LIFE_ESCAPED) {
    reset();
  }
}
