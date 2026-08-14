#ifndef _CARE_ACTION_RULES_H_
#define _CARE_ACTION_RULES_H_

#include "GameplaySample.h"
#include "GameProgress.h"
#include "GroceryFoods.h"
#include "PetSim.h"
#include "PetTotoroState.h"

// Excitement sheds PET_EXCITEMENT_DECAY_FRAC of itself per minute, so a full
// 100 lands here about an hour after the last game: the pet reads as bored.
#define CARE_BORED_EXCITEMENT 40

// Headroom required on top of a round's hunger/cleanness cost before play is
// suggested, so a game never drains the stat that was just topped up.
#define CARE_PLAY_COST_MARGIN 10

// Rule-based teacher for auto-labeling training data and as an inference fallback
// when the model is disabled or confidence is low.
//
// Serves whichever of the three visible stats reads lowest (all on the same
// 0..100 scale) with the action that raises it:
//
//   hunger    -> Eat   (a meal is the only thing that refills it)
//   cleanness -> Bath  (restores it to full)
//   happiness -> Play when the pet is bored and can afford a round, else Pet
//
// Comparing raw values rather than shortfalls against a care target means the
// suggestion keeps tracking the neediest stat even when every stat is healthy —
// at hunger 94 / clean 88 / happy 100 the answer is a bath, not a distraction.
// A tie goes to the stat that will fall first: hunger drains faster than
// cleanness at either growth stage (see the decay rates in PetSim.h), and both
// outrun the happiness drift.
//
// The one exception to the plain ranking: hunger or cleanness inside the band
// where statusUpdateTick() steepens the happiness slide jumps the queue, even if
// another stat reads lower. While a basic need is that unmet, lifting happiness
// treats the symptom and the drain continues.
//
// Excitement is deliberately not ranked as a need of its own: it is hidden from
// the HUD and decays fast enough to sit near zero between games, so ranking it
// would make Play the answer almost every time. It only picks between the two
// ways to raise happiness.
class CareActionRules {
  public:
    static CareAction suggest(const GameplaySample &sample) {
      (void)sample;
      const PetTotoroStats &s = PetTotoroState::stats();

      if (s.hunger < PET_HUNGRY_HAPPY_THRESHOLD) {
        return CARE_ACTION_EAT;
      }
      if (s.cleanness < PET_DIRTY_HAPPY_THRESHOLD) {
        return CARE_ACTION_BATH;
      }

      int lowest = s.hunger;
      if (s.cleanness < lowest) {
        lowest = s.cleanness;
      }
      if (s.happiness < lowest) {
        lowest = s.happiness;
      }

      if (s.hunger == lowest) {
        return CARE_ACTION_EAT;
      }
      if (s.cleanness == lowest) {
        return CARE_ACTION_BATH;
      }
      return playOrPet(s, CARE_BORED_EXCITEMENT);
    }

    // Replaces a suggestion the player has no way to act on. A meal has to be
    // bought, so with less than the cheapest item on the shelves "Eat" is a dead
    // end, and playing a round is how coins are earned.
    //
    // Deliberately separate from suggest() rather than folded into the ranking,
    // because the coin balance is not one of the model's features: labeling
    // training rows with it would teach the network to answer Play on hungry
    // rows for reasons it cannot observe. CareActionPredictor applies this to
    // whichever action wins, so the constraint is enforced rather than learned.
    //
    // A broke *and* starving pet gets Play even though a round costs it more
    // hunger — there is no other way out, so the alternative is advice that
    // cannot be followed.
    static CareAction affordableAlternative(CareAction action) {
      if (action == CARE_ACTION_EAT && GameProgress::getCoins() < groceryCheapestCost()) {
        return CARE_ACTION_PLAY;
      }
      return action;
    }

    static const char *actionName(CareAction action) {
      switch (action) {
        case CARE_ACTION_EAT: return "eat";
        case CARE_ACTION_PLAY: return "play";
        case CARE_ACTION_PET: return "pet";
        case CARE_ACTION_BATH: return "bath";
        default: return "unknown";
      }
    }

  private:
    // A round costs hunger and cleanness, so the pet has to be able to pay.
    static bool canAffordPlay(const PetTotoroStats &s) {
      return s.hunger > (PET_GAME_PLAY_HUNGER_COST + CARE_PLAY_COST_MARGIN) &&
             s.cleanness > (PET_GAME_PLAY_CLEAN_COST + CARE_PLAY_COST_MARGIN);
    }

    // Both ways to raise happiness. A won game is the bigger lift and feeds
    // excitement too, but it only suits a pet dull enough to want one; petting
    // is the small, always-affordable alternative.
    static CareAction playOrPet(const PetTotoroStats &s, int boredBar) {
      if (s.excitement < boredBar && canAffordPlay(s)) {
        return CARE_ACTION_PLAY;
      }
      return CARE_ACTION_PET;
    }
};

#endif
