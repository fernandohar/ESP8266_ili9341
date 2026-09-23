#include "PetSave.h"
#include "PetTotoroState.h"
#include "GameProgress.h"
#include "PetClock.h"

// Bumped whenever the stored layout changes so stale/foreign blobs are ignored.
static const uint16_t PET_SAVE_MAGIC = 0x7003;

// Cached real-time bookkeeping. Mirrored to NVS so it survives reboots; kept in
// RAM too so callers (boot-time catch-up) can read it without reopening NVS.
static uint32_t s_lastSeenEpoch = 0;
static uint32_t s_bornEpoch = 0;

uint32_t PetSave::lastSeenEpoch() {
  return s_lastSeenEpoch;
}

uint32_t PetSave::bornEpoch() {
  return s_bornEpoch;
}

void PetSave::setLastSeenEpoch(uint32_t epoch) {
  s_lastSeenEpoch = epoch;
}

void PetSave::setBornEpoch(uint32_t epoch) {
  s_bornEpoch = epoch;
}

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>

static const char *NVS_NAMESPACE = "petsave";

bool PetSave::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) {
    return false;
  }

  bool ok = false;
  const uint16_t magic = prefs.getUShort("magic", 0);
  if (magic == PET_SAVE_MAGIC || magic == 0x7002) {
    PetTotoroState::setHealth(prefs.getInt("hp", PET_STAT_MAX));
    PetTotoroState::setHunger(prefs.getInt("hu", PET_STAT_MAX));
    PetTotoroState::setHappiness(prefs.getInt("ha", PET_STAT_MAX));
    PetTotoroState::setCleanness(prefs.getInt("cl", PET_STAT_MAX));
    if (magic == PET_SAVE_MAGIC) {
      PetTotoroState::setExcitement(prefs.getInt("ex", PET_STAT_MAX));
      PetTotoroState::setMinutesSinceLastPet(prefs.getUInt("petMin", 0));
    }
    GameProgress::setCoins(prefs.getInt("coins", 0));
    PetTotoroState::setCareXP(prefs.getUInt("xp", 0));
    PetTotoroState::setLife((PetLifeState)prefs.getUChar("life", PET_LIFE_ALIVE));
    s_lastSeenEpoch = prefs.getUInt("seen", 0);
    s_bornEpoch = prefs.getUInt("born", 0);
    ok = true;
  }

  prefs.end();
  return ok;
}

void PetSave::save() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
    return;
  }

  // Stamp "last seen" with the current wall-clock so a later boot can measure
  // offline time. No-op (stays 0) without an RTC.
  if (PetClock::available()) {
    s_lastSeenEpoch = PetClock::nowEpoch();
  }

  const PetTotoroStats &s = PetTotoroState::stats();
  prefs.putInt("hp", s.health);
  prefs.putInt("hu", s.hunger);
  prefs.putInt("ha", s.happiness);
  prefs.putInt("cl", s.cleanness);
  prefs.putInt("ex", s.excitement);
  prefs.putUInt("petMin", PetTotoroState::minutesSinceLastPet());
  prefs.putInt("coins", GameProgress::getCoins());
  prefs.putUInt("xp", PetTotoroState::careXP());
  prefs.putUChar("life", (uint8_t)PetTotoroState::life());
  prefs.putUInt("seen", s_lastSeenEpoch);
  prefs.putUInt("born", s_bornEpoch);
  prefs.putUShort("magic", PET_SAVE_MAGIC);
  prefs.end();
}

void PetSave::wipe() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
    prefs.clear();
    prefs.end();
  }
  s_lastSeenEpoch = 0;
  s_bornEpoch = 0;
  PetTotoroState::reset();
  GameProgress::resetCoins();
}
#else
bool PetSave::load() {
  return false;
}

void PetSave::save() {
}

void PetSave::wipe() {
  s_lastSeenEpoch = 0;
  s_bornEpoch = 0;
  PetTotoroState::reset();
  GameProgress::resetCoins();
}
#endif
