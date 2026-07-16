#include "Input.h"

static GameInput inputState;
static bool prevLeft = false;
static bool prevRight = false;
static bool prevHome = false;

void Input::begin() {
  pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode(BTN_HOME_PIN, INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
}

void Input::update() {
  inputState.left = digitalRead(BTN_LEFT_PIN) == LOW;
  inputState.right = digitalRead(BTN_RIGHT_PIN) == LOW;
  inputState.home = digitalRead(BTN_HOME_PIN) == LOW;

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
