/*
  inference_engine.h  — EdgeWatch (fixed for TensorFlowLite_ESP32 older API)
  ---------------------------------------------------------------------------
  Compatible with TensorFlowLite_ESP32 library (older API) that uses:
    MicroInterpreter(model, resolver, uint8_t* buf, size_t buf_size, error_reporter)

  Class order matches Keras ImageDataGenerator alphabetical assignment:
    0=happy  1=neutral  2=sad  3=stressed
  LABEL_NAMES and LABEL_ICONS come from emotion_model.h — not redefined here.
*/

#pragma once

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "emotion_model.h"

#define TENSOR_ARENA_SIZE (280 * 1024)

static uint8_t tensorArena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));

static tflite::MicroErrorReporter  tflErrorReporter;
static tflite::ErrorReporter*      tflReporter    = &tflErrorReporter;
static const tflite::Model*        tflModel       = nullptr;
static tflite::MicroInterpreter*   tflInterpreter = nullptr;
static tflite::AllOpsResolver      tflResolver;

#define NUM_CLASSES 4

struct InferenceResult {
  uint8_t label;       // 0–3
  uint8_t confidence;  // 0–255
};

bool initInferenceEngine() {
  tflModel = tflite::GetModel(emotion_model_tflite);

  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("[TFLite] Schema mismatch: model=%d lib=%d\n",
                  tflModel->version(), TFLITE_SCHEMA_VERSION);
    return false;
  }

  tflInterpreter = new tflite::MicroInterpreter(
    tflModel,
    tflResolver,
    tensorArena,
    TENSOR_ARENA_SIZE,
    tflReporter
  );

  if (tflInterpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[TFLite] AllocateTensors() FAILED");
    Serial.printf("  Try increasing TENSOR_ARENA_SIZE (now %d KB)\n",
                  TENSOR_ARENA_SIZE / 1024);
    return false;
  }

  size_t used = tflInterpreter->arena_used_bytes();
  Serial.printf("[TFLite] Arena: %u bytes (%.1f KB)\n", used, used / 1024.0f);

  TfLiteTensor* inp = tflInterpreter->input(0);
  Serial.printf("[TFLite] Input: %dx%dx%d type=%d\n",
                inp->dims->data[1], inp->dims->data[2],
                inp->dims->data[3], inp->type);
  return true;
}

InferenceResult runInference(const uint8_t* frame, size_t frame_size) {
  InferenceResult result = { 1, 128 };

  TfLiteTensor* inp = tflInterpreter->input(0);
  TfLiteTensor* out = tflInterpreter->output(0);

  int8_t* in_data = inp->data.int8;
  for (size_t i = 0; i < frame_size; i++) {
    in_data[i] = (int8_t)((int16_t)frame[i] - 128);
  }

  unsigned long t0 = millis();
  if (tflInterpreter->Invoke() != kTfLiteOk) {
    Serial.println("[TFLite] Invoke() failed");
    return result;
  }
  Serial.printf("[TFLite] %lu ms\n", millis() - t0);

  int8_t* scores   = out->data.int8;
  int8_t  best     = -128;
  uint8_t best_idx = 0;
  for (uint8_t i = 0; i < NUM_CLASSES; i++) {
    if (scores[i] > best) { best = scores[i]; best_idx = i; }
  }

  result.label      = best_idx;
  result.confidence = (uint8_t)((int16_t)best + 128);
  return result;
}
