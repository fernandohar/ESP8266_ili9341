#ifndef _GESTUREPREDICTOR_H_
#define _GESTUREPREDICTOR_H_

#include <Arduino.h>

#include "GestureEpisode.h"

// Classifies a finished touch episode.
//
// Unlike the care-action model there is no rule fallback, because there is no
// sensible one: a rule good enough to tell a circle from a zigzag from a stray
// palm is the thing the model exists to avoid writing. When the model is missing
// or unsure the answer is simply "no gesture", and nothing happens - which is also
// what the player gets for a genuinely unrecognisable scribble, so the two failure
// modes look identical from the outside.

// Below this the prediction is discarded. Measured on the honest
// leave-one-session-out split of the first 519 captures:
//
//   threshold   acted on   correct   wrong actions
//        0.70        94%     92.4%            7.1%
//        0.80        90%     93.2%            6.2%
//        0.90        84%     94.9%            4.2%
//        0.95        72%     96.0%            2.9%
//
// A rejected gesture costs the player a repeat; an accepted wrong one makes the pet
// do something unasked, which reads as the toy being broken. 0.80 keeps nine in ten
// gestures responsive while holding the visible error rate near six percent.
#define GESTURE_MIN_CONFIDENCE 0.80f

struct GesturePrediction {
  uint8_t label;      // GestureLabel; only meaningful when recognised is true
  float confidence;   // softmax probability of that label
  bool recognised;    // false when unavailable, failed, unsure, or GESTURE_UNKNOWN
};

class GesturePredictor {
  public:
    // Safe to call before the model has ever run; initialises on first use.
    static GesturePrediction classify(const GestureEpisode &episode);
    static bool modelReady();
    static uint32_t lastInferenceMicros();
    // Last raw result including rejections, for the on-device debug readout.
    static GesturePrediction lastRaw();
};

#endif
