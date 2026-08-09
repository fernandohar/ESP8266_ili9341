#include "MLDataLogger.h"

#include "CareActionRules.h"
#include "GameSceneIds.h"
#include "PetTotoroState.h"

uint8_t MLDataLogger::sessionGames = 0;

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
  Serial.print(sample.healthNorm, 3);
  Serial.print(F(","));
  Serial.print(sample.cleanNorm, 3);
  Serial.print(F(","));
  Serial.print(sample.isSick, 0);
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
                   "hunger,happy,health,clean,sick,game_id_norm,win,session_games,label"));
#endif
}

void MLDataLogger::resetSession() {
  sessionGames = 0;
}

void MLDataLogger::onHubVisit() {
#if defined(TINYML_DATA_LOG)
  logEvent(GAMEPLAY_EVENT_HUB_VISIT, SCENE_PET_TOTORO, GAME_RESULT_NONE, 0, 0, 0);
#endif
}

void MLDataLogger::onGameEnd(int gameId, GameOutcome outcome, int score, int difficulty, uint16_t sessionSeconds) {
#if defined(TINYML_DATA_LOG)
  sessionGames++;
  logEvent(GAMEPLAY_EVENT_GAME_END, gameId, outcome, score, difficulty, sessionSeconds);
#else
  (void)gameId;
  (void)outcome;
  (void)score;
  (void)difficulty;
  (void)sessionSeconds;
#endif
}

GameplaySample MLDataLogger::buildSample(int gameId, GameOutcome outcome) {
  const PetTotoroStats &s = PetTotoroState::stats();
  GameplaySample sample = {};
  sample.hungerNorm = s.hunger / 100.0f;
  sample.happinessNorm = s.happiness / 100.0f;
  sample.healthNorm = s.health / 100.0f;
  sample.cleanNorm = s.cleanness / 100.0f;
  sample.isSick = PetTotoroState::isSick() ? 1.0f : 0.0f;
  sample.lastGameIdNorm = gameId / 7.0f;
  sample.lastOutcomeWin = (outcome == GAME_RESULT_WIN) ? 1.0f : 0.0f;
  float capped = sessionGames > 10 ? 10.0f : (float)sessionGames;
  sample.sessionGamesNorm = capped / 10.0f;
  return sample;
}
