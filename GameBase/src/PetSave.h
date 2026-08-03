#ifndef _PETSAVE_H_
#define _PETSAVE_H_

#include <Arduino.h>

// Persists the pet's stats, life state, care progress and the player's coins to
// non-volatile storage (NVS on ESP32) so progress survives power cycles. On
// non-ESP32 targets these are no-ops and the game simply starts fresh each boot.
class PetSave {
  public:
    // Restore saved coins + stats into GameProgress / PetTotoroState.
    // Returns true if a valid save was found and applied.
    static bool load();

    // Write the current coins + stats to storage.
    static void save();

    // Erase the saved data and reset the in-RAM pet + coins to a fresh start
    // (a "factory reset"). Caller typically reboots afterwards.
    static void wipe();

    // Real-time bookkeeping (unix seconds). Both are 0 when no valid time has
    // ever been recorded (e.g. no RTC), which disables offline catch-up.
    static uint32_t lastSeenEpoch();  // when the pet was last simulated
    static uint32_t bornEpoch();      // when the current pet's life started
    static void setLastSeenEpoch(uint32_t epoch);
    static void setBornEpoch(uint32_t epoch);
};

#endif
