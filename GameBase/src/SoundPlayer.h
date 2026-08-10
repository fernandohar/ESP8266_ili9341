#ifndef _SOUNDPLAYER_H_
#define _SOUNDPLAYER_H_

#include <Arduino.h>

#define MAX_SOUND_TONE_SIZE 200

class SoundPlayer {
  public:
    static void begin(uint8_t speakerPin);
    static bool enqueue(int soundTone, int soundDurationMs);
#if !defined(ARDUINO_ARCH_ESP32)
    static void update();
#endif

  private:
    struct SoundNote {
      int tone;
      int durationMs;
    };

    static bool pushNote(int soundTone, int soundDurationMs);
    static bool popNote(SoundNote *note);
    static void servicePlayback();
    static void startTone(int soundTone);
    static void stopTone();

#if defined(ARDUINO_ARCH_ESP32)
    static void soundTask(void *param);
#endif

    static uint8_t _speakerPin;
    static int _toneQueue[MAX_SOUND_TONE_SIZE];
    static int _durationQueue[MAX_SOUND_TONE_SIZE];
    static volatile int _queueCount;
    static volatile int _queueHead;
    static volatile int _queueTail;
    static bool _playing;
    static bool _toneActive;
    static unsigned long _stopAtMs;

#if defined(ARDUINO_ARCH_ESP32)
    static TaskHandle_t _taskHandle;
    static portMUX_TYPE _queueMux;
#endif
};

#endif
