#include "GameResult.h"

static GameOutcome s_outcome = GAME_RESULT_NONE;
static int s_coinsOwed = 0;
static int s_happiness = -1;

void GameResult::report(GameOutcome outcome, int coins, int happiness) {
  s_outcome = outcome;
  s_happiness = happiness;
  if (outcome == GAME_RESULT_WIN) {
    s_coinsOwed += (coins >= 0) ? coins : GAME_WIN_DEFAULT_COINS;
  } else if (outcome == GAME_RESULT_LOSS || outcome == GAME_RESULT_NEUTRAL) {
    s_coinsOwed += GAME_LOSS_CONSOLATION_COINS;
  }
}

bool GameResult::pending() {
  return s_outcome != GAME_RESULT_NONE;
}

GameOutcome GameResult::outcome() {
  return s_outcome;
}

int GameResult::takeCoins() {
  int owed = s_coinsOwed;
  s_coinsOwed = 0;
  return owed;
}

int GameResult::happiness() {
  return s_happiness;
}

void GameResult::clear() {
  s_outcome = GAME_RESULT_NONE;
  s_coinsOwed = 0;
  s_happiness = -1;
}
