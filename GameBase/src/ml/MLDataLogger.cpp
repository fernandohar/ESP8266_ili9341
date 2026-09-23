#include "MLDataLogger.h"

#include "CareActionRules.h"
#include "GameSceneIds.h"
#include "PetTotoroState.h"

uint8_t MLDataLogger::sessionGames = 0;

// Tracked in every build, not just logging ones: CareActionPredictor has to feed
// the model the same last-game context the hub rows were trained on, and
// sessionGames is a feature. Only the Serial output is conditional.
static int s_lastGameId = 0;
static GameOutcome s_lastOutcome = GAME_RESULT_NONE;
static int s_lastScore = 0;
static int s_lastDifficulty = 0;
static uint16_t s_lastSessionSec = 0;
static bool s_hasLastGame = false;

#if defined(TINYML_DATA_LOG)
static void logEvent(GameplayEventKind kind, int gameId, GameOutcome outcome,
                     int score, int difficulty, uint16_t sessionSeconds) {
  GameplaySample sample = MLDataLogger::buildSample(gameId, outcome);
  CareAction label = CareActionRules::suggest(sample);

  Serial.print(F("ML,"));
  Serial.print(millis());
  Serial.print(F(","));
  Serial.print((int)kind);
  Serial.print(F(","));
  Serial.print(gameId);
  Serial.print(F(","));
  Serial.print((int)outcome);
  Serial.print(F(","));
  Serial.print(score);
  Serial.print(F(","));
  Serial.print(difficulty);
  Serial.print(F(","));
  Serial.print(sessionSeconds);
  Serial.print(F(","));
  Serial.print(sample.hungerNorm, 3);
  Serial.print(F(","));
  Serial.print(sample.happinessNorm, 3);
  Serial.print(F(","));
  Serial.print(sample.excitementNorm, 3);
  Serial.print(F(","));
  Serial.print(sample.cleanNorm, 3);
  Serial.print(F(","));
  Serial.print(sample.isUnhappy, 0);
  Serial.print(F(","));
  Serial.print(sample.lastGameIdNorm, 3);
  Serial.print(F(","));
  Serial.print(sample.lastOutcomeWin, 0);
  Serial.print(F(","));
  Serial.print(sample.sessionGamesNorm, 3);
  Serial.print(F(","));
  Serial.println(CareActionRules::actionName(label));
}
#endif

void MLDataLogger::printCsvHeader() {
#if defined(TINYML_DATA_LOG)
  Serial.println(F("ML,ms,event,game_id,outcome,score,difficulty,session_sec,"
                   "hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label"));
#endif
}

void MLDataLogger::resetSession() {
  sessionGames = 0;
  s_hasLastGame = false;
}

#if defined(TINYML_DATA_LOG)
// Log the current pet stats against the last finished round's context, so a row
// taken in the home still says which game was played and how it went — not zeros.
static void logWithLastGame(GameplayEventKind kind) {
  if (s_hasLastGame) {
    logEvent(kind, s_lastGameId, s_lastOutcome, s_lastScore, s_lastDifficulty,
             s_lastSessionSec);
  } else {
    logEvent(kind, SCENE_PET_TOTORO, GAME_RESULT_NONE, 0, 0, 0);
  }
}
#endif

void MLDataLogger::onHubVisit() {
#if defined(TINYML_DATA_LOG)
  // Returning home after a game: pet stats are post-reward (applyGameReward ran
  // first). Game-end rows logged in the mini-game scene carry pre-reward stats.
  logWithLastGame(GAMEPLAY_EVENT_HUB_VISIT);
#endif
}

void MLDataLogger::onCareState() {
#if defined(TINYML_DATA_LOG)
  logWithLastGame(GAMEPLAY_EVENT_CARE_STATE);
#endif
}

void MLDataLogger::onGameEnd(int gameId, GameOutcome outcome, int score, int difficulty, uint16_t sessionSeconds) {
  s_lastGameId = gameId;
  s_lastOutcome = outcome;
  s_lastScore = score;
  s_lastDifficulty = difficulty;
  s_lastSessionSec = sessionSeconds;
  s_hasLastGame = true;
  sessionGames++;

#if defined(TINYML_DATA_LOG)
  logEvent(GAMEPLAY_EVENT_GAME_END, gameId, outcome, score, difficulty, sessionSeconds);
#endif
}

// The feature vector a suggestion at home is made from: current pet stats
// against the last finished round's context, matching the hub rows in training.
GameplaySample MLDataLogger::buildHubSample() {
  if (s_hasLastGame) {
    return buildSample(s_lastGameId, s_lastOutcome);
  }
  return buildSample(SCENE_PET_TOTORO, GAME_RESULT_NONE);
}

GameplaySample MLDataLogger::buildSample(int gameId, GameOutcome outcome) {
  const PetTotoroStats &s = PetTotoroState::stats();
  GameplaySample sample = {};
  sample.hungerNorm = s.hunger / 100.0f;
  sample.happinessNorm = s.happiness / 100.0f;
  sample.excitementNorm = s.excitement / 100.0f;
  sample.cleanNorm = s.cleanness / 100.0f;
  sample.isUnhappy = PetTotoroState::isSick() ? 1.0f : 0.0f;
  sample.lastGameIdNorm = gameId / 7.0f;
  sample.lastOutcomeWin = (outcome == GAME_RESULT_WIN) ? 1.0f : 0.0f;
  float capped = sessionGames > 10 ? 10.0f : (float)sessionGames;
  sample.sessionGamesNorm = capped / 10.0f;
  return sample;
}
