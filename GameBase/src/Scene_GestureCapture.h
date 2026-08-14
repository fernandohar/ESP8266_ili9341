#ifndef _SCENE_GESTURECAPTURE_H_
#define _SCENE_GESTURECAPTURE_H_

#include <Arduino.h>
#include "GameScene.h"
#include "GameSceneIds.h"
#include "Input.h"
#include "ml/GestureEpisode.h"
#include "ml/TouchSampler.h"
#if defined(TINYML_GESTURE_INFERENCE)
#include "ml/GesturePredictor.h"
#endif

// Labelled gesture capture, for building the training set. The label is chosen
// *before* the gesture is performed, so every episode arrives already labelled
// and nothing has to be reconstructed afterwards.
//
// Each episode is streamed to Serial as one row per sampled point. Raw points
// rather than features, deliberately: the feature extractor will change several
// times while the model is being tuned, and re-recording hundreds of gestures
// each time it does is not a reasonable way to spend an evening.
//
//   pio run -e esp32-gesture-log -t upload
//   pio device monitor -b 115200 | python tinyml/download_serial.py --gestures \
//       >> tinyml/data/raw/gestures.csv
class Scene_GestureCapture : public GameScene {
  public:
    Scene_GestureCapture(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    }

    void update(boolean isTouching, boolean *needChangeScene, int *nextSceneIndex) {
      (void)isTouching;
      const GameInput &input = Input::current();

      if (input.homePressed) {
        *needChangeScene = true;
        *nextSceneIndex = SCENE_SETTINGS;
        return;
      }

      if (input.leftPressed) {
        cycleLabel(-1);
        return;
      }
      if (input.rightPressed) {
        cycleLabel(1);
        return;
      }

      if (TouchSampler::episodeReady()) {
        handleEpisode(TouchSampler::episode());
        TouchSampler::consumeEpisode();
        trailDrawn = 0;
        clearPad = true;
        requestRender();
      }
    }

    void render() {
      if (clearPad) {
        clearPad = false;
        drawPad();
        drawStatus();
#if defined(TINYML_GESTURE_INFERENCE)
        // In the pad rather than the cramped status row, because the pad has just
        // been cleared and is about to be drawn over by the next gesture anyway.
        drawPrediction();
#endif
        return;
      }
      drawTrail();
    }

    void initScene() {
      label = GESTURE_POKE;
      episodeCounter = 0;
      lastEpisodeId = 0;
      lastEpisodeLabel = 0;
      haveLastEpisode = false;
      trailDrawn = 0;
      clearPad = false;
      rejected = 0;
      showRejectNotice = false;
      lastPoints = 0;
      for (int i = 0; i < GESTURE_LABEL_COUNT; i++) {
        captured[i] = 0;
      }
      // Distinguishes capture sessions in an appended CSV: the episode counter
      // restarts at every boot, so on its own it would collide across runs.
      // Arduino's random() is unseeded and would hand out the same tag every
      // boot, hence the hardware RNG.
#if defined(ARDUINO_ARCH_ESP32)
      bootTag = (uint16_t)(esp_random() & 0xFFFF);
#else
      bootTag = (uint16_t)(millis() & 0xFFFF);
#endif
      if (bootTag == 0) {
        bootTag = 1;
      }

      TouchSampler::reset();

      uint16_t bg = bgColor();
      setBackgroundColor(bg);
      _tft->fillScreen(bg);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(rgb565(150, 200, 255), bg);
      _tft->drawString("GESTURE CAPTURE", SCREENWIDTH / 2, 12, 2);

      drawPrompt();
      drawPad();
      drawStatus();
      drawUndoButton();

      _tft->setTextDatum(MR_DATUM);
      _tft->setTextColor(rgb565(150, 158, 178), bg);
      _tft->drawString("L/R class  Home exit", SCREENWIDTH - 8, UNDO_Y + UNDO_H / 2, 1);
      _tft->setTextDatum(TL_DATUM);

      Serial.println(F("MLG,boot,episode,label,stroke,t_ms,x,y"));
    }

    void destroyScene() {
      TouchSampler::reset();
      GameScene::destroyScene();
    }

  private:
    static const int16_t PAD_X = 8;
    static const int16_t PAD_Y = 90;
    static const int16_t PAD_W = 224;
    static const int16_t PAD_H = 176;

