#ifndef _GAMERESULT_H_
#define _GAMERESULT_H_

#include "GameSceneIds.h"

// One-shot hand-off from a mini-game back to the pet's home. A game reports the
// outcome of every round it finishes; the coin reward screen banks the coins on
// the way out and the pet scene consumes the rest (happiness / care-XP), then
// clears it. Not persisted (it is only meaningful for the single transition from
// a game back home).
enum GameOutcome {
  GAME_RESULT_NONE = 0,
  GAME_RESULT_WIN,
  GAME_RESULT_LOSS,
  GAME_RESULT_NEUTRAL   // played but no win/loss verdict (e.g. 2P Tic-Tac-Toe)
};

// Payout for a win that names no amount of its own, and the flat consolation a
// lost round always pays.
#define GAME_WIN_DEFAULT_COINS 5
#define GAME_LOSS_CONSOLATION_COINS 2

class GameResult {
  public:
    // Records one finished round. coins < 0 means "no game-specific amount; use
    // the default"; a loss always pays the consolation instead of the reported
    // amount. Payouts add up across every round reported before the player
    // leaves, so replaying is rewarded for each round rather than only the last.
    // happiness < 0 means "let the pet use its default win/loss happiness".
    static void report(GameOutcome outcome, int coins = -1, int happiness = -1);
    static bool pending();
    // Outcome of the most recent round; sets the pet's happiness/care-XP tier.
    static GameOutcome outcome();
    // Hands the accumulated coins over and zeroes the tally, so only the first
    // caller (the coin reward screen) can bank them.
    static int takeCoins();
    static int happiness();
    static void clear();
};

// Where a mini-game sends the player when it hands control back: the coin reward
// screen when a round was actually finished, otherwise straight home.
inline int gameExitSceneIndex() {
  return GameResult::pending() ? SCENE_COIN_REWARD : SCENE_PET_TOTORO;
}

#endif
