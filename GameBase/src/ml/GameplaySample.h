#ifndef _GAMEPLAY_SAMPLE_H_
#define _GAMEPLAY_SAMPLE_H_

#include <Arduino.h>

// Normalized feature vector for TinyML (care-action model, phase 1).
// All floats are intended to be in roughly 0..1 unless noted.
struct GameplaySample {
  float hungerNorm;
  float happinessNorm;
  float excitementNorm;
  float cleanNorm;
  float isUnhappy;        // 0 or 1 (happiness very low)
  float lastGameIdNorm;   // gameId / MAX_GAME_ID
  float lastOutcomeWin;   // 1 = win, 0 = loss/none
  float sessionGamesNorm; // min(sessionGames, cap) / cap
};

// Care actions align with the pet radial menu (Phase 1 classifier).
enum CareAction : uint8_t {
  CARE_ACTION_EAT = 0,
  CARE_ACTION_PLAY = 1,
  CARE_ACTION_PET = 2,
  CARE_ACTION_BATH = 3,
  CARE_ACTION_COUNT = 4
};

enum GameplayEventKind : uint8_t {
  GAMEPLAY_EVENT_HUB_VISIT = 0,
  GAMEPLAY_EVENT_GAME_END = 1
};

#endif