    static const int16_t STATUS_Y = 272;

    static const int16_t UNDO_X = 8;
    static const int16_t UNDO_Y = 286;
    static const int16_t UNDO_W = 74;
    static const int16_t UNDO_H = 28;

    uint8_t label = GESTURE_POKE;
    uint16_t captured[GESTURE_LABEL_COUNT] = {};
    uint16_t bootTag = 0;
    uint16_t episodeCounter = 0;
    uint16_t lastEpisodeId = 0;
    uint8_t lastEpisodeLabel = 0;
    bool haveLastEpisode = false;
    uint8_t trailDrawn = 0;
    bool clearPad = false;

    // Set from the last accepted episode, shown so a bad sample rate or a
    // mis-segmented stroke is obvious while capturing rather than after.
    uint16_t lastPoints = 0;
    uint16_t lastDuration = 0;
    uint8_t lastStrokes = 0;
    bool lastTruncated = false;
    uint16_t lastPressureDrops = 0;
    uint16_t lastJitterDrops = 0;
    uint16_t rejected = 0;
    bool showRejectNotice = false;
    uint8_t rejectedStrokes = 0;
    uint16_t rejectedMs = 0;
#if defined(TINYML_GESTURE_INFERENCE)
    uint8_t predictedLabel = GESTURE_UNKNOWN;
    float predictedConfidence = 0.0f;
    bool predictedActedOn = false;
    bool havePrediction = false;
    uint16_t modelAgreed = 0;
    uint16_t modelScored = 0;
#endif

    uint16_t bgColor() {
      return rgb565(24, 26, 38);
    }

    void cycleLabel(int delta) {
      label = (uint8_t)((label + GESTURE_LABEL_COUNT + delta) % GESTURE_LABEL_COUNT);
      showRejectNotice = false;
      lastPoints = 0;
      addSound(NOTE_E5, noteDurationMs(16, 800));
      drawPrompt();
      clearPad = true;
      trailDrawn = 0;
      requestRender();
    }

    void drawPrompt() {
      uint16_t bg = bgColor();
      _tft->fillRect(0, 26, SCREENWIDTH, 60, bg);

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, bg);
      _tft->drawString(gestureLabelTitle(label), SCREENWIDTH / 2, 40, 4);

      _tft->setTextColor(rgb565(160, 200, 170), bg);
      _tft->drawString(gestureLabelIntent(label), SCREENWIDTH / 2, 62, 2);

