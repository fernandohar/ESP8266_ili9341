#ifndef _TOUCHSAMPLER_H_
#define _TOUCHSAMPLER_H_

#include <Arduino.h>
#include "GestureEpisode.h"

// Defined per touch backend in main.cpp: one delay-free read, unlike the
// averaged-and-settled TFT_eSPI getTouch() the game tick uses.
extern bool sampleTouchFast(uint16_t *x, uint16_t *y);
// Reads rejected mid-stroke by the pressure and jitter guards, counted per
// episode so a capture session shows which guard is dropping samples.
extern void touchFastResetStats();
extern void touchFastStats(uint16_t *pressureDrops, uint16_t *jitterDrops);

// Samples the panel far faster than the 20 Hz game tick and cuts the stream into
// gesture episodes. Two consumers are expected to share it:
//
//  - a live tier that reacts while the finger is still down (dragging the pet
//    cannot wait for the episode to close), which claims a contact by calling
//    abortEpisode() so the classifier does not also fire on it, and
//  - an episode tier that classifies discrete commands after the finger lifts.
//
// State is global because sampling has to continue across scene boundaries and
// there is only ever one panel.
class TouchSampler {
  public:
    // Call as often as possible; internally rate-limited to
    // GESTURE_SAMPLE_INTERVAL_MS, so extra calls are nearly free.
    static void poll(unsigned long now);

    // Whether poll() would actually read the panel rather than return immediately.
    // Lets a caller that has to prepare something first - a renderer holding the SPI
    // bus - pay that cost only when there is a sample to take.
    static bool pollDue(unsigned long now);

    // Clears any part-built episode. Call when entering or leaving a scene so a
    // touch that spans the transition cannot be recorded as a gesture.
    static void reset();

    static bool episodeReady();
    static const GestureEpisode &episode();
    static void consumeEpisode();

    // A stationary episode, offered for classification the moment the finger lifts
    // instead of when the episode closes. Without this the gap window is input
    // latency, and the window has to be long enough for a slow double poke (550 ms),
    // which is far too long to wait before reacting to a single tap.
    //
    // It is provisional: a second tap inside the window withdraws the offer and the
    // episode goes on to close as a double poke. So a consumer that acts on a preview
    // gets the first tap's reaction *and then* the double tap's, in that order, which
    // is what a desktop click-then-double-click does too.
    //
    // Only stationary episodes are offered, because there the set of possibilities is
    // benign: one stroke standing still is a poke or a long press, and the only thing
    // it can grow into is a double poke. A travelled first stroke would preview as a
    // swipe and could grow into a brush, which is a different action, so those still
    // wait for the episode to close.
    //
    // Consumers that must see whole episodes only - the capture screen - ignore this
    // and keep using episodeReady().
    static bool previewReady();
    // Withdraws the offer without disturbing the episode, which may still be growing.
    static void consumePreview();

    // Live tier: true while the panel is being touched.
    static bool contactActive();
    // How long the current contact has been held, 0 when nothing is touching.
    static unsigned long contactHoldMs(unsigned long now);
    // Distance the finger has covered so far in this episode, summed within strokes.
    // Lets the live tier tell a finger being held still from one being dragged over
    // the same spot for the same length of time.
    static float episodeTravelPx();

    // Abandons the episode in progress and ignores the rest of this contact.
    // Used both when the live tier claims a contact and when a touch lands on a
    // UI control that is not part of the gesture area.
    static void abortEpisode();

    // The working buffer, for drawing a live trail. While episodeReady() is true
    // this is the finished episode.
    static const GestureEpisode &working();

    // Samples dropped because the buffer filled, for diagnostics.
    static uint16_t droppedSamples();

  private:
    enum SamplerState : uint8_t {
      SAMPLER_IDLE = 0,   // nothing touching, no episode open
      SAMPLER_CONTACT,    // finger down, stroke in progress
      SAMPLER_GAP,        // finger up, episode still open for another stroke
      SAMPLER_DRAIN       // episode closed or abandoned, wait for finger-up
    };

    static void beginEpisode(unsigned long now, int16_t x, int16_t y);
    static void appendSample(unsigned long now, int16_t x, int16_t y);
    static void closeEpisode(unsigned long now, bool truncated);
    // True while the episode has stayed in one place, so it can still only be a poke,
    // a double poke or a long press.
    static bool episodeStationary();
    // Length of the episode so far, as the feature extractor measures it.
    static uint16_t episodeElapsedMs();
};

#endif
