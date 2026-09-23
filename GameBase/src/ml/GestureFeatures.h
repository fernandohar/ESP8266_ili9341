#ifndef _GESTUREFEATURES_H_
#define _GESTUREFEATURES_H_

// Turns a captured episode into the fixed-length vector the classifier consumes.
//
// This file is deliberately free of Arduino dependencies so the PC pipeline can
// compile and run it directly (see tinyml/host/gesture_features_main.cpp). The
// care-action model has the same function written twice, once in C++ and once in
// Python, and keeping the two in step needs a parity harness; here there is only
// one implementation, so they cannot drift.
//
// Two kinds of feature, because the gesture set needs both:
//
//  - A path resampled to a fixed number of points, carrying *shape*. This is what
//    separates a circle from a zigzag. Resampling is by cumulative path length,
//    not by sample index, so a capture at 33 Hz and one at 60 Hz produce the same
//    vector for the same stroke.
//  - Scalars carrying *dynamics* and scale. A poke and a long press draw the same
//    picture; only duration tells them apart. A brush and a slow drag likewise
//    differ in speed and reversals, not shape.
//
// Short episodes are normal, not degenerate: a 70 ms poke has three or four
// samples at any realistic rate. Those classes are carried entirely by the
// scalars, and the shape half is damped to zero for them (see MIN_SPAN below)
// rather than amplifying a few pixels of jitter into a bogus shape.

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "GestureEpisode.h"

#define GESTURE_RESAMPLE_POINTS 12
#define GESTURE_SCALAR_FEATURES 16
#define GESTURE_FEATURE_COUNT (GESTURE_RESAMPLE_POINTS * 2 + GESTURE_SCALAR_FEATURES)

// Normalisation caps. Anything past these saturates; they exist so every feature
// lands in a comparable range without a scaler having to be shipped for them.
#define GESTURE_NORM_DURATION_MS 2500.0f
#define GESTURE_NORM_PATH_PX 800.0f
#define GESTURE_NORM_SPAN_PX 240.0f
#define GESTURE_NORM_GAP_MS 500.0f
#define GESTURE_NORM_SPEED_PX_MS 3.0f
#define GESTURE_NORM_MAX_SPEED_PX_MS 5.0f
#define GESTURE_NORM_REVERSALS 20.0f
#define GESTURE_NORM_STROKES 4.0f

// A stroke smaller than this is treated as having no trajectory at all, and every
// path-derived feature reads zero for it. Without this a held finger's jitter
// looks like deliberate motion: a real long press covers an 11x11 px box, yet its
// wobble accumulated 66 px of "path" and saturated the turning features, making it
// indistinguishable from a brush on everything except path length. A stationary
// contact is then described only by how long it lasted, which is the truth.
#define GESTURE_MIN_SPAN_PX 20.0f

// Minimum segment length before a direction change counts. Even inside a large
// gesture, consecutive samples a pixel or two apart are noise, and each such step
// contributes a near-180-degree turn that swamps the real curvature.
#define GESTURE_MIN_TURN_SEG_PX 4.0f
#define GESTURE_MIN_REVERSAL_PX 3.0f

