/*
 * EdgeGaze — Emotion Detection on ESP32-CAM
 * ==========================================
 * FreeRTOS tasks:
 *   Core 0 → CameraTask    (grab + preprocess frame)
 *   Core 1 → InferenceTask (TFLite INT8 inference)
 *   Core 1 → DisplayTask   (OLED animation)
 *   Core 1 → WebTask       (WiFi + HTTP server + SSE stream)
 *
 * Flash settings (Arduino IDE):
 *   Board  : AI-Thinker ESP32-CAM
 *   Partition scheme : Huge APP (3MB No OTA)
 *   PSRAM  : Enabled
 *   Speed  : 240MHz
 */

#include "Arduino.h"
#include "config.h"
#include "camera_task.h"
#include "inference_task.h"
#include "display_task.h"
#include "web_task.h"

// ── Shared FreeRTOS primitives ────────────────────────────────────────────────
QueueHandle_t  g_frame_queue;    // raw 48x48 grayscale bytes (camera → inference)
QueueHandle_t  g_result_queue;   // InferenceResult structs   (inference → display/web)
SemaphoreHandle_t g_i2c_mutex;   // protects I2C bus for OLED

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== EdgeGaze v1.0 ===");

    // Create queues
    g_frame_queue  = xQueueCreate(2, FRAME_BYTES);      // 2 frames buffered
    g_result_queue = xQueueCreate(8, sizeof(InferenceResult));
    g_i2c_mutex    = xSemaphoreCreateMutex();

    if (!g_frame_queue || !g_result_queue || !g_i2c_mutex) {
        Serial.println("[FATAL] Queue / mutex creation failed");
        while(1) delay(1000);
    }

    // Spawn tasks — order matters, camera must be ready before inference polls
    xTaskCreatePinnedToCore(displayTask,   "OLED",  4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(webTask,       "WEB",  16384, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(inferenceTask, "INF",  32768, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(cameraTask,    "CAM",   8192, NULL, 4, NULL, 0);

    Serial.println("[SETUP] All tasks spawned.");
}

void loop() {
    // Watchdog: print heap every 10s
    static uint32_t last = 0;
    if (millis() - last > 10000) {
        last = millis();
        Serial.printf("[HEAP] Free: %u  Min: %u\n",
                      esp_get_free_heap_size(),
                      esp_get_minimum_free_heap_size());
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}