#ifndef _CARE_ACTION_RULES_H_
#define _CARE_ACTION_RULES_H_

#include "GameplaySample.h"
#include "PetSim.h"
#include "PetTotoroState.h"

// Care target for each stat the oracle ranks. Reusing PetSim's "all good" bar
// aims the oracle at the state the simulation already rewards with a happiness
// boost, so "below target" means the same thing in both places.
#define CARE_TARGET_HUNGER PET_ALL_GOOD_HUNGER
#define CARE_TARGET_CLEAN PET_ALL_GOOD_CLEAN
#define CARE_TARGET_HAPPINESS 80

// Excitement sheds PET_EXCITEMENT_DECAY_FRAC of itself per minute, so a full
// 100 lands here about an hour after the last game: the pet reads as bored.
#define CARE_BORED_EXCITEMENT 40

// Headroom required on top of a round's hunger/cleanness cost before play is
// suggested, so a game never drains the stat that was just topped up.
#define CARE_PLAY_COST_MARGIN 10

// Rule-based teacher for auto-labeling training data and as an inference fallback
// when the model is disabled or confidence is low.
//
// Serves whichever stat sits furthest below its care target (0..100 scale), and
// picks the action that raises that stat:
//
//   hunger    -> Eat   (a meal is the only thing that refills it)
//   cleanness -> Bath  (restores it to full)
//   happiness -> Play when the pet is bored and can afford a round, else Pet
//
// Two exceptions to the plain ranking:
//   * Hunger or cleanness inside the band where statusUpdateTick() steepens the
//     happiness slide jumps the queue. While a basic need is unmet, lifting
//     happiness treats the symptom and the drain continues.
//   * With every target already met there is no deficit to serve, so build
//     excitement toward the boost bar instead.
//
// Excitement is deliberately not ranked as a need of its own: it is hidden from
// the HUD and decays fast enough to sit near zero between games, so ranking it
// would make Play the answer almost every time.
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

      const int hungerGap = deficitPct(s.hunger, CARE_TARGET_HUNGER);
      const int cleanGap = deficitPct(s.cleanness, CARE_TARGET_CLEAN);
      const int happyGap = deficitPct(s.happiness, CARE_TARGET_HAPPINESS);

      int worst = hungerGap;
      if (cleanGap > worst) {
        worst = cleanGap;
      }
      if (happyGap > worst) {
        worst = happyGap;
      }

      if (worst == 0) {
        return playOrPet(s, PET_ALL_GOOD_EXCITEMENT);
      }
      // Ties go to the concrete fixes, which feed back into happiness anyway.
      if (hungerGap == worst) {
        return CARE_ACTION_EAT;
      }
      if (cleanGap == worst) {
        return CARE_ACTION_BATH;
      }
      return playOrPet(s, CARE_BORED_EXCITEMENT);
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
    // How far a stat sits below its target, as a percentage of that target: 0 at
    // or above it, 100 when empty. Ranking percentages rather than raw points
    // keeps the comparison meaningful if the targets ever diverge.
    static int deficitPct(int value, int target) {
      if (target <= 0 || value >= target) {
        return 0;
      }
      return ((target - value) * 100) / target;
    }

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
