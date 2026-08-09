#ifndef _ML_GAME_HOOKS_H_
#define _ML_GAME_HOOKS_H_

#include "GameResult.h"
#include "MLDataLogger.h"

// Thin wrappers so scenes log telemetry next to GameResult::report().
// MLDataLogger no-ops when TINYML_DATA_LOG is not defined.
inline void mlLogHubVisit() {
  MLDataLogger::onHubVisit();
}

inline void mlLogGameEnd(int gameId, GameOutcome outcome, int score, int difficulty, uint16_t sessionSeconds) {
  MLDataLogger::onGameEnd(gameId, outcome, score, difficulty, sessionSeconds);
}

#endif
