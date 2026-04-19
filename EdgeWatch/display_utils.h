/*
  display_utils.h  — EdgeWatch
  -----------------------------
  OLED display (SSD1306, 128×64, I2C) helpers.
  Wiring: SDA → GPIO14,  SCL → GPIO15

  Shows:
    - Emotion label (large)
    - Emoji icon
    - Confidence bar (0–100%)
    - Confidence % text
*/

#pragma once

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_W      128
#define OLED_H       64
#define OLED_RESET   -1
#define OLED_ADDR   0x3C
#define SDA_PIN      14
#define SCL_PIN      15

static Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET);

// Must match inference_engine.h order: happy=0 neutral=1 sad=2 stressed=3
static const char* DISP_LABELS[] = { "Happy",    "Neutral",  "Sad",    "Stressed" };
static const char* DISP_ICONS[]  = { ":)",        ":|",       ":C",     ":("       };

// ── Init ──────────────────────────────────────────────────────────────────

bool displayInit() {
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Not found at 0x3C — check wiring and address");
    return false;
  }

  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
  Serial.println("[OLED] OK");
  return true;
}

// ── Splash ────────────────────────────────────────────────────────────────

void displaySplash() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(4, 4);
  display.print("EdgeWatch");

  display.setTextSize(1);
  display.setCursor(16, 28);
  display.print("TinyML on ESP32");

  display.setCursor(22, 42);
  display.print("64.7% val acc");

  display.setCursor(10, 54);
  display.print("Loading model...");

  display.display();
  delay(2000);
}

// ── Main result ───────────────────────────────────────────────────────────
/*
  label      : 0=Happy  1=Neutral  2=Sad  3=Stressed
  confidence : 0–255
*/
void displayResult(uint8_t label, uint8_t confidence) {
  if (label >= 4) label = 1;
  uint8_t pct = (uint16_t)confidence * 100 / 255;

  display.clearDisplay();

  // ── Row 1: label + icon ──
  display.setTextSize(2);
  display.setCursor(0, 2);
  display.print(DISP_LABELS[label]);

  // Icon — right-aligned
  display.setTextSize(2);
  int16_t  ix, iy;
  uint16_t iw, ih;
  display.getTextBounds(DISP_ICONS[label], 0, 0, &ix, &iy, &iw, &ih);
  display.setCursor(OLED_W - iw - 2, 2);
  display.print(DISP_ICONS[label]);

  // ── Divider ──
  display.drawFastHLine(0, 22, OLED_W, SSD1306_WHITE);

  // ── Row 2: confidence bar ──
  // Outline rect
  display.drawRect(0, 26, OLED_W, 12, SSD1306_WHITE);
  // Fill
  uint8_t fill = (uint16_t)pct * (OLED_W - 4) / 100;
  if (fill > 0) {
    display.fillRect(2, 28, fill, 8, SSD1306_WHITE);
  }

  // ── Row 3: pct + label index ──
  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print("Conf: ");
  display.print(pct);
  display.print("%");

  // Small class index indicator (useful for debugging)
  display.setCursor(80, 42);
  display.print("cls:");
  display.print(label);

  // ── Row 4: footer ──
  display.setCursor(0, 54);
  display.print("EdgeWatch v1.0");

  display.display();
}

// ── Error screen ──────────────────────────────────────────────────────────

void displayError(const char* line1, const char* line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("!! ERROR !!");
  display.setCursor(0, 14);
  display.print(line1);
  if (line2[0]) {
    display.setCursor(0, 28);
    display.print(line2);
  }
  display.display();
}
