/*
  EdgeWatch.ino
  -------------
  Real-time emotion classifier on ESP32-CAM using TFLite Micro + FreeRTOS.

  Model: 4 classes — Happy / Neutral / Sad / Stressed
  Size:  192.8 KB INT8 quantized

  FreeRTOS task layout:
    CaptureTask   Core 0  Priority 2  — grab frame, resize to 48×48 grayscale
    InferenceTask Core 1  Priority 3  — run TFLite model, push result
    DisplayTask   Core 0  Priority 1  — render label + bar on OLED

  Hardware:
    ESP32-CAM AI Thinker  +  1.3" OLED SSD1306
    OLED: SDA→GPIO14  SCL→GPIO15

  Libraries (install via Arduino IDE Library Manager):
    TensorFlowLite_ESP32
    Adafruit SSD1306
    Adafruit GFX Library

  Board: AI Thinker ESP32-CAM  |  Flash: 115200 baud
  Partition scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"  ← required for 192KB model
*/

#include "camera_utils.h"
#include "inference_engine.h"
#include "display_utils.h"

// ── Queue config ────────────────────────────────────────────────────────────

#define FRAME_PIXELS  (48 * 48)   // 2304 bytes per frame

// frameQueue  : uint8 pixel arrays  Capture → Inference
// resultQueue : InferenceResult     Inference → Display
QueueHandle_t frameQueue;
QueueHandle_t resultQueue;

// ── Task: Camera capture ────────────────────────────────────────────────────

void captureTask(void* pvParams) {
  // Allocate frame buffer in PSRAM
  uint8_t* buf = (uint8_t*) ps_malloc(FRAME_PIXELS);
  if (!buf) {
    Serial.println("[Capture] ps_malloc failed — not enough PSRAM");
    displayError("PSRAM alloc fail", "captureTask");
    vTaskDelete(NULL);
    return;
  }

  Serial.println("[Capture] Task started on Core 0");

  for (;;) {
    camera_fb_t* fb = esp_camera_fb_get();

    if (fb && fb->buf && fb->len > 0) {
      if (resizeToGrayscale48(fb->buf, fb->len, fb->width, fb->height, buf)) {
        // Drop oldest frame if queue full — inference is the bottleneck
        xQueueSend(frameQueue, buf, 0);
      }
      esp_camera_fb_return(fb);
    } else {
      Serial.println("[Capture] Empty frame");
    }

    vTaskDelay(pdMS_TO_TICKS(120));   // ~8 FPS cap, matches inference speed
  }
}

// ── Task: ML inference ──────────────────────────────────────────────────────

void inferenceTask(void* pvParams) {
  uint8_t* frame = (uint8_t*) ps_malloc(FRAME_PIXELS);
  if (!frame) {
    Serial.println("[Inference] ps_malloc failed");
    vTaskDelete(NULL);
    return;
  }

  Serial.println("[Inference] Task started on Core 1");

  for (;;) {
    // Block indefinitely until a frame arrives
    if (xQueueReceive(frameQueue, frame, portMAX_DELAY) == pdTRUE) {
      InferenceResult result = runInference(frame, FRAME_PIXELS);

      Serial.printf("[Inference] %s  conf=%d%%\n",
                    LABEL_NAMES[result.label],
                    result.confidence * 100 / 255);

      // Overwrite stale result if display hasn't caught up
      xQueueOverwrite(resultQueue, &result);
    }
  }
}

// ── Task: OLED display ───────────────────────────────────────────────────────

void displayTask(void* pvParams) {
  Serial.println("[Display] Task started on Core 0");

  displayInit();
  displaySplash();

  InferenceResult result = { 1, 128 };  // Neutral while waiting

  for (;;) {
    // Poll with 800ms timeout — re-renders last result if nothing new
    xQueueReceive(resultQueue, &result, pdMS_TO_TICKS(800));
    displayResult(result.label, result.confidence);
  }
}

// ── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n============================");
  Serial.println("  EdgeWatch  booting...");
  Serial.println("============================");

  // ── Camera ──
  if (!initCamera()) {
    Serial.println("FATAL: Camera init failed");
    while (true) { delay(1000); }
  }
  Serial.println("Camera: OK");

  // ── Model ──
  if (!initInferenceEngine()) {
    Serial.println("FATAL: Model init failed");
    Serial.println("Check: emotion_model.h is in firmware/EdgeWatch/");
    Serial.println("Check: partition scheme = Huge APP");
    while (true) { delay(1000); }
  }
  Serial.println("Model: OK");

  // ── Queues ──
  // frameQueue  : 2 slots (drop oldest on overflow — we want latest frame)
  // resultQueue : 1 slot xQueueOverwrite-style (always has latest result)
  frameQueue  = xQueueCreate(2, FRAME_PIXELS * sizeof(uint8_t));
  resultQueue = xQueueCreate(1, sizeof(InferenceResult));

  if (!frameQueue || !resultQueue) {
    Serial.println("FATAL: Queue creation failed");
    while (true) { delay(1000); }
  }

  // ── Tasks ──
  //                       func            name        stack    param  prio handle  core
  xTaskCreatePinnedToCore(captureTask,   "Capture",    4096,   NULL,   2,   NULL,   0);
  xTaskCreatePinnedToCore(inferenceTask, "Inference",  16384,  NULL,   3,   NULL,   1);
  xTaskCreatePinnedToCore(displayTask,   "Display",    4096,   NULL,   1,   NULL,   0);

  Serial.println("All tasks created. Running.");
  Serial.printf("Free heap: %u bytes\n", esp_get_free_heap_size());
  Serial.printf("Free PSRAM: %u bytes\n", esp_get_free_internal_heap_size());
}

void loop() {
  // Everything runs in FreeRTOS tasks — nothing here
  vTaskDelay(pdMS_TO_TICKS(10000));
}
