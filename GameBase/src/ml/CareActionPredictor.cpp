#include "CareActionPredictor.h"

#include "CareActionRules.h"

#if defined(TINYML_INFERENCE)

#include <TensorFlowLite_ESP32.h>

#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// The generated arrays are static, so this is the only place that may include it.
#include "models/care_action_model.h"

namespace {

// Do not size this from the `arena_used_bytes()` figure init prints: that counts
// only the tensors that survive planning, while AllocateTensors also carves its
// memory planner's bookkeeping out of the same arena. Squeezing this to 1280 for
// a model reporting 916 bytes used made the planner fail with "Too many buffers
// (max is 4)" — its scratch space, and so the number of buffers it can track,
// scales with what is left over. 4 KB is verified on hardware; if it is ever too
// small, AllocateTensors fails and every prediction falls back to the rules
// rather than crashing.
constexpr int kArenaSize = 4 * 1024;
uint8_t s_arena[kArenaSize];

tflite::MicroInterpreter *s_interpreter = nullptr;
bool s_ready = false;
bool s_initTried = false;
uint32_t s_lastMicros = 0;

// Nothing here throws or aborts: any failure leaves s_ready false, which the
// caller reads as "use the rules".
void initOnce() {
  if (s_initTried) {
    return;
  }
  s_initTried = true;

  const tflite::Model *model = tflite::GetModel(care_action_model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println(F("ML: model schema mismatch, using care rules"));
    return;
  }

  // FullyConnected covers the three dense layers, Softmax the output. A new
  // layer type means bumping this count and registering the op.
  static tflite::MicroMutableOpResolver<2> resolver;
  if (resolver.AddFullyConnected() != kTfLiteOk || resolver.AddSoftmax() != kTfLiteOk) {
    Serial.println(F("ML: op registration failed, using care rules"));
    return;
  }

  static tflite::MicroErrorReporter reporter;
  static tflite::MicroInterpreter interpreter(model, resolver, s_arena, kArenaSize,
                                              &reporter);
  if (interpreter.AllocateTensors() != kTfLiteOk) {
    Serial.println(F("ML: AllocateTensors failed, raise kArenaSize"));
    return;
  }

  // The header is generated, so a stale regeneration is the likely cause of a
  // shape mismatch. Checking here beats reading past the tensor at runtime.
  const TfLiteTensor *in = interpreter.input(0);
  const TfLiteTensor *out = interpreter.output(0);
  if (in->type != kTfLiteFloat32 || out->type != kTfLiteFloat32 ||
      in->bytes != CARE_MODEL_FEATURE_COUNT * sizeof(float) ||
      out->bytes != CARE_MODEL_LABEL_COUNT * sizeof(float)) {
    Serial.println(F("ML: unexpected tensor shape, using care rules"));
    return;
  }

  s_interpreter = &interpreter;
  s_ready = true;
  Serial.printf("ML: care model ready, arena %u/%u bytes\n",
                (unsigned)interpreter.arena_used_bytes(), (unsigned)kArenaSize);
}

// Runs the network on `sample`. Returns false — leaving `action` and
// `confidence` untouched — when the model is unavailable, fails, or is not sure
// enough to be trusted, all of which mean the rule answer stands.
bool runModel(const GameplaySample &sample, CareAction *action, float *confidence) {
  initOnce();
  if (!s_ready) {
    return false;
  }

  // Same order as FEATURES in tinyml/prepare_dataset.py, and standardized the
  // same way training did — the network never saw raw 0..1 values.
  const float raw[CARE_MODEL_FEATURE_COUNT] = {
      sample.hungerNorm,     sample.happinessNorm,  sample.excitementNorm,
      sample.cleanNorm,      sample.isUnhappy,      sample.lastGameIdNorm,
      sample.lastOutcomeWin, sample.sessionGamesNorm};

  TfLiteTensor *input = s_interpreter->input(0);
  for (int i = 0; i < CARE_MODEL_FEATURE_COUNT; i++) {
    input->data.f[i] =
        (raw[i] - care_action_scaler_mean[i]) / care_action_scaler_scale[i];
  }

  const uint32_t started = micros();
  if (s_interpreter->Invoke() != kTfLiteOk) {
    return false;
  }
  s_lastMicros = micros() - started;

  const TfLiteTensor *output = s_interpreter->output(0);
  int best = 0;
  for (int i = 1; i < CARE_MODEL_LABEL_COUNT; i++) {
    if (output->data.f[i] > output->data.f[best]) {
      best = i;
    }
  }
  if (output->data.f[best] < CARE_PREDICT_MIN_CONFIDENCE) {
    return false;
  }

  // LABELS in the exporter is ordered to match the CareAction enum index for
  // index, which is what makes this cast safe.
  *action = (CareAction)best;
  *confidence = output->data.f[best];
  return true;
}

}  // namespace

#endif  // TINYML_INFERENCE

CarePrediction CareActionPredictor::predict(const GameplaySample &sample) {
  CarePrediction result;
  result.action = CareActionRules::suggest(sample);
  result.confidence = 0.0f;
  result.fromModel = false;

#if defined(TINYML_INFERENCE)
  result.fromModel = runModel(sample, &result.action, &result.confidence);
#endif

  // Last word on both paths: a suggestion the player cannot afford to act on is
  // no use, however confident the model was about it.
  result.action = CareActionRules::affordableAlternative(result.action);
  return result;
}

bool CareActionPredictor::modelReady() {
#if defined(TINYML_INFERENCE)
  initOnce();
  return s_ready;
#else
  return false;
#endif
}

uint32_t CareActionPredictor::lastInferenceMicros() {
#if defined(TINYML_INFERENCE)
  return s_lastMicros;
#else
  return 0;
#endif
}
