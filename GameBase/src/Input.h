#ifndef _INPUT_H_
#define _INPUT_H_

#include <Arduino.h>

// Physical buttons below the screen (active LOW with internal pull-up).
#define BTN_LEFT_PIN  13
#define BTN_HOME_PIN  27
#define BTN_RIGHT_PIN 14

struct GameInput {
  bool left = false;
  bool right = false;
  bool home = false;
  bool leftPressed = false;
  bool rightPressed = false;
  bool homePressed = false;
};

class Input {
  public:
    static void begin();
    static void update();
    static void syncEdges();
    static const GameInput &current();
};

#endif
