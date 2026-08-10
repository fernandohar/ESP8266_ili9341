#include "PetTotoroState.h"
#include "PetSim.h"

PetTotoroStats PetTotoroState::current = {PET_STAT_MAX, PET_STAT_MAX, PET_STAT_MAX, PET_STAT_MAX, PET_STAT_MAX};
PetLifeState PetTotoroState::lifeState = PET_LIFE_ALIVE;
uint32_t PetTotoroState::careXpValue = 0;
uint32_t PetTotoroState::minutesSinceLastPetValue = 0;
float PetTotoroState::hungerDecayAccumValue = 0;
float PetTotoroState::cleanDecayAccumValue = 0;
float PetTotoroState::excitementDecayAccumValue = 0;
float PetTotoroState::happyBoostAccumValue = 0;
float PetTotoroState::happyPenaltyAccumValue = 0;

void PetTotoroState::reset() {
  current.health = PET_STAT_MAX;
  current.hunger = PET_STAT_MAX;
  current.happiness = PET_STAT_MAX;
  current.cleanness = PET_STAT_MAX;
  current.excitement = PET_STAT_MAX;
  lifeState = PET_LIFE_ALIVE;
  careXpValue = 0;
  minutesSinceLastPetValue = 0;
  hungerDecayAccumValue = 0;
  cleanDecayAccumValue = 0;
  excitementDecayAccumValue = 0;
  happyBoostAccumValue = 0;
  happyPenaltyAccumValue = 0;
}

bool PetTotoroState::isAlive() {
  return lifeState != PET_LIFE_ESCAPED;
}

bool PetTotoroState::isGameOver() {
  return lifeState == PET_LIFE_ESCAPED;
}

bool PetTotoroState::isSick() {
  return isAlive() && current.happiness < PET_UNHAPPY_BANNER;
}

bool PetTotoroState::hasEscaped() {
  return lifeState == PET_LIFE_ESCAPED;
}

PetLifeState PetTotoroState::life() {
  return lifeState;
}

void PetTotoroState::setLife(PetLifeState value) {
  lifeState = value;
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

void PetTotoroState::setExcitement(int value) {
  current.excitement = clampStat(value);
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

void PetTotoroState::adjustExcitement(int delta) {
  setExcitement(current.excitement + delta);
}

int PetTotoroState::excitement() {
  return current.excitement;
}

uint32_t PetTotoroState::minutesSinceLastPet() {
  return minutesSinceLastPetValue;
}

void PetTotoroState::setMinutesSinceLastPet(uint32_t minutes) {
  minutesSinceLastPetValue = minutes;
}

void PetTotoroState::recordPetting() {
  minutesSinceLastPetValue = 0;
}

float &PetTotoroState::hungerDecayAccum() {
  return hungerDecayAccumValue;
}

float &PetTotoroState::cleanDecayAccum() {
  return cleanDecayAccumValue;
}

float &PetTotoroState::excitementDecayAccum() {
  return excitementDecayAccumValue;
}

float &PetTotoroState::happyBoostAccum() {
  return happyBoostAccumValue;
}

float &PetTotoroState::happyPenaltyAccum() {
  return happyPenaltyAccumValue;
}

void PetTotoroState::tickMinutesSinceLastPet() {
  if (minutesSinceLastPetValue < 0xFFFFFFFF) {
    minutesSinceLastPetValue++;
  }
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