      char line[32];
      snprintf(line, sizeof(line), "captured: %u", (unsigned)captured[label]);
      _tft->setTextColor(rgb565(200, 190, 120), bg);
      _tft->drawString(line, SCREENWIDTH / 2, 78, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawPad() {
      _tft->fillRect(PAD_X, PAD_Y, PAD_W, PAD_H, rgb565(14, 16, 24));
      _tft->drawRect(PAD_X, PAD_Y, PAD_W, PAD_H, rgb565(70, 80, 110));
    }

#if defined(TINYML_GESTURE_INFERENCE)
    void drawPrediction() {
      if (!havePrediction) {
        return;
      }

      const bool agreed = predictedActedOn && predictedLabel == label;
      uint16_t color;
      if (!predictedActedOn) {
        // Ignored, either as unknown or for want of confidence. Not a wrong answer.
        color = rgb565(150, 158, 178);
      } else if (agreed) {
        color = rgb565(120, 230, 140);
      } else {
        color = rgb565(240, 120, 120);
      }

      // Clamped rather than cast straight through: casting a non-finite float to
      // unsigned is undefined, and on this target it printed 4294967295%.
      float pct = predictedConfidence * 100.0f + 0.5f;
      if (!(pct >= 0.0f)) {
        pct = 0.0f;  // also catches NaN, for which every comparison is false
      } else if (pct > 100.0f) {
        pct = 100.0f;
      }

      char line[32];
      snprintf(line, sizeof(line), "%s %u%%", gestureLabelTitle(predictedLabel),
               (unsigned)pct);

      _tft->setTextDatum(TL_DATUM);
      _tft->setTextColor(color, rgb565(14, 16, 24));
      _tft->drawString(predictedActedOn ? line : "ignored", PAD_X + 8, PAD_Y + 8, 2);

      char detail[48];
      // The raw answer even when it was ignored, which is the interesting case: it
      // shows whether the gesture was misread or merely read without conviction.
      snprintf(detail, sizeof(detail), "best guess %s, agrees %u/%u", line,
               (unsigned)modelAgreed, (unsigned)modelScored);
      _tft->setTextColor(rgb565(150, 158, 178), rgb565(14, 16, 24));
      _tft->drawString(detail, PAD_X + 8, PAD_Y + 30, 1);
    }
#endif

    void drawStatus() {
      uint16_t bg = bgColor();
      _tft->fillRect(0, STATUS_Y - 6, SCREENWIDTH, 14, bg);

      char line[64];
      if (showRejectNotice) {
        snprintf(line, sizeof(line),
                 "needs %u str, got %u in %u ms  drop z%u  (%u rejected)",
                 (unsigned)gestureLabelMinStrokes(label), (unsigned)rejectedStrokes,
                 (unsigned)rejectedMs, (unsigned)lastPressureDrops,
                 (unsigned)rejected);
      } else if (lastPoints == 0) {
        snprintf(line, sizeof(line), "draw in the box above");
      } else {
        unsigned hz = lastDuration > 0 ? (unsigned)((lastPoints * 1000UL) / lastDuration) : 0;
        snprintf(line, sizeof(line), "%u pts %u ms %u str %u Hz  drop z%u j%u%s",
                 (unsigned)lastPoints, (unsigned)lastDuration, (unsigned)lastStrokes,
                 hz, (unsigned)lastPressureDrops, (unsigned)lastJitterDrops,
                 lastTruncated ? " CUT" : "");
      }

      uint16_t color = rgb565(150, 158, 178);
      if (showRejectNotice) {
        color = rgb565(240, 120, 120);
      } else if (lastTruncated) {
        color = rgb565(240, 160, 120);
      }

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(color, bg);
      _tft->drawString(line, SCREENWIDTH / 2, STATUS_Y, 1);
      _tft->setTextDatum(TL_DATUM);
    }

    void drawUndoButton() {
      uint16_t color = haveLastEpisode ? rgb565(150, 70, 70) : rgb565(60, 62, 76);
      _tft->fillRoundRect(UNDO_X, UNDO_Y, UNDO_W, UNDO_H, 6, color);
      _tft->drawRoundRect(UNDO_X, UNDO_Y, UNDO_W, UNDO_H, 6, rgb565(120, 130, 150));

      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_WHITE, color);
      _tft->drawString("UNDO", UNDO_X + UNDO_W / 2, UNDO_Y + UNDO_H / 2, 2);
      _tft->setTextDatum(TL_DATUM);
    }

    bool insidePad(int16_t x, int16_t y) {
      return x >= PAD_X && x < (PAD_X + PAD_W) && y >= PAD_Y && y < (PAD_Y + PAD_H);
    }

    bool insideUndo(int16_t x, int16_t y) {
      return x >= UNDO_X && x < (UNDO_X + UNDO_W) &&
             y >= UNDO_Y && y < (UNDO_Y + UNDO_H);
    }

    // Everything outside the pad is chrome, so an episode that started there is a
    // UI tap rather than a gesture. Routing it through the same episode path
    // costs the tap the gap-window delay but keeps one touch code path.
    void handleEpisode(const GestureEpisode &ep) {
      if (ep.sampleCount == 0) {
        return;
      }

      int16_t firstX = ep.samples[0].x;
      int16_t firstY = ep.samples[0].y;

      if (!insidePad(firstX, firstY)) {
        if (insideUndo(firstX, firstY)) {
          undoLast();
        }
        return;
      }

      if (ep.strokeCount < gestureLabelMinStrokes(label)) {
        rejectEpisode(ep);
        return;
      }

      recordEpisode(ep);
    }

#if defined(TINYML_GESTURE_INFERENCE)
    void classifyForDisplay(const GestureEpisode &ep) {
      GesturePredictor::classify(ep);
      GesturePrediction raw = GesturePredictor::lastRaw();
      predictedLabel = raw.label;
      predictedConfidence = raw.confidence;
      predictedActedOn = raw.recognised;
      havePrediction = true;
      if (raw.recognised && raw.label == label) {
        modelAgreed++;
      }
      modelScored++;
    }
#endif

