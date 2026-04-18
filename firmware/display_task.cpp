/*
 * EdgeGaze — display_task.cpp
 * Drives the 1.3" OLED (SSD1306/SH1106) with animated emoji-style faces,
 * confidence bar, and scrolling label.
 *
 * Coordinate system: 128×64 pixels, top-left origin.
 * Face sprite: 36×36 px centred at x=16..52, y=10..46
 * Right panel: x=58..127, y=0..63
 *
 * Library: Adafruit SSD1306  (or U8g2 if using SH1106)
 * Install via Arduino Library Manager:
 *   "Adafruit SSD1306" + "Adafruit GFX Library"
 */

#include "config.h"
#include "display_task.h"

#if OLED_IS_SH1106
  // Use U8g2 for SH1106 (better compatibility)
  #include <U8g2lib.h>
  #include <Wire.h>
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
  #define USE_U8G2
#else
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
#endif

// ── Emoji bitmaps (36×36, 1bpp) ──────────────────────────────────────────────
// Each is a circle with hand-drawn expression.
// Generated with: https://javl.github.io/image2cpp/

// HAPPY  😊 — big eyes, wide smile
static const uint8_t PROGMEM bmp_happy[] = {
  0x00,0x07,0xE0,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x3F,0xFC,0x00,
  0x00,0x7F,0xFE,0x00,
  0x00,0xFF,0xFF,0x00,
  0x01,0xFF,0xFF,0x80,
  0x01,0xFF,0xFF,0x80,
  0x03,0xFF,0xFF,0xC0,
  0x03,0xE7,0xE7,0xC0,
  0x03,0xE7,0xE7,0xC0,
  0x07,0xE7,0xE7,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xC0,0x03,0xE0,
  0x07,0x80,0x01,0xE0,
  0x07,0x80,0x01,0xE0,
  0x07,0xC0,0x03,0xE0,
  0x03,0xF0,0x0F,0xC0,
  0x03,0xFF,0xFF,0xC0,
  0x01,0xFF,0xFF,0x80,
  0x01,0xFF,0xFF,0x80,
  0x00,0xFF,0xFF,0x00,
  0x00,0x7F,0xFE,0x00,
  0x00,0x3F,0xFC,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x07,0xE0,0x00,
};

// ANGRY 😠 — angled brows, frown
static const uint8_t PROGMEM bmp_angry[] = {
  0x00,0x07,0xE0,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x7F,0xFE,0x00,
  0x01,0xFF,0xFF,0x80,
  0x03,0xE0,0x07,0xC0,
  0x07,0xE0,0x07,0xE0,
  0x07,0xF0,0x0F,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xE3,0xC7,0xE0,
  0x07,0xE3,0xC7,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xF0,0x0F,0xE0,
  0x07,0x80,0x01,0xE0,
  0x07,0x80,0x01,0xE0,
  0x07,0xC0,0x03,0xE0,
  0x03,0xE0,0x07,0xC0,
  0x03,0xFF,0xFF,0xC0,
  0x01,0xFF,0xFF,0x80,
  0x00,0xFF,0xFF,0x00,
  0x00,0x7F,0xFE,0x00,
  0x00,0x3F,0xFC,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x07,0xE0,0x00,
};

// SAD 😢 — droopy eyes, inverted smile, teardrop
static const uint8_t PROGMEM bmp_sad[] = {
  0x00,0x07,0xE0,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x3F,0xFC,0x00,
  0x00,0xFF,0xFF,0x00,
  0x01,0xFF,0xFF,0x80,
  0x03,0xFF,0xFF,0xC0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xE7,0xE7,0xE0,
  0x07,0xE7,0xE7,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xF8,0x1F,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xF0,0x0F,0xE0,
  0x07,0xC0,0x03,0xE0,
  0x07,0x80,0x01,0xE0,
  0x07,0x80,0x01,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x03,0xFF,0xFF,0xC0,
  0x01,0xFF,0xFF,0x80,
  0x00,0xFF,0xFF,0x00,
  0x00,0x7F,0xFE,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x07,0xE0,0x00,
};

// NEUTRAL 😐 — flat line mouth
static const uint8_t PROGMEM bmp_neutral[] = {
  0x00,0x07,0xE0,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x3F,0xFC,0x00,
  0x00,0xFF,0xFF,0x00,
  0x01,0xFF,0xFF,0x80,
  0x03,0xFF,0xFF,0xC0,
  0x07,0xE7,0xE7,0xE0,
  0x07,0xE7,0xE7,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xC0,0x03,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x07,0xFF,0xFF,0xE0,
  0x03,0xFF,0xFF,0xC0,
  0x01,0xFF,0xFF,0x80,
  0x00,0xFF,0xFF,0x00,
  0x00,0x3F,0xFC,0x00,
  0x00,0x1F,0xF8,0x00,
  0x00,0x07,0xE0,0x00,
};

// Pointers and sizes
static const uint8_t* const BITMAPS[NUM_CLASSES] = {
    bmp_happy, bmp_angry, bmp_sad, bmp_neutral
};
static const uint8_t BMP_H = 26;  // rows in each bitmap (32-bit rows × 26)
static const uint8_t BMP_W = 26;

// Accent colour fill per emotion (used for bar and label bg)
// In 1-bit OLED we toggle inversion instead
static const bool INVERT_LABEL[NUM_CLASSES] = {
    false,  // happy  → normal
    true,   // angry  → inverted (dramatic)
    false,  // sad    → normal
    false,  // neutral
};

