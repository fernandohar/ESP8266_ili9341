#include "SoundPlayer.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// The ESP32 core's tone()/noTone() forward every note to their own task, which
// starts a note with ledcAttachPin() -> ledc_get_duty(). That reads back a
// channel nothing has configured yet, so the IDF driver logs
// "ledc_get_duty(745): LEDC is not initialized" on the serial port. Claiming one
// channel here and configuring it in begin() means the peripheral is always
// initialised before a duty is read or written, and drops a task + queue hop per
// note.
static const uint8_t SOUND_LEDC_CHANNEL = 0;
static const uint8_t SOUND_LEDC_BITS = 10;
static const uint32_t SOUND_LEDC_IDLE_FREQ = 1000;
#endif

uint8_t SoundPlayer::_speakerPin = 255;
int SoundPlayer::_toneQueue[MAX_SOUND_TONE_SIZE];
int SoundPlayer::_durationQueue[MAX_SOUND_TONE_SIZE];
volatile int SoundPlayer::_queueCount = 0;
volatile int SoundPlayer::_queueHead = 0;
volatile int SoundPlayer::_queueTail = 0;
bool SoundPlayer::_playing = false;
bool SoundPlayer::_toneActive = false;
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

void SoundPlayer::startTone(int soundTone) {
#if defined(ARDUINO_ARCH_ESP32)
  ledcWriteTone(SOUND_LEDC_CHANNEL, (uint32_t)soundTone);
#else
  tone(_speakerPin, soundTone);
#endif
}

void SoundPlayer::stopTone() {
#if defined(ARDUINO_ARCH_ESP32)
  ledcWrite(SOUND_LEDC_CHANNEL, 0);
#else
  noTone(_speakerPin);
#endif
}

void SoundPlayer::servicePlayback() {
  if (_speakerPin == 255) {
    return;
  }

  unsigned long now = millis();

  if (_playing && now < _stopAtMs) {
    return;
  }

  if (_playing) {
    // Rests are queued as tone 0, so only silence the speaker when a note is
    // really sounding - never stop a tone that was never started.
    if (_toneActive) {
      stopTone();
      _toneActive = false;
    }
    _playing = false;
  }

  SoundNote note;
  if (!popNote(&note)) {
    return;
  }

  _stopAtMs = now + (unsigned long)note.durationMs;
  _playing = true;

  if (note.tone > 0) {
    startTone(note.tone);
    _toneActive = true;
  }
}

void SoundPlayer::begin(uint8_t speakerPin) {
  _speakerPin = speakerPin;
  pinMode(_speakerPin, OUTPUT);
  digitalWrite(_speakerPin, LOW);

#if defined(ARDUINO_ARCH_ESP32)
  ledcSetup(SOUND_LEDC_CHANNEL, SOUND_LEDC_IDLE_FREQ, SOUND_LEDC_BITS);
  ledcAttachPin(_speakerPin, SOUND_LEDC_CHANNEL);
  ledcWrite(SOUND_LEDC_CHANNEL, 0);

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
