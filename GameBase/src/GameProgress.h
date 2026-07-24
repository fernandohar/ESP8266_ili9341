#ifndef _GAMEPROGRESS_H_
#define _GAMEPROGRESS_H_

#include <Arduino.h>

class GameProgress {
  public:
    static int getCoins();
    static void addCoin();
    static void addCoins(int count);
    static void resetCoins();
};

#endif
