#ifndef _GESTUREEPISODE_H_
#define _GESTUREEPISODE_H_

// stdint rather than Arduino.h on purpose: this header and the feature extractor
// beside it are compiled unchanged on the PC, so the training pipeline runs the
// firmware's own extractor instead of a Python re-implementation that could drift
// away from it.
#include <stdint.h>

// One gesture is captured as an *episode*: a run of screen contact that may be
// made of several separate strokes, ending once the screen has been idle long
// enough that no further stroke is coming. A poke is one short stroke, a double
// poke is two strokes inside the gap window, a brush is one long stroke.
//
// A gesture made in one spot is offered to the classifier as soon as the finger
// lifts, so the wait for a possible second stroke is not paid as input latency. See
// GESTURE_EPISODE_GAP_MS below and TouchSampler::previewReady().

// ~60 Hz. A poke can last barely 100 ms, so the 20 Hz game tick is far too
// coarse to describe one.
#define GESTURE_SAMPLE_INTERVAL_MS 16

// Consecutive misses needed to call the pen lifted. Resistive panels drop the
// occasional read mid-stroke, and without this a single dropout would split one
// brush into two strokes.
#define GESTURE_STROKE_END_MS 48

// Idle time after the last stroke that closes the episode. It has to cover the
// slowest pause a player leaves *inside* one gesture: the lift between two taps of
// a double poke, and the lift between two strokes of a brush.
//
// A captured session of 58 deliberate double pokes puts that pause at 184-327 ms,
// with a third of them past 280 ms. Brush gaps reach ~300 ms. So 300 ms - which is
// what this was - cuts through the middle of normal play: half the double pokes
// arrived just late enough to be split into two single pokes, and which half
// depended on where the poll landed, so the gesture felt broken at random.
//
// This number used to double as input latency, because nothing could be classified
// until the episode closed, and that is what kept it short. It does not any more:
// TouchSampler offers a stationary episode for classification as soon as the finger
// lifts (see previewReady), so a poke is acted on ~100 ms after the tap no matter
// how long this window is. That decoupling is what makes it safe to be patient, and
// it is also why there is now one window instead of a short stationary one and a
// long travelled one.
//
// Worth knowing before shortening it again: the model is happier with slow double
// pokes, not less happy. Shifting the second tap of every captured double poke up
// to 500 ms later leaves recognition at 100% and raises mean confidence from 0.93 to
// 0.98, because a long pause makes a double poke look like nothing else.
#define GESTURE_EPISODE_GAP_MS 550

// Travel above which an episode counts as having gone somewhere. Separates the
// gestures made in one spot - poke, double poke, long press - from the ones that go
// somewhere, which is what lets TouchSampler close and preview the stationary ones
// early without ever cutting a brush in half.
#define GESTURE_TRAVEL_MOVING_PX 30.0f

// Hard cap so a resting palm cannot grow an episode without bound.
#define GESTURE_MAX_EPISODE_MS 2500

// GESTURE_MAX_EPISODE_MS at GESTURE_SAMPLE_INTERVAL_MS needs 157 slots, so a
// full-length episode fits without truncation.
#define GESTURE_MAX_SAMPLES 160
#define GESTURE_MAX_STROKES 8

struct GestureSample {
  int16_t x;
  int16_t y;
  uint16_t t;      // ms since the episode started
  uint8_t stroke;  // 0-based index of the stroke this sample belongs to
};

struct GestureEpisode {
  GestureSample samples[GESTURE_MAX_SAMPLES];
  uint8_t sampleCount;
  uint8_t strokeCount;
  uint16_t durationMs;
  bool truncated;  // hit GESTURE_MAX_SAMPLES or GESTURE_MAX_EPISODE_MS
  uint32_t startMs;
};