// Label strings with unicode approximation on OLED font
static const char* DISPLAY_LABELS[NUM_CLASSES] = {
    ":) HAPPY",
    ">:( ANGRY",
    ":( SAD",
    ":| NEUTRAL"
};

// ── Helpers ──────────────────────────────────────────────────────────────────
static uint8_t  current_class   = 3;   // start neutral
static float    current_conf    = 0.0f;
static uint32_t anim_tick       = 0;
static bool     blink_state     = false;

#ifdef USE_U8G2
static void drawFrame(uint8_t cls, float conf) {
    u8g2.clearBuffer();

    // ── Left panel: emoji face (28×28 centred in 50px wide zone) ─────────────
    int fx = 11, fy = 18;
    u8g2.drawXBMP(fx, fy, BMP_W, BMP_H, BITMAPS[cls]);

    // Blink animation: overwrite eyes every ~3s for 200ms
    if (blink_state) {
        // draw two small filled rects over eye positions
        u8g2.setDrawColor(1);
        u8g2.drawBox(fx + 5, fy + 7, 5, 2);
        u8g2.drawBox(fx + 16, fy + 7, 5, 2);
    }

    // Vertical divider
    u8g2.drawVLine(53, 0, 64);

    // ── Right panel ───────────────────────────────────────────────────────────
    // Emotion label (big)
    u8g2.setFont(u8g2_font_6x10_tf);
    const char* short_labels[NUM_CLASSES] = {"HAPPY","ANGRY","SAD","NEUTRAL"};
    u8g2.drawStr(57, 14, short_labels[cls]);

    // Confidence bar outline
    u8g2.drawFrame(57, 20, 66, 9);
    // Filled portion
    int bar_w = (int)(conf * 64.0f);
    if (bar_w > 0) u8g2.drawBox(58, 21, bar_w, 7);

    // Confidence percent
    char pct[10];
    snprintf(pct, sizeof(pct), "%d%%", (int)(conf * 100));
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(57, 38, pct);

    // Bottom: inference note
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(57, 52, "EdgeGaze v1");
    u8g2.drawStr(57, 61, "TinyML INT8");

    u8g2.sendBuffer();
}
#else
static void drawFrame(uint8_t cls, float conf) {
    display.clearDisplay();
    display.setTextColor(WHITE);

    // Face bitmap
    display.drawBitmap(11, 18, BITMAPS[cls], BMP_W, BMP_H, WHITE);

    if (blink_state) {
        display.fillRect(16, 25, 5, 2, BLACK);
        display.fillRect(27, 25, 5, 2, BLACK);
    }

    // Divider
    display.drawFastVLine(53, 0, 64, WHITE);

    // Label
    display.setTextSize(1);
    const char* short_labels[NUM_CLASSES] = {"HAPPY","ANGRY","SAD","NEUTRAL"};
    display.setCursor(57, 4);
    display.print(short_labels[cls]);

    // Bar
    display.drawRect(57, 14, 66, 7, WHITE);
    int bar_w = (int)(conf * 64.0f);
    if (bar_w > 0) display.fillRect(58, 15, bar_w, 5, WHITE);

    // Percent
    char pct[10];
    snprintf(pct, sizeof(pct), "%d%%", (int)(conf * 100));
    display.setCursor(57, 26);
    display.print(pct);

    display.setCursor(57, 48);
    display.print("EdgeGaze v1");
    display.setCursor(57, 57);
    display.print("TinyML INT8");

    display.display();
}
#endif

// ── Startup splash ────────────────────────────────────────────────────────────
static void showSplash() {
#ifdef USE_U8G2
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_9x18B_tf);
    u8g2.drawStr(14, 28, "EdgeGaze");
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(22, 42, "TinyML  v1.0");
    u8g2.drawStr(18, 54, "Connecting WiFi..");
    u8g2.sendBuffer();
#else
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(4, 10);
    display.print("EdgeGaze");
    display.setTextSize(1);
    display.setCursor(20, 36);
    display.print("TinyML  v1.0");
    display.setCursor(10, 52);
    display.print("Connecting WiFi..");
    display.display();
#endif
    delay(2500);
}

void displayTask(void* pvParams) {
    Serial.println("[OLED] Task started");

    // Initialise I2C on custom pins
    Wire.begin(OLED_SDA, OLED_SCL);

#ifdef USE_U8G2
    u8g2.begin();
    u8g2.setContrast(200);
#else
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("[OLED] SSD1306 not found — check wiring");
        vTaskDelete(NULL);
        return;
    }
    display.setRotation(0);
    display.dim(false);
#endif

    showSplash();

    InferenceResult result;
    uint32_t last_blink = 0;

    for (;;) {
        // Non-blocking peek at latest result
        if (xQueuePeek(g_result_queue, &result, pdMS_TO_TICKS(100)) == pdTRUE) {
            current_class = result.class_idx;
            current_conf  = result.confidence;
        }

        // Blink every ~4s, for 180ms
        uint32_t now = millis();
        if (!blink_state && (now - last_blink > 4000)) {
            blink_state = true;
            last_blink  = now;
        }
        if (blink_state && (now - last_blink > 180)) {
            blink_state = false;
        }

        if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(30)) == pdTRUE) {
            drawFrame(current_class, current_conf);
            xSemaphoreGive(g_i2c_mutex);
        }

        anim_tick++;
        vTaskDelay(pdMS_TO_TICKS(60));  // ~16 fps display update
    }
}