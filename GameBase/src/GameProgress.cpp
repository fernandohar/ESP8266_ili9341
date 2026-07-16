#include "GameProgress.h"

static int coinCount = 0;

int GameProgress::getCoins() {
  return coinCount;
}

void GameProgress::addCoin() {
  coinCount++;
}

void GameProgress::resetCoins() {
  coinCount = 0;
}
