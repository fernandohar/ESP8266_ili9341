The original scope of this project was to create a tamagotchi styled game console running
on ESP8266 with 240X320 ILI9341 TFT display module and XPT2046 touch panel.

The constrained hardware presents major challenges to the project:
  - Limited RAM in ESP8266 to perform screen buffering
  - Slow SPI bus speed creates visual scan line, and impairs the visual experience
  - Limited Flash size in ESP8266, reduces the number of images / background that can be stored
  - Limited GPIO pins, after connection of TFT display and touch module, we do not have enough pins for input buttons.

I have developed a custom 2D gaming engine to overcome the challenges.
The gaming engine consists of multiple major components:
  - rendering engine
  - Scene management
  - Sprite loading
  - input handling
  - gameloop (with ideas from deWiTTERS game loop)
  - sound/fx handling engine

With the original game engine came to a completion, the following component was incomplete: physical input buttons, a working sound handling engine.
These leads to the migration to more capable ESP32 hardware.

The additional GPIOs allows me to wire three (3) physical buttons, provides much better gaming experience. Multiple cores of ESP32 
allows one CPU core to handle game loop, rendering engine, input, and the other CPU core to handle sound.

These are the current state of the project:
  - simulation with WokWi simulator
  - compile project from platformIO

These are the current features provided in the project:
  - virtual pet (totoro themed), with two (2) growing stages
  - Emotion of the totoro reflects the current happiness/hunger status
  - Walking, dancing animation of totoro in pet home
  - persistence of pet status, coins
  - interaction with pet through touch
  - four (4) mini games - Catch an acron, Tic-Tac-Tao, Whack a mole, Cat bus crossing
  - Reward system, earn coins from mini games. Coins used to purchase food / pet supply
  - Grocery store for purchasing food, food eaten shows progress.
  - Resetting to clear/wipe persisted progress

Possible Future development (subject to changes):
  - Hardware (custom PCB)
    - Power management
    - Use of raw ESP32 module, without development board
    - ILI9341 display module without breakout board
  - Software
    - AI integration
      - TinyML
        - Train a tiny model to make a small, useful decision locally. 

        e.g. What should the player to do next?
        input:
          {
            "event" : "player returned"
            "pet": "Totoro",
            "hunger": 82,
            "happiness": 34,
            "energy": 71,
            "last_game": "WhackAMole",
            "last_score": 42,
            "hours_since_play": 5
          }
        output:
        {
          "message" : "You haven't played with me for a while! Want to catch some soot sprites?"
        }
        or
        {
          "message" : "I'm getting hungry... maybe we should visit the kitchen?"
        }

        e.g. After game celebration
        input:
        {
          "game": "whack",
          "score": 92,
          "previous_best": 40
        }
        output:
        {
          "message" : "Amazing! You destroyed the soot sprites!"
        }

    - Online AI (microsoft Foundry)
      - AI Companion
      - AI Game Director
      - Daily Quest Generator
      - Personality
      - Game Coach
        