    // Nothing is logged, so a fumbled gesture cannot become a mislabelled row. What
    // was seen instead is kept and shown: for a double poke, "got 1" is the whole
    // diagnosis - the second tap either never reached the pressure threshold or never
    // separated from the first, and no amount of retraining or window-widening can
    // recover a gesture the panel recorded as one stroke.
    void rejectEpisode(const GestureEpisode &ep) {
      addSound(NOTE_A3, noteDurationMs(16, 800));
      addSound(NOTE_F3, noteDurationMs(16, 800));
      rejected++;
      lastPoints = 0;
      rejectedStrokes = ep.strokeCount;
      rejectedMs = ep.durationMs;
      touchFastStats(&lastPressureDrops, &lastJitterDrops);
      showRejectNotice = true;
    }

    void recordEpisode(const GestureEpisode &ep) {
      episodeCounter++;

      for (uint8_t i = 0; i < ep.sampleCount; i++) {
        const GestureSample &s = ep.samples[i];
        Serial.print(F("MLG,"));
        Serial.print(bootTag);
        Serial.print(F(","));
        Serial.print(episodeCounter);
        Serial.print(F(","));
        Serial.print(gestureLabelName(label));
        Serial.print(F(","));
        Serial.print(s.stroke);
        Serial.print(F(","));
        Serial.print(s.t);
        Serial.print(F(","));
        Serial.print(s.x);
        Serial.print(F(","));
        Serial.println(s.y);
      }

      showRejectNotice = false;
      captured[label]++;
      lastEpisodeId = episodeCounter;
#if defined(TINYML_GESTURE_INFERENCE)
      // Built from the episode that was just logged, so the reading on screen is
      // the model's answer to the exact gesture the label describes. Right and
      // wrong both matter: this is a live confusion matrix.
      classifyForDisplay(ep);
#endif
      lastEpisodeLabel = label;
      haveLastEpisode = true;

      lastPoints = ep.sampleCount;
      lastDuration = ep.durationMs;
      lastStrokes = ep.strokeCount;
      lastTruncated = ep.truncated;
      touchFastStats(&lastPressureDrops, &lastJitterDrops);

      addSound(NOTE_C5, noteDurationMs(16, 800));
      drawPrompt();
      drawUndoButton();
    }

    // The rows are already on the wire, so undo emits a marker the parser uses to
    // drop the whole episode. Mis-drawn gestures are frequent, and training on
    // them teaches the model your mistakes.
    void undoLast() {
      if (!haveLastEpisode) {
        addSound(NOTE_A3, noteDurationMs(16, 800));
        return;
      }

      Serial.print(F("MLG,"));
      Serial.print(bootTag);
      Serial.print(F(","));
      Serial.print(lastEpisodeId);
      Serial.println(F(",DISCARD,0,0,0,0"));

      if (captured[lastEpisodeLabel] > 0) {
        captured[lastEpisodeLabel]--;
      }
      haveLastEpisode = false;
      lastPoints = 0;

      addSound(NOTE_G3, noteDurationMs(16, 800));
      drawPrompt();
      drawUndoButton();
    }

    // Incremental: only the segments added since the last render are stroked, so
    // the trail costs a few short lines per frame.
    void drawTrail() {
      const GestureEpisode &ep = TouchSampler::working();
      if (ep.sampleCount == 0) {
        trailDrawn = 0;
        return;
      }
      if (trailDrawn >= ep.sampleCount) {
        return;
      }

      uint16_t color = rgb565(120, 220, 255);
      for (uint8_t i = trailDrawn; i < ep.sampleCount; i++) {
        const GestureSample &s = ep.samples[i];
        if (!insidePad(s.x, s.y)) {
          continue;
        }
        if (i == 0 || ep.samples[i - 1].stroke != s.stroke ||
            !insidePad(ep.samples[i - 1].x, ep.samples[i - 1].y)) {
          _tft->fillCircle(s.x, s.y, 2, rgb565(255, 220, 120));
        } else {
          const GestureSample &prev = ep.samples[i - 1];
          _tft->drawLine(prev.x, prev.y, s.x, s.y, color);
        }
      }
      trailDrawn = ep.sampleCount;
    }
};

#endif