inline float gestureClamp(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

inline const char *gestureFeatureName(int index) {
  // Static buffer: the caller writes a CSV header one column at a time and does
  // not hold on to the string.
  static char buffer[16];
  if (index < GESTURE_RESAMPLE_POINTS * 2) {
    int point = index / 2;
    snprintf(buffer, sizeof(buffer), "p%d%c", point, (index % 2) == 0 ? 'x' : 'y');
    return buffer;
  }
  switch (index - GESTURE_RESAMPLE_POINTS * 2) {
    case 0: return "duration";
    case 1: return "path_len";
    case 2: return "bbox_w";
    case 3: return "bbox_h";
    case 4: return "straightness";
    case 5: return "strokes";
    case 6: return "max_gap";
    case 7: return "contact_frac";
    case 8: return "mean_speed";
    case 9: return "max_speed";
    case 10: return "net_dx";
    case 11: return "net_dy";
    case 12: return "winding";
    case 13: return "abs_turn";
    case 14: return "reversals";
    case 15: return "elongation";
    default: return "?";
  }
}

// Writes GESTURE_FEATURE_COUNT floats to out. Returns false only when there is
// nothing to describe.
inline bool gestureExtractFeatures(const GestureSample *samples, uint8_t count,
                                   float *out) {
  for (int i = 0; i < GESTURE_FEATURE_COUNT; i++) {
    out[i] = 0.0f;
  }
  if (samples == NULL || count == 0) {
    return false;
  }

  float minX = samples[0].x, maxX = samples[0].x;
  float minY = samples[0].y, maxY = samples[0].y;
  for (uint8_t i = 1; i < count; i++) {
    if (samples[i].x < minX) minX = samples[i].x;
    if (samples[i].x > maxX) maxX = samples[i].x;
    if (samples[i].y < minY) minY = samples[i].y;
    if (samples[i].y > maxY) maxY = samples[i].y;
  }

  const float bboxW = maxX - minX;
  const float bboxH = maxY - minY;
  const float centerX = (minX + maxX) * 0.5f;
  const float centerY = (minY + maxY) * 0.5f;
  float span = bboxW > bboxH ? bboxW : bboxH;
  const bool hasExtent = span >= GESTURE_MIN_SPAN_PX;
  if (!hasExtent) {
    span = GESTURE_MIN_SPAN_PX;
  }

  // Single pass over within-stroke segments. Inter-stroke transitions are skipped
  // everywhere: the finger was not on the panel, so treating the jump as travel
  // would invent a long fast segment that never happened.
  float pathLen = 0.0f;
  float maxSpeed = 0.0f;
  float signedTurn = 0.0f;
  float absTurn = 0.0f;
  int reversals = 0;
  int lastSignX = 0;
  int lastSignY = 0;
  float prevSegX = 0.0f, prevSegY = 0.0f;
  bool havePrevSeg = false;

  float contactMs = 0.0f;
  float maxGapMs = 0.0f;
  uint16_t strokeFirstT = samples[0].t;
  uint16_t strokeLastT = samples[0].t;

  for (uint8_t i = 1; i < count; i++) {
    const bool sameStroke = samples[i].stroke == samples[i - 1].stroke;
    if (!sameStroke) {
      contactMs += (float)(strokeLastT - strokeFirstT);
      const float gap = (float)(samples[i].t - samples[i - 1].t);
      if (gap > maxGapMs) {
        maxGapMs = gap;
      }
      strokeFirstT = samples[i].t;
      strokeLastT = samples[i].t;
      havePrevSeg = false;
      lastSignX = 0;
      lastSignY = 0;
      continue;
    }

    strokeLastT = samples[i].t;

    const float dx = (float)(samples[i].x - samples[i - 1].x);
    const float dy = (float)(samples[i].y - samples[i - 1].y);
    const float dist = sqrtf(dx * dx + dy * dy);
    pathLen += dist;

    const float dt = (float)(samples[i].t - samples[i - 1].t);
    if (dt > 0.0f) {
      const float speed = dist / dt;
      if (speed > maxSpeed) {
        maxSpeed = speed;
      }
    }

    // Reversals are what make a brush a brush: back-and-forth on one axis.
    // Small wobble is ignored so panel noise is not counted as a reversal.
    if (fabsf(dx) > GESTURE_MIN_REVERSAL_PX) {
      const int signX = dx > 0.0f ? 1 : -1;
      if (lastSignX != 0 && signX != lastSignX) {
        reversals++;
      }
      lastSignX = signX;
    }
    if (fabsf(dy) > GESTURE_MIN_REVERSAL_PX) {
      const int signY = dy > 0.0f ? 1 : -1;
      if (lastSignY != 0 && signY != lastSignY) {
        reversals++;
      }
      lastSignY = signY;
    }

    // Turning angle between consecutive segments. The signed sum is winding: a
    // circle accumulates about one full turn in a consistent direction, while a
    // zigzag's alternating turns cancel. The absolute sum is large for both, so
    // the pair separates them where neither does alone.
    if (dist > GESTURE_MIN_TURN_SEG_PX) {
      if (havePrevSeg) {
        const float cross = prevSegX * dy - prevSegY * dx;
        const float dot = prevSegX * dx + prevSegY * dy;
        const float angle = atan2f(cross, dot);
        signedTurn += angle;
        absTurn += fabsf(angle);
      }
      prevSegX = dx;
      prevSegY = dy;
      havePrevSeg = true;
    }
  }
  contactMs += (float)(strokeLastT - strokeFirstT);

  const float durationMs = (float)samples[count - 1].t;
  const float netDx = (float)(samples[count - 1].x - samples[0].x);
  const float netDy = (float)(samples[count - 1].y - samples[0].y);
  const float netDisp = sqrtf(netDx * netDx + netDy * netDy);

  // Shape half: walk the concatenated within-stroke path and sample it at equal
  // arc-length intervals.
  float *shape = out;
  if (!hasExtent || pathLen <= 0.5f) {
    // Nowhere to go: a poke, or a held finger whose only travel is jitter. Every
    // resampled point sits at the centre, which normalises to zero and lets the
    // scalars carry the class.
    for (int i = 0; i < GESTURE_RESAMPLE_POINTS * 2; i++) {
      shape[i] = 0.0f;
    }
  } else {
    const float step = pathLen / (float)(GESTURE_RESAMPLE_POINTS - 1);
    int written = 0;
    float travelled = 0.0f;
    float nextMark = 0.0f;
    float curX = (float)samples[0].x;
    float curY = (float)samples[0].y;

    shape[written * 2] = gestureClamp((curX - centerX) / span, -1.0f, 1.0f);
    shape[written * 2 + 1] = gestureClamp((curY - centerY) / span, -1.0f, 1.0f);
    written++;
    nextMark = step;

    for (uint8_t i = 1; i < count && written < GESTURE_RESAMPLE_POINTS; i++) {
      if (samples[i].stroke != samples[i - 1].stroke) {
        curX = (float)samples[i].x;
        curY = (float)samples[i].y;
        continue;
      }
      float segX = (float)samples[i].x - curX;
      float segY = (float)samples[i].y - curY;
      float segLen = sqrtf(segX * segX + segY * segY);

      while (segLen > 0.0f && travelled + segLen >= nextMark &&
             written < GESTURE_RESAMPLE_POINTS) {
        const float need = nextMark - travelled;
        const float ratio = need / segLen;
        curX += segX * ratio;
        curY += segY * ratio;
        shape[written * 2] = gestureClamp((curX - centerX) / span, -1.0f, 1.0f);
        shape[written * 2 + 1] = gestureClamp((curY - centerY) / span, -1.0f, 1.0f);
        written++;
        nextMark += step;

        segX = (float)samples[i].x - curX;
        segY = (float)samples[i].y - curY;
        travelled += need;
        segLen = sqrtf(segX * segX + segY * segY);
      }

      travelled += segLen;
      curX = (float)samples[i].x;
      curY = (float)samples[i].y;
    }

    // Rounding can leave the last slot or two unfilled; pin them to the endpoint.
    const float endX = gestureClamp(((float)samples[count - 1].x - centerX) / span, -1.0f, 1.0f);
    const float endY = gestureClamp(((float)samples[count - 1].y - centerY) / span, -1.0f, 1.0f);
    while (written < GESTURE_RESAMPLE_POINTS) {
      shape[written * 2] = endX;
      shape[written * 2 + 1] = endY;
      written++;
    }
  }

  float *scalar = out + GESTURE_RESAMPLE_POINTS * 2;
  scalar[0] = gestureClamp(durationMs / GESTURE_NORM_DURATION_MS, 0.0f, 1.0f);
  scalar[2] = gestureClamp(bboxW / GESTURE_NORM_SPAN_PX, 0.0f, 1.0f);
  scalar[3] = gestureClamp(bboxH / GESTURE_NORM_SPAN_PX, 0.0f, 1.0f);
  scalar[5] = gestureClamp((float)(count > 0 ? samples[count - 1].stroke : 0) /
                               GESTURE_NORM_STROKES,
                           0.0f, 1.0f);
  scalar[6] = gestureClamp(maxGapMs / GESTURE_NORM_GAP_MS, 0.0f, 1.0f);
  scalar[7] = durationMs > 0.0f ? gestureClamp(contactMs / durationMs, 0.0f, 1.0f) : 1.0f;
  scalar[15] = (bboxW + bboxH) > 1.0f
                   ? gestureClamp((bboxW - bboxH) / (bboxW + bboxH), -1.0f, 1.0f)
                   : 0.0f;

  if (hasExtent) {
    scalar[1] = gestureClamp(pathLen / GESTURE_NORM_PATH_PX, 0.0f, 1.0f);
    scalar[4] = pathLen > 0.5f ? gestureClamp(netDisp / pathLen, 0.0f, 1.0f) : 0.0f;
    scalar[8] = durationMs > 0.0f
                    ? gestureClamp((pathLen / durationMs) / GESTURE_NORM_SPEED_PX_MS, 0.0f, 1.0f)
                    : 0.0f;
    scalar[9] = gestureClamp(maxSpeed / GESTURE_NORM_MAX_SPEED_PX_MS, 0.0f, 1.0f);
    scalar[10] = gestureClamp(netDx / span, -1.0f, 1.0f);
    scalar[11] = gestureClamp(netDy / span, -1.0f, 1.0f);
    scalar[12] = gestureClamp(signedTurn / (4.0f * (float)M_PI), -1.0f, 1.0f);
    scalar[13] = gestureClamp(absTurn / (8.0f * (float)M_PI), 0.0f, 1.0f);
    scalar[14] = gestureClamp((float)reversals / GESTURE_NORM_REVERSALS, 0.0f, 1.0f);
  }
  // Otherwise the path-derived slots stay at the zero they were initialised to:
  // a stationary contact has no trajectory to describe.

  return true;
}

#endif
