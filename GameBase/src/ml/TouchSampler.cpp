#include "TouchSampler.h"

#include <math.h>

static GestureEpisode s_episode;
// Distance covered inside strokes, which decides how long to hold the episode
// open for another stroke.
static float s_travelPx = 0.0f;
static uint8_t s_state = 0;  // SamplerState, kept as a plain byte for file scope
static bool s_ready = false;
static bool s_preview = false;
static unsigned long s_lastSampleMs = 0;
static unsigned long s_lastTouchMs = 0;
static unsigned long s_gapStartMs = 0;
static unsigned long s_contactStartMs = 0;
static uint16_t s_dropped = 0;

void TouchSampler::reset() {
  s_episode.sampleCount = 0;
  s_episode.strokeCount = 0;
  s_episode.durationMs = 0;
  s_episode.truncated = false;
  s_episode.startMs = 0;
  s_state = SAMPLER_IDLE;
  s_ready = false;
  s_preview = false;
  s_lastSampleMs = 0;
  s_lastTouchMs = 0;
  s_gapStartMs = 0;
  s_contactStartMs = 0;
  s_dropped = 0;
  s_travelPx = 0.0f;
}

bool TouchSampler::episodeReady() {
  return s_ready;
}

const GestureEpisode &TouchSampler::episode() {
  return s_episode;
}

const GestureEpisode &TouchSampler::working() {
  return s_episode;
}

void TouchSampler::consumeEpisode() {
  s_ready = false;
  s_preview = false;
  s_episode.sampleCount = 0;
  s_episode.strokeCount = 0;
  s_episode.durationMs = 0;
  s_episode.truncated = false;
}

bool TouchSampler::previewReady() {
  // Once the episode has closed there is a real result to act on, so the provisional
  // one stops being offered even if nobody collected it.
  return s_preview && !s_ready;
}

void TouchSampler::consumePreview() {
  s_preview = false;
}

bool TouchSampler::contactActive() {
  return s_state == SAMPLER_CONTACT;
}

unsigned long TouchSampler::contactHoldMs(unsigned long now) {
  if (s_state != SAMPLER_CONTACT || s_contactStartMs == 0) {
    return 0;
  }
  return now - s_contactStartMs;
}

float TouchSampler::episodeTravelPx() {
  return s_travelPx;
}

uint16_t TouchSampler::droppedSamples() {
  return s_dropped;
}

void TouchSampler::abortEpisode() {
  s_ready = false;
  s_preview = false;
  s_episode.sampleCount = 0;
  s_episode.strokeCount = 0;
  s_episode.durationMs = 0;
  s_episode.truncated = false;
  // Swallow the remainder of the contact so lifting off does not immediately
  // open a fresh episode from the tail of the same touch.
  s_state = (s_state == SAMPLER_CONTACT) ? SAMPLER_DRAIN : SAMPLER_IDLE;
}

void TouchSampler::beginEpisode(unsigned long now, int16_t x, int16_t y) {
  s_episode.sampleCount = 0;
  s_episode.strokeCount = 1;
  s_episode.durationMs = 0;
  s_episode.truncated = false;
  s_episode.startMs = now;
  s_preview = false;
  s_dropped = 0;
  s_travelPx = 0.0f;
  s_contactStartMs = now;
  touchFastResetStats();
  appendSample(now, x, y);
}

void TouchSampler::appendSample(unsigned long now, int16_t x, int16_t y) {
  s_lastTouchMs = now;
  if (s_episode.sampleCount >= GESTURE_MAX_SAMPLES) {
    s_episode.truncated = true;
    s_dropped++;
    return;
  }

  if (s_episode.sampleCount > 0) {
    const GestureSample &prev = s_episode.samples[s_episode.sampleCount - 1];
    if (prev.stroke == (uint8_t)(s_episode.strokeCount - 1)) {
      const float dx = (float)(x - prev.x);
      const float dy = (float)(y - prev.y);
      s_travelPx += sqrtf(dx * dx + dy * dy);
    }
  }

  GestureSample &sample = s_episode.samples[s_episode.sampleCount++];
  sample.x = x;
  sample.y = y;
  sample.t = (uint16_t)(now - s_episode.startMs);
  sample.stroke = (uint8_t)(s_episode.strokeCount - 1);
}

bool TouchSampler::episodeStationary() {
  return s_travelPx < GESTURE_TRAVEL_MOVING_PX;
}

