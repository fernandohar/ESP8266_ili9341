#ifndef _ML_DATA_LOGGER_H_
#define _ML_DATA_LOGGER_H_

#include "GameplaySample.h"
#include "GameResult.h"

// Serial CSV logger for TinyML dataset collection.
// Enable with build flag TINYML_DATA_LOG=1 (see env esp32-tinyml-log).
//
// CSV header (print once from setup when logging is enabled):
// ML,ms,event,game_id,outcome,score,difficulty,session_sec,
// hunger,happy,excitement,clean,unhappy,game_id_norm,win,session_games,label
//
// event: 0 = hub visit, 1 = game end (in mini-game scene, pre-reward stats).
// Hub rows after a finished round repeat the last game's id/score/outcome with
// post-reward pet stats (hunger/clean updated by applyGameReward).
class MLDataLogger {
  public:
    static void printCsvHeader();
    static void resetSession();

    static void onHubVisit();
    static void onGameEnd(int gameId, GameOutcome outcome, int score, int difficulty, uint16_t sessionSeconds);

    static GameplaySample buildSample(int gameId, GameOutcome outcome);

  private:
    static uint8_t sessionGames;
};

#endif
