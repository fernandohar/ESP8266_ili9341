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
// event: 0 = hub visit, 1 = game end (in mini-game scene, pre-reward stats),
// 2 = care state (just after a feed / pet / bath took effect).
// Hub rows after a finished round repeat the last game's id/score/outcome with
// post-reward pet stats (hunger/clean updated by applyGameReward).
//
// Hub rows alone make a biased dataset: they are written moments after the
// mini-game reward tops happiness up, so they cluster at the high end. Care rows
// sample the other half of the curve - a stat has just been refilled or, for the
// vegetables, knocked down - which is where a care suggestion has to be right.
class MLDataLogger {
  public:
    static void printCsvHeader();
    static void resetSession();

    static void onHubVisit();
    static void onCareState();
    static void onGameEnd(int gameId, GameOutcome outcome, int score, int difficulty, uint16_t sessionSeconds);

    static GameplaySample buildSample(int gameId, GameOutcome outcome);
    // buildSample() against the remembered last-game context, so an at-home
    // suggestion sees the same features the hub training rows carried.
    static GameplaySample buildHubSample();

  private:
    static uint8_t sessionGames;
};

#endif
