#include "GameProgress.h"

static int coinCount = 0;

int GameProgress::getCoins() {
  return coinCount;
}

void GameProgress::addCoin() {
  coinCount++;
}

void GameProgress::addCoins(int count) {
  if (count > 0) {
    coinCount += count;
  }
}

void GameProgress::setCoins(int count) {
  coinCount = (count < 0) ? 0 : count;
}

void GameProgress::resetCoins() {
  coinCount = 0;
}
