/*
 * EdgeGaze — config.h
 * Edit WIFI_SSID and WIFI_PASS before flashing.
 */

#pragma once

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID   "YOUR_WIFI_SSID"
#define WIFI_PASS   "YOUR_WIFI_PASSWORD"
#define WEB_PORT    80

// ── Camera (AI-Thinker ESP32-CAM pin map) ─────────────────────────────────────
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

// ── OLED (SSD1306, 1.3" = SH1106 driver variant) ─────────────────────────────
// Wiring: SDA → GPIO 14, SCL → GPIO 15
// (GPIO 2 is LED_BUILTIN — avoid it; GPIO 12/13 are used by SD card on some boards)
#define OLED_SDA     14
#define OLED_SCL     15
#define OLED_WIDTH  128
#define OLED_HEIGHT  64
// Set to true if you have the 1.3" SH1106 variant (slightly different driver):
#define OLED_IS_SH1106  true

// ── Inference ─────────────────────────────────────────────────────────────────
#define IMG_SIZE          48          // model input: 48×48 greyscale
#define FRAME_BYTES       (IMG_SIZE * IMG_SIZE)  // 2304 bytes per frame
#define NUM_CLASSES        4
#define CONFIDENCE_THRESH  0.60f     // below this → show "uncertain"

// ── Class labels (must match training order) ───────────────────────────────────
// Index 0=happy, 1=angry, 2=sad, 3=neutral
const char* const CLASS_LABELS[NUM_CLASSES] = {
    "HAPPY", "ANGRY", "SAD", "NEUTRAL"
};

// Emoji for OLED splash animation (UTF-8, drawn as bitmap sprites)
// See display_task.cpp for actual bitmap definitions
const uint8_t CLASS_EMOJI_IDX[NUM_CLASSES] = { 0, 1, 2, 3 };  // smiley, angry, sad, neutral

// ── Inference result struct (passed via queue) ─────────────────────────────────
struct InferenceResult {
    uint8_t  class_idx;
    float    confidence;
    uint32_t inference_ms;
    float    scores[NUM_CLASSES];
};

// ── FreeRTOS queues (extern declared in main sketch) ──────────────────────────
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

extern QueueHandle_t    g_frame_queue;
extern QueueHandle_t    g_result_queue;
extern SemaphoreHandle_t g_i2c_mutex;