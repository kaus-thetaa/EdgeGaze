/*
  inference_engine.h  — EdgeWatch
  --------------------------------
  TFLite Micro wrapper for the 4-class emotion model.
  Classes (alphabetical = Keras generator order):
    0 = Happy
    1 = Neutral
    2 = Sad
    3 = Stressed

  IMPORTANT: emotion_model.h must be in the same folder.
  Generate it with: python convert_to_tflite.py
*/

#pragma once

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "emotion_model.h"

// 192KB model + activations — increase by 16KB if you see AllocateTensors() fail
#define TENSOR_ARENA_SIZE (280 * 1024)

static uint8_t tensorArena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));

static const tflite::Model*        tflModel       = nullptr;
static tflite::MicroInterpreter*   tflInterpreter = nullptr;
static tflite::MicroMutableOpResolver<12> tflResolver;

// Must match the alphabetical order Keras ImageDataGenerator assigned
// (happy=0, neutral=1, sad=2, stressed=3)
const char* const LABEL_NAMES[] = { "Happy",   "Neutral", "Sad",  "Stressed" };
const char* const LABEL_ICONS[] = { ":)",       ":|",      ":C",   ":("       };
#define NUM_CLASSES 4

struct InferenceResult {
  uint8_t label;       // 0–3
  uint8_t confidence;  // 0–255  (maps to 0–100%)
};

// ── Init ────────────────────────────────────────────────────────────────────

bool initInferenceEngine() {
  tflModel = tflite::GetModel(emotion_model_tflite);

  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("[TFLite] Schema mismatch: got %d, expected %d\n",
                  tflModel->version(), TFLITE_SCHEMA_VERSION);
    return false;
  }

  // Register every op the model uses
  tflResolver.AddConv2D();
  tflResolver.AddDepthwiseConv2D();
  tflResolver.AddMaxPool2D();
  tflResolver.AddFullyConnected();
  tflResolver.AddSoftmax();
  tflResolver.AddReshape();
  tflResolver.AddMean();          // GlobalAveragePooling2D
  tflResolver.AddAdd();           // BatchNorm fusion
  tflResolver.AddMul();           // BatchNorm fusion
  tflResolver.AddQuantize();
  tflResolver.AddDequantize();

  tflInterpreter = new tflite::MicroInterpreter(
    tflModel, tflResolver, tensorArena, TENSOR_ARENA_SIZE
  );

  if (tflInterpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[TFLite] AllocateTensors() FAILED");
    Serial.printf("  Try increasing TENSOR_ARENA_SIZE (currently %d KB)\n",
                  TENSOR_ARENA_SIZE / 1024);
    return false;
  }

  size_t used = tflInterpreter->arena_used_bytes();
  Serial.printf("[TFLite] Arena used: %u bytes (%.1f KB)\n", used, used / 1024.0f);

  TfLiteTensor* input = tflInterpreter->input(0);
  Serial.printf("[TFLite] Input: %dx%dx%d  type=%d\n",
                input->dims->data[1], input->dims->data[2],
                input->dims->data[3], input->type);

  return true;
}

// ── Inference ────────────────────────────────────────────────────────────────

InferenceResult runInference(const uint8_t* frame, size_t frame_size) {
  InferenceResult result = { 1, 128 };  // default: Neutral

  TfLiteTensor* input  = tflInterpreter->input(0);
  TfLiteTensor* output = tflInterpreter->output(0);

  // uint8 pixels (0–255) → int8 by subtracting 128 (zero-point for symmetric INT8)
  int8_t* in = input->data.int8;
  for (size_t i = 0; i < frame_size; i++) {
    in[i] = (int8_t)((int16_t)frame[i] - 128);
  }

  unsigned long t0 = millis();
  if (tflInterpreter->Invoke() != kTfLiteOk) {
    Serial.println("[TFLite] Invoke() failed");
    return result;
  }
  Serial.printf("[TFLite] Inference: %lu ms\n", millis() - t0);

  // Find argmax across 4 classes
  int8_t* out = output->data.int8;
  int8_t  best_score = -128;
  uint8_t best_label = 0;
  for (uint8_t i = 0; i < NUM_CLASSES; i++) {
    if (out[i] > best_score) {
      best_score = out[i];
      best_label = i;
    }
  }

  result.label      = best_label;
  result.confidence = (uint8_t)((int16_t)best_score + 128);  // → 0–255
  return result;
}
