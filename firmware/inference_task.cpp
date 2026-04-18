/*
 * EdgeGaze — inference_task.cpp
 * Reads 48×48 grayscale frames from g_frame_queue, runs TFLite INT8
 * inference, and posts InferenceResult to g_result_queue.
 * Runs on Core 1, priority 3.
 */

#include "config.h"
#include "inference_task.h"
#include "model.h"   // auto-generated C array

// TensorFlow Lite Micro headers
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Tensor arena — must live in PSRAM on ESP32-CAM (DRAM too small)
// Adjust if you see kTfLiteError on AllocateTensors
#define TENSOR_ARENA_SIZE  (96 * 1024)  // 96KB

static uint8_t* tensor_arena = nullptr;

static tflite::MicroErrorReporter micro_error_reporter;
static tflite::ErrorReporter*     error_reporter = &micro_error_reporter;
static const tflite::Model*       tfl_model      = nullptr;
static tflite::MicroInterpreter*  interpreter    = nullptr;
static TfLiteTensor*              input_tensor   = nullptr;
static TfLiteTensor*              output_tensor  = nullptr;

static bool initTFLite() {
    // Allocate arena in PSRAM if available
    if (psramFound()) {
        tensor_arena = (uint8_t*)ps_malloc(TENSOR_ARENA_SIZE);
    } else {
        tensor_arena = (uint8_t*)malloc(TENSOR_ARENA_SIZE);
    }
    if (!tensor_arena) {
        Serial.println("[INF] Fatal: cannot allocate tensor arena");
        return false;
    }
    Serial.printf("[INF] Tensor arena: %u KB (%s)\n",
                  TENSOR_ARENA_SIZE / 1024,
                  psramFound() ? "PSRAM" : "DRAM");

    tfl_model = tflite::GetModel(g_model_data);
    if (tfl_model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("[INF] Model schema version mismatch");
        return false;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interp(
        tfl_model, resolver, tensor_arena, TENSOR_ARENA_SIZE, error_reporter);
    interpreter = &static_interp;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("[INF] AllocateTensors failed");
        return false;
    }

    input_tensor  = interpreter->input(0);
    output_tensor = interpreter->output(0);

    Serial.printf("[INF] Input  : dtype=%d shape=[%d,%d,%d,%d]\n",
                  input_tensor->type,
                  input_tensor->dims->data[0], input_tensor->dims->data[1],
                  input_tensor->dims->data[2], input_tensor->dims->data[3]);
    Serial.printf("[INF] Output : dtype=%d shape=[%d,%d]\n",
                  output_tensor->type,
                  output_tensor->dims->data[0], output_tensor->dims->data[1]);
    Serial.println("[INF] TFLite interpreter ready");
    return true;
}

void inferenceTask(void* pvParams) {
    Serial.println("[INF] Task started on Core 1");

    if (!initTFLite()) {
        Serial.println("[INF] Fatal: TFLite init failed. Task halting.");
        vTaskDelete(NULL);
        return;
    }

    // Fetch quantization params from input tensor
    float   input_scale  = input_tensor->params.scale;
    int32_t input_zp     = input_tensor->params.zero_point;
    float   output_scale = output_tensor->params.scale;
    int32_t output_zp    = output_tensor->params.zero_point;

    static uint8_t frame_buf[FRAME_BYTES];
    InferenceResult result;

    for (;;) {
        // Block until a frame arrives (max 200ms timeout)
        if (xQueueReceive(g_frame_queue, frame_buf, pdMS_TO_TICKS(200)) != pdTRUE) {
            continue;
        }

        // ── Quantize input (uint8 pixel → INT8 tensor) ────────────────────────
        int8_t* input_data = input_tensor->data.int8;
        for (int i = 0; i < FRAME_BYTES; i++) {
            float normalised = frame_buf[i] / 255.0f;
            int32_t q        = (int32_t)(normalised / input_scale) + input_zp;
            q = q < -128 ? -128 : (q > 127 ? 127 : q);
            input_data[i]    = (int8_t)q;
        }

        // ── Invoke ────────────────────────────────────────────────────────────
        uint32_t t0 = millis();
        if (interpreter->Invoke() != kTfLiteOk) {
            Serial.println("[INF] Invoke failed");
            continue;
        }
        result.inference_ms = millis() - t0;

        // ── Dequantize output ─────────────────────────────────────────────────
        int8_t* out_data = output_tensor->data.int8;
        float best_score = -1e9f;
        result.class_idx  = 0;

        for (int c = 0; c < NUM_CLASSES; c++) {
            float score = (out_data[c] - output_zp) * output_scale;
            result.scores[c] = score;
            if (score > best_score) {
                best_score      = score;
                result.class_idx = (uint8_t)c;
            }
        }
        result.confidence = best_score;

        Serial.printf("[INF] %s (%.1f%%) in %ums\n",
                      CLASS_LABELS[result.class_idx],
                      result.confidence * 100.0f,
                      result.inference_ms);

        // Post to both display and web tasks (non-blocking, overwrite old result)
        xQueueOverwrite(g_result_queue, &result);
    }
}