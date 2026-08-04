#include "Input.h"

static GameInput inputState;
static bool prevLeft = false;
static bool prevRight = false;
static bool prevHome = false;

// Integrator debounce: a reading must stay at the same level for DEBOUNCE_COUNTS
// consecutive polls (update() runs once per game tick, ~50 ms) before it flips the
// stable state. Hysteresis (only commit at the 0 / MAX rails, otherwise hold)
// rejects the random single-tick glitches seen when noise couples into a button
// line, so a brief spike can never register as a press.
//
// At 1 a single poll (~50 ms) commits the press immediately: maximally
// responsive (matches the old raw read + edge detection) with no multi-poll
// glitch rejection. Bump back up if button-line noise causes false presses.
#define DEBOUNCE_COUNTS 1

static uint8_t leftCount = 0;
static uint8_t rightCount = 0;
static uint8_t homeCount = 0;

static bool debounce(uint8_t &counter, bool rawPressed, bool prevStable) {
  if (rawPressed) {
    if (counter < DEBOUNCE_COUNTS) counter++;
  } else {
    if (counter > 0) counter--;
  }
  if (counter >= DEBOUNCE_COUNTS) return true;   // held long enough -> pressed
  if (counter == 0) return false;                // released long enough -> up
  return prevStable;                             // in between -> keep last state
}

void Input::begin() {
  pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode(BTN_HOME_PIN, INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
}

void Input::update() {
  inputState.left = debounce(leftCount, digitalRead(BTN_LEFT_PIN) == LOW, inputState.left);
  inputState.right = debounce(rightCount, digitalRead(BTN_RIGHT_PIN) == LOW, inputState.right);
  inputState.home = debounce(homeCount, digitalRead(BTN_HOME_PIN) == LOW, inputState.home);

  inputState.leftPressed = inputState.left && !prevLeft;
  inputState.rightPressed = inputState.right && !prevRight;
  inputState.homePressed = inputState.home && !prevHome;

  prevLeft = inputState.left;
  prevRight = inputState.right;
  prevHome = inputState.home;
}

void Input::syncEdges() {
  prevLeft = inputState.left;
  prevRight = inputState.right;
  prevHome = inputState.home;
}

const GameInput &Input::current() {
  return inputState;
}