void TouchSampler::closeEpisode(unsigned long now, bool truncated) {
  (void)now;
  if (truncated) {
    s_episode.truncated = true;
  }
  if (s_episode.sampleCount == 0) {
    s_state = SAMPLER_IDLE;
    return;
  }
  s_episode.durationMs = episodeElapsedMs();
  s_ready = true;
}

uint16_t TouchSampler::episodeElapsedMs() {
  // Spans the samples themselves, so trailing idle time is excluded.
  return s_episode.sampleCount == 0
             ? 0
             : s_episode.samples[s_episode.sampleCount - 1].t;
}

bool TouchSampler::pollDue(unsigned long now) {
  return s_lastSampleMs == 0 ||
         (now - s_lastSampleMs) >= GESTURE_SAMPLE_INTERVAL_MS;
}

void TouchSampler::poll(unsigned long now) {
  if (!pollDue(now)) {
    return;
  }
  s_lastSampleMs = now;

  uint16_t rawX = 0;
  uint16_t rawY = 0;
  bool touched = sampleTouchFast(&rawX, &rawY);
  int16_t x = (int16_t)rawX;
  int16_t y = (int16_t)rawY;

  switch (s_state) {
    case SAMPLER_IDLE:
      if (touched) {
        // A consumer that missed the previous episode loses it here rather than
        // blocking the new one. Consumers run on the 20 Hz game tick, so they have
        // 50 ms to collect; an episode that ended early still survives, because the
        // shortest gap between two deliberate taps measured in the captures is 64 ms.
        s_ready = false;
        beginEpisode(now, x, y);
        s_state = SAMPLER_CONTACT;
      }
      break;

    case SAMPLER_CONTACT:
      if (touched) {
        appendSample(now, x, y);
        if ((now - s_episode.startMs) >= GESTURE_MAX_EPISODE_MS) {
          closeEpisode(now, true);
          s_state = SAMPLER_DRAIN;
        }
      } else if ((now - s_lastTouchMs) >= GESTURE_STROKE_END_MS) {
        // Measure the gap from the last real contact, not from when the dropout
        // debounce expired, so the gap window means what it says.
        s_gapStartMs = s_lastTouchMs;
        s_contactStartMs = 0;
        if (!episodeStationary()) {
          s_state = SAMPLER_GAP;  // could still be growing into a multi-stroke brush
        } else if (s_episode.strokeCount >= 2) {
          // Two taps in the same spot: the episode is already as long as any
          // stationary gesture gets, so sitting out the gap window is pure latency.
          // Ending it here is the difference between a double poke that feels like a
          // double click and one that feels like a delayed one.
          //
          // The cost is that a stationary *three*-tap can never be one episode. No
          // class needs that, and unlike a timing rule this cannot mask a class the
          // model is supposed to tell apart: poke, double poke and long press all
          // still reach it intact.
          closeEpisode(now, false);
          s_state = SAMPLER_IDLE;
        } else {
          // One stroke, in one spot. Offer it now rather than making the player wait
          // out a window that only exists in case they tap again. Duration has to be
          // filled in for the features to be readable.
          s_episode.durationMs = episodeElapsedMs();
          s_preview = true;
          s_state = SAMPLER_GAP;
        }
      }
      break;

    case SAMPLER_GAP:
      if (touched) {
        if (s_episode.strokeCount >= GESTURE_MAX_STROKES) {
          closeEpisode(now, true);
          s_state = SAMPLER_DRAIN;
          break;
        }
        // Another stroke is coming, so the single-stroke reading offered on lift is
        // withdrawn. A consumer that already acted on it keeps that reaction; the
        // finished episode arrives separately as the double poke it turned out to be.
        s_preview = false;
        s_episode.strokeCount++;
        s_contactStartMs = now;
        appendSample(now, x, y);
        s_state = SAMPLER_CONTACT;
      } else if ((now - s_gapStartMs) >= GESTURE_EPISODE_GAP_MS ||
                 (now - s_episode.startMs) >= GESTURE_MAX_EPISODE_MS) {
        closeEpisode(now, (now - s_episode.startMs) >= GESTURE_MAX_EPISODE_MS);
        s_state = SAMPLER_IDLE;
      }
      break;

    case SAMPLER_DRAIN:
      if (!touched) {
        s_contactStartMs = 0;
        s_state = SAMPLER_IDLE;
      }
      break;

    default:
      s_state = SAMPLER_IDLE;
      break;
  }
}
