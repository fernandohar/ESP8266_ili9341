#ifndef _CARE_ACTION_RULES_H_
#define _CARE_ACTION_RULES_H_

#include "GameplaySample.h"
#include "PetTotoroState.h"

// Rule-based teacher for auto-labeling training data and as an inference fallback
// when the model is disabled or confidence is low.
//
// Care loop:
//   Eat  — food restores hunger (+ happiness per grocery item).
//   Bathe — restores cleanness only (not health).
//   Pet / Play — raise happiness; play also costs hunger and cleanness when enabled.
class CareActionRules {
  public:
    static CareAction suggest(const GameplaySample &sample) {
      const PetTotoroStats &s = PetTotoroState::stats();

      if (s.hunger < 30) {
        return CARE_ACTION_EAT;
      }
      if (s.cleanness < 25) {
        return CARE_ACTION_BATH;
      }
      if (s.happiness < 35) {
        return CARE_ACTION_PET;
      }
      if (sample.lastOutcomeWin < 0.5f && sample.sessionGamesNorm > 0.3f) {
        return CARE_ACTION_PET;
      }
      return CARE_ACTION_PLAY;
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
};

#endif
