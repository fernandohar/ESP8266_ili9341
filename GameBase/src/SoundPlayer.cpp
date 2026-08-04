#include "SoundPlayer.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

uint8_t SoundPlayer::_speakerPin = 255;
int SoundPlayer::_toneQueue[MAX_SOUND_TONE_SIZE];
int SoundPlayer::_durationQueue[MAX_SOUND_TONE_SIZE];
volatile int SoundPlayer::_queueCount = 0;
volatile int SoundPlayer::_queueHead = 0;
volatile int SoundPlayer::_queueTail = 0;
bool SoundPlayer::_playing = false;
unsigned long SoundPlayer::_stopAtMs = 0;

#if defined(ARDUINO_ARCH_ESP32)
TaskHandle_t SoundPlayer::_taskHandle = NULL;
portMUX_TYPE SoundPlayer::_queueMux = portMUX_INITIALIZER_UNLOCKED;
#endif

bool SoundPlayer::pushNote(int soundTone, int soundDurationMs) {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_queueMux);
#endif

  if (_queueCount >= MAX_SOUND_TONE_SIZE) {
#if defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL(&_queueMux);
#endif
    return false;
  }

  _toneQueue[_queueTail] = soundTone;
  _durationQueue[_queueTail] = soundDurationMs;
  _queueTail = (_queueTail + 1) % MAX_SOUND_TONE_SIZE;
  _queueCount++;

#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&_queueMux);
#endif
  return true;
}

bool SoundPlayer::popNote(SoundNote *note) {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_queueMux);
#endif

  if (_queueCount == 0) {
#if defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL(&_queueMux);
#endif
    return false;
  }

  note->tone = _toneQueue[_queueHead];
  note->durationMs = _durationQueue[_queueHead];
  _queueHead = (_queueHead + 1) % MAX_SOUND_TONE_SIZE;
  _queueCount--;

#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&_queueMux);
#endif
  return true;
}

void SoundPlayer::servicePlayback() {
  unsigned long now = millis();

  if (_playing && now < _stopAtMs) {
    return;
  }

  if (_playing) {
    noTone(_speakerPin);
    _playing = false;
  }

  SoundNote note;
  if (!popNote(&note)) {
    return;
  }

  _stopAtMs = now + (unsigned long)note.durationMs;
  _playing = true;

  if (note.tone != 0) {
    // Let SoundPlayer control note length via noTone(); ESP32's tone(duration)
    // also blocks its own tone task and can fight this scheduler.
    tone(_speakerPin, note.tone);
  }
}

void SoundPlayer::begin(uint8_t speakerPin) {
  _speakerPin = speakerPin;
  pinMode(_speakerPin, OUTPUT);

#if defined(ARDUINO_ARCH_ESP32)
  if (_taskHandle == NULL) {
    xTaskCreatePinnedToCore(
      soundTask,
      "sound",
      2048,
      NULL,
      2,
      &_taskHandle,
      1
    );
  }
#endif
}

bool SoundPlayer::enqueue(int soundTone, int soundDurationMs) {
  if (!pushNote(soundTone, soundDurationMs)) {
    Serial.println("sound dropped");
    return false;
  }
  return true;
}

#if defined(ARDUINO_ARCH_ESP32)
void SoundPlayer::soundTask(void *param) {
  (void)param;

  for (;;) {
    servicePlayback();
    vTaskDelay(1);
  }
}
#else
void SoundPlayer::update() {
  servicePlayback();
}
#endif
