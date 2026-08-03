#include "GameResult.h"

static GameOutcome s_outcome = GAME_RESULT_NONE;
static int s_coins = -1;
static int s_happiness = -1;

void GameResult::report(GameOutcome outcome, int coins, int happiness) {
  s_outcome = outcome;
  s_coins = coins;
  s_happiness = happiness;
}

bool GameResult::pending() {
  return s_outcome != GAME_RESULT_NONE;
}

GameOutcome GameResult::outcome() {
  return s_outcome;
}

int GameResult::coins() {
  return s_coins;
}

int GameResult::happiness() {
  return s_happiness;
}

void GameResult::clear() {
  s_outcome = GAME_RESULT_NONE;
  s_coins = -1;
  s_happiness = -1;
}
