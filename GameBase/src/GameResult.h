#ifndef _GAMERESULT_H_
#define _GAMERESULT_H_

// One-shot hand-off from a mini-game back to the pet's home. A game reports its
// outcome when a round ends; the pet scene consumes it on the next entry to
// grant coins / happiness / care-XP, then clears it. Not persisted (it is only
// meaningful for the single transition from a game straight back home).
enum GameOutcome {
  GAME_RESULT_NONE = 0,
  GAME_RESULT_WIN,
  GAME_RESULT_LOSS
};

class GameResult {
  public:
    // coins < 0 means "no game-specific amount; let the pet use its default".
    // happiness < 0 means "let the pet use its default win/loss happiness".
    static void report(GameOutcome outcome, int coins = -1, int happiness = -1);
    static bool pending();
    static GameOutcome outcome();
    static int coins();
    static int happiness();
    static void clear();
};

#endif
