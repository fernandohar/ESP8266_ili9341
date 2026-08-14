#ifndef _CARE_ACTION_PREDICTOR_H_
#define _CARE_ACTION_PREDICTOR_H_

#include <Arduino.h>

#include "GameplaySample.h"

// Softmax score the winning class must reach before the model is allowed to
// answer. Below it the rule oracle decides, so a hesitant network never drives
// the UI.
#define CARE_PREDICT_MIN_CONFIDENCE 0.65f

struct CarePrediction {
  CareAction action;
  float confidence;  // score of `action`; 0 when the rules answered
  bool fromModel;    // false when CareActionRules produced the answer
};

// Care-action suggestion: the trained model first, CareActionRules as the floor.
// Built for TINYML_INFERENCE=1; without that flag this compiles down to the rule
// oracle alone, so callers never need an #ifdef.
class CareActionPredictor {
  public:
    static CarePrediction predict(const GameplaySample &sample);
    // True once the interpreter has allocated its tensors.
    static bool modelReady();
    // Duration of the last Invoke(), for a dev overlay. 0 without a model.
    static uint32_t lastInferenceMicros();
};

#endif
