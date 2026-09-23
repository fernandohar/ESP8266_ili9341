#include "GesturePredictor.h"

#include "GestureFeatures.h"

#if defined(TINYML_GESTURE_INFERENCE)

#include <TensorFlowLite_ESP32.h>

#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// The generated array is static, so this is the only place that may include it.
#include "models/gesture_model.h"

namespace {

// Larger than the care model's 4 KB because this network is wider: 40 inputs and a
// 32-unit hidden layer against 8 inputs and 16 units. The care model taught the
// lesson that sizing this from arena_used_bytes() is wrong - AllocateTensors also
// carves its memory planner's own bookkeeping out of the same arena, and starving
// it fails with "Too many buffers" rather than an out-of-memory error. RAM is not
// scarce here, so this is deliberately generous.
constexpr int kArenaSize = 8 * 1024;
uint8_t s_arena[kArenaSize];

tflite::MicroInterpreter *s_interpreter = nullptr;
bool s_ready = false;
bool s_initTried = false;
uint32_t s_lastMicros = 0;
// Warned once rather than per gesture, since the cause is structural and the log
// would otherwise scroll away everything else.
bool s_warnedNonFinite = false;

void initOnce() {
  if (s_initTried) {
    return;
  }
  s_initTried = true;

  const tflite::Model *model = tflite::GetModel(gesture_model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println(F("gesture: model schema mismatch, gestures disabled"));
    return;
  }

  // Two dense layers plus the softmax output; dropout exists only during training.
  static tflite::MicroMutableOpResolver<2> resolver;
  if (resolver.AddFullyConnected() != kTfLiteOk || resolver.AddSoftmax() != kTfLiteOk) {
    Serial.println(F("gesture: op registration failed, gestures disabled"));
    return;
  }

  static tflite::MicroErrorReporter reporter;
  static tflite::MicroInterpreter interpreter(model, resolver, s_arena, kArenaSize,
                                              &reporter);
  if (interpreter.AllocateTensors() != kTfLiteOk) {
    Serial.println(F("gesture: AllocateTensors failed, raise kArenaSize"));
    return;
  }

  // The export script already refuses to emit a header whose feature count
  // disagrees with GestureFeatures.h. This catches the other direction: a header
  // committed before the extractor changed underneath it.
  const TfLiteTensor *in = interpreter.input(0);
  const TfLiteTensor *out = interpreter.output(0);
  if (in->type != kTfLiteFloat32 || out->type != kTfLiteFloat32 ||
      in->bytes != GESTURE_MODEL_FEATURE_COUNT * sizeof(float) ||
      out->bytes != GESTURE_MODEL_LABEL_COUNT * sizeof(float)) {
    Serial.println(F("gesture: unexpected tensor shape, gestures disabled"));
    return;
  }
  if (GESTURE_MODEL_FEATURE_COUNT != GESTURE_FEATURE_COUNT) {
    Serial.println(F("gesture: extractor and model disagree, gestures disabled"));
    return;
  }

  s_interpreter = &interpreter;
  s_ready = true;
  Serial.printf("gesture: model ready, arena %u/%u bytes\n",
                (unsigned)interpreter.arena_used_bytes(), (unsigned)kArenaSize);
}

}  // namespace

#endif  // TINYML_GESTURE_INFERENCE

static GesturePrediction s_lastRaw = {GESTURE_UNKNOWN, 0.0f, false};

GesturePrediction GesturePredictor::classify(const GestureEpisode &episode) {
  GesturePrediction result;
  result.label = GESTURE_UNKNOWN;
  result.confidence = 0.0f;
  result.recognised = false;

#if defined(TINYML_GESTURE_INFERENCE)
  initOnce();
  if (!s_ready) {
    return result;
  }

  float features[GESTURE_FEATURE_COUNT];
  if (!gestureExtractFeatures(episode.samples, episode.sampleCount, features)) {
    return result;
  }

  TfLiteTensor *input = s_interpreter->input(0);
  for (int i = 0; i < GESTURE_FEATURE_COUNT; i++) {
    input->data.f[i] = features[i];
  }

  const uint32_t started = micros();
  if (s_interpreter->Invoke() != kTfLiteOk) {
    return result;
  }
  s_lastMicros = micros() - started;

  const TfLiteTensor *output = s_interpreter->output(0);

  // Checked before the argmax, because NaN poisons every step downstream while
  // looking like a valid answer. No comparison against NaN is ever true, so the
  // argmax reports index 0, and `NaN < threshold` is false, so the confidence floor
  // waves it through. A broken model then presents as a confident wrong gesture -
  // which is exactly how a hybrid-quantized export failed here once.
  for (int i = 0; i < GESTURE_MODEL_LABEL_COUNT; i++) {
    if (!isfinite(output->data.f[i])) {
      s_lastRaw.label = GESTURE_UNKNOWN;
      s_lastRaw.confidence = 0.0f;
      s_lastRaw.recognised = false;
      if (!s_warnedNonFinite) {
        s_warnedNonFinite = true;
        Serial.println(F("gesture: model returned NaN, gestures ignored"));
      }
      return result;
    }
  }

  int best = 0;
  for (int i = 1; i < GESTURE_MODEL_LABEL_COUNT; i++) {
    if (output->data.f[i] > output->data.f[best]) {
      best = i;
    }
  }

  s_lastRaw.label = (uint8_t)best;
  s_lastRaw.confidence = output->data.f[best];
  s_lastRaw.recognised = false;

  if (output->data.f[best] < GESTURE_MIN_CONFIDENCE) {
    return result;
  }
  // An episode the model is confident is nothing in particular is still nothing in
  // particular. Reported separately from a rejection so the debug readout can tell
  // "confidently ignored" from "too unsure to act".
  if (best == GESTURE_UNKNOWN) {
    return result;
  }

  result.label = (uint8_t)best;
  result.confidence = output->data.f[best];
  result.recognised = true;
  s_lastRaw.recognised = true;
#else
  (void)episode;
#endif

  return result;
}

bool GesturePredictor::modelReady() {
#if defined(TINYML_GESTURE_INFERENCE)
  initOnce();
  return s_ready;
#else
  return false;
#endif
}

uint32_t GesturePredictor::lastInferenceMicros() {
#if defined(TINYML_GESTURE_INFERENCE)
  return s_lastMicros;
#else
  return 0;
#endif
}

GesturePrediction GesturePredictor::lastRaw() { return s_lastRaw; }