// Where the gesture happened. The features are position-invariant on purpose - a
// circle is a circle anywhere - so a consumer that cares *what was touched* has to
// ask separately.
inline void gestureEpisodeCentroid(const GestureEpisode &ep, int16_t *x, int16_t *y) {
  if (ep.sampleCount == 0) {
    *x = 0;
    *y = 0;
    return;
  }
  int32_t sumX = 0;
  int32_t sumY = 0;
  for (uint8_t i = 0; i < ep.sampleCount; i++) {
    sumX += ep.samples[i].x;
    sumY += ep.samples[i].y;
  }
  *x = (int16_t)(sumX / ep.sampleCount);
  *y = (int16_t)(sumY / ep.sampleCount);
}

// Net horizontal travel, which is what gives a swipe its direction.
inline int16_t gestureEpisodeNetDx(const GestureEpisode &ep) {
  if (ep.sampleCount < 2) {
    return 0;
  }
  return (int16_t)(ep.samples[ep.sampleCount - 1].x - ep.samples[0].x);
}

// Capture classes. The intents are what each gesture is meant to drive once a
// classifier is trained; circle and zigzag share an intent but stay separate
// classes because they are different shapes to recognise.
enum GestureLabel : uint8_t {
  GESTURE_POKE = 0,
  GESTURE_DOUBLE_POKE = 1,
  GESTURE_LONG_PRESS = 2,
  GESTURE_BRUSH = 3,
  GESTURE_SWIPE = 4,
  GESTURE_CIRCLE = 5,
  GESTURE_ZIGZAG = 6,
  GESTURE_UNKNOWN = 7
};
#define GESTURE_LABEL_COUNT 8

// Short machine-readable name, used as the label column in the capture CSV.
inline const char *gestureLabelName(uint8_t label) {
  switch (label) {
    case GESTURE_POKE: return "poke";
    case GESTURE_DOUBLE_POKE: return "double_poke";
    case GESTURE_LONG_PRESS: return "long_press";
    case GESTURE_BRUSH: return "brush";
    case GESTURE_SWIPE: return "swipe";
    case GESTURE_CIRCLE: return "circle";
    case GESTURE_ZIGZAG: return "zigzag";
    default: return "unknown";
  }
}

inline const char *gestureLabelTitle(uint8_t label) {
  switch (label) {
    case GESTURE_POKE: return "POKE";
    case GESTURE_DOUBLE_POKE: return "DOUBLE POKE";
    case GESTURE_LONG_PRESS: return "LONG PRESS";
    case GESTURE_BRUSH: return "BRUSH";
    case GESTURE_SWIPE: return "SWIPE";
    case GESTURE_CIRCLE: return "CIRCLE";
    case GESTURE_ZIGZAG: return "ZIGZAG";
    default: return "UNKNOWN";
  }
}

// Strokes a label cannot possibly have fewer of. A one-stroke double poke is not a
// borderline example, it is a mislabel - it looks exactly like a poke, and a third
// of the first capture session's double pokes were exactly that, which would have
// poisoned the very distinction the class exists to make.
//
// Only *structural* impossibilities belong here. Rejecting a 200 ms long press or
// a slow swipe would be hand-writing the classifier's decision boundary, which is
// the model's job to learn.
inline uint8_t gestureLabelMinStrokes(uint8_t label) {
  return label == GESTURE_DOUBLE_POKE ? 2 : 1;
}

// Shown on the capture screen so the gesture is performed the way it will be
// used, not the way it is easiest to draw.
inline const char *gestureLabelIntent(uint8_t label) {
  switch (label) {
    case GESTURE_POKE: return "get Totoro's attention";
    case GESTURE_DOUBLE_POKE: return "open the menu";
    case GESTURE_LONG_PRESS: return "grab, then drag";
    case GESTURE_BRUSH: return "petting";
    case GESTURE_SWIPE: return "send Totoro away";
    case GESTURE_CIRCLE: return "make Totoro dance";
    case GESTURE_ZIGZAG: return "make Totoro dance";
    default: return "stray touch, ignore";
  }
}

#endif
