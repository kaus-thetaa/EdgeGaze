# EdgeGaze 😄😠😢😐
### Real-Time Facial Emotion Detection on ESP32-CAM · TinyML + FreeRTOS + Live Web Dashboard

[![TensorFlow Lite](https://img.shields.io/badge/TFLite-INT8-orange?style=flat-square)](https://www.tensorflow.org/lite/microcontrollers)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-4%20tasks-green?style=flat-square)](https://www.freertos.org/)
[![ESP32-CAM](https://img.shields.io/badge/ESP32--CAM-AI--Thinker-blue?style=flat-square)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)

> A complete embedded AI system that detects your facial emotion in real-time, entirely **on-device** — no cloud, no API, no latency. The ESP32-CAM runs a quantized CNN at ~12 FPS, displays the result on a 1.3" OLED with animated emoji faces, and streams live data to a retro-futuristic browser dashboard over WiFi.

---

## Demo

```
ESP32-CAM captures → 48×48 grayscale → INT8 CNN → HAPPY 😄 (94.2%) → OLED + Browser
```

**OLED shows:** Animated emoji face · Confidence bar · Class label  
**Browser shows:** Live emotion · Per-class scores · Sparkline history · System stats

---

## System Architecture

```
┌─────────────────────── PC (Training) ──────────────────────────┐
│                                                                  │
│  collect.py → data/ (300 imgs × 4 classes)                      │
│      ↓                                                           │
│  train.py → emotion_model.h5  (Keras CNN, ~42K params)          │
│      ↓                                                           │
│  convert_tflite.py → emotion_model.tflite (INT8, ~75KB)         │
│      ↓                                                           │
│  xxd → firmware/model.h  (C byte array)                         │
└──────────────────────────────────────────────────────────────────┘
                              │  Flash via Arduino IDE
                              ▼
┌─────────────────── ESP32-CAM (Runtime) ────────────────────────┐
│                                                                  │
│  Core 0:  CameraTask   (grab 320×240 → resize 48×48 → queue)   │
│  Core 1:  InferenceTask (TFLite INT8 → InferenceResult queue)  │
│  Core 1:  DisplayTask   (OLED: emoji + bar + label)            │
│  Core 1:  WebTask       (WiFi + HTTP + SSE stream)             │
│                                                                  │
│  FreeRTOS primitives:                                            │
│    g_frame_queue   (QueueHandle_t, size 2)                       │
│    g_result_queue  (QueueHandle_t, size 8, xQueueOverwrite)      │
│    g_i2c_mutex     (SemaphoreHandle_t, protects I2C bus)        │
└──────────────────────────────────────────────────────────────────┘
                              │  WiFi (SSE /events)
                              ▼
┌─────────────────── Browser Dashboard ──────────────────────────┐
│  Retro dark UI · Live emotion · Confidence · Sparkline history  │
│  Per-class score bars · System stats · Auto-reconnect SSE       │
└──────────────────────────────────────────────────────────────────┘
```

---

## Folder Structure

```
EdgeGaze/
│
├── training/
│   ├── collect.py          Webcam data collector (face detection + auto-mode)
│   ├── train.py            Keras CNN training with augmentation + plots
│   ├── convert_tflite.py   INT8 quantization + C header generation
│   └── requirements.txt    Python dependencies
│
├── firmware/
│   ├── EdgeGaze.ino        Main sketch: setup() + FreeRTOS task spawning
│   ├── config.h            ← EDIT THIS: WiFi SSID/pass + pin config
│   ├── model.h             Auto-generated (run convert_tflite.py)
│   ├── camera_task.cpp/h   Core 0: OV2640 capture + resize
│   ├── inference_task.cpp/h Core 1: TFLite INT8 inference
│   ├── display_task.cpp/h  Core 1: SSD1306/SH1106 OLED animation
│   └── web_task.cpp/h      Core 1: WiFi + HTTP server + SSE
│
├── web_dashboard/
│   └── index.html          Standalone dashboard (also embedded in firmware)
│
├── assets/
│   └── (demo gif, screenshots)
│
└── README.md
```

---

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| MCU + Camera | AI-Thinker ESP32-CAM | OV2640, 4MB PSRAM |
| Display | 1.3" OLED SSD1306 / SH1106 | I2C, 128×64 |
| Power | USB-TTL adapter | For flashing; 5V/2A for runtime |

**OLED Wiring:**
```
OLED SDA  →  ESP32-CAM GPIO 14
OLED SCL  →  ESP32-CAM GPIO 15
OLED VCC  →  3.3V
OLED GND  →  GND
```

> **Note:** If you have the 1.3" variant (SH1106 driver, common on AliExpress), set `OLED_IS_SH1106 true` in `config.h`. The 0.96" is SSD1306.

---

## Setup Guide

### Step 1 — Python environment

```bash
cd training/
pip install -r requirements.txt
```

### Step 2 — Collect data

```bash
python collect.py
```

- Window opens with your webcam
- Press `A` to enter **auto-capture mode** (easiest)
- Hold each expression for ~15 seconds per class
- Works through 4 classes: **HAPPY → ANGRY → SAD → NEUTRAL**
- Press `Q` to move to the next class

### Step 3 — Train

```bash
python train.py
```

- Trains for up to 50 epochs with early stopping
- Saves `emotion_model.h5` + `training_results.png`
- Targets >88% validation accuracy

### Step 4 — Convert to TFLite

```bash
python convert_tflite.py
```

- Quantizes to INT8 (~75KB target)
- Generates `../firmware/model.h` automatically

### Step 5 — Configure firmware

Open `firmware/config.h` and set:
```cpp
#define WIFI_SSID  "your_ssid_here"
#define WIFI_PASS  "your_password_here"
```

Also set `OLED_IS_SH1106` to `true` if you have the 1.3" OLED.

### Step 6 — Arduino IDE settings

| Setting | Value |
|---------|-------|
| Board | AI-Thinker ESP32-CAM |
| Partition Scheme | Huge APP (3MB No OTA) |
| PSRAM | Enabled |
| CPU Speed | 240 MHz |
| Upload Speed | 115200 |

**Required libraries** (install via Library Manager):
- `TensorFlowLite_ESP32` by TensorFlow
- `Adafruit SSD1306` + `Adafruit GFX Library`  
  *(or `U8g2` if using SH1106 variant)*
- `esp32` board package by Espressif

### Step 7 — Flash & run

1. Connect USB-TTL adapter (GPIO 0 → GND during flash)
2. Upload from Arduino IDE
3. Disconnect GPIO 0 from GND, press reset
4. Open Serial Monitor at **115200 baud**
5. Watch for: `[WEB] Connected! IP: 192.168.x.x`
6. Open that IP in your browser 🎉

---

## Model Details

| Parameter | Value |
|-----------|-------|
| Architecture | Custom CNN (3 conv blocks) |
| Input | 48×48 grayscale |
| Classes | happy, angry, sad, neutral |
| Parameters | ~42,000 |
| Format | TFLite INT8 |
| Size | ~75 KB |
| Inference time | ~65–90 ms on ESP32 |
| Tensor arena | 96 KB (PSRAM) |
| Val accuracy | ~89–92% (dataset-dependent) |

### CNN Architecture
```
Input (48, 48, 1)
→ Conv2D(16, 3×3) + BN + MaxPool + Dropout(0.2)
→ Conv2D(32, 3×3) + BN + MaxPool + Dropout(0.2)
→ Conv2D(64, 3×3) + BN + MaxPool + Dropout(0.3)
→ Flatten
→ Dense(128) + Dropout(0.4)
→ Dense(4, softmax)
```

---

## FreeRTOS Design

```
Core 0                          Core 1
──────                          ──────
CameraTask (P4)                 InferenceTask (P3)
  grab QVGA frame                 receive frame from queue
  resize → 48×48 gray             run TFLite INT8 invoke
  push to g_frame_queue           xQueueOverwrite result
        │                                 │
        │   g_frame_queue                 │  g_result_queue
        └────────────────────────►────────┘
                                          ├──► DisplayTask (P1)
                                          │      xSemaphoreTake(i2c)
                                          │      draw OLED frame
                                          │      xSemaphoreGive(i2c)
                                          │
                                          └──► WebTask (P2)
                                                 xQueuePeek result
                                                 send SSE event
```

**Design decisions:**
- `xQueueOverwrite` on result queue — display/web always get the freshest result, never stale
- I2C mutex prevents display and any future sensor from fighting the bus
- Camera pinned to Core 0 — interrupt latency isolated from inference
- SSE over WebSocket — no library needed, browser reconnects automatically

---

## Browser Dashboard

Navigate to `http://<ESP32-IP>/` after boot.

| Endpoint | Description |
|----------|-------------|
| `GET /` | Dashboard HTML |
| `GET /events` | SSE stream (live results, ~20Hz) |
| `GET /latest` | JSON snapshot |
| `GET /status` | System info (heap, uptime) |

---

## Stretch Goals

- [ ] **BLE HID** — gestures control PC directly as Bluetooth keyboard
- [ ] **SPIFFS** — serve dashboard HTML from flash, update without reflash
- [ ] **OTA updates** — pull new model over WiFi
- [ ] **Confidence threshold** — show "uncertain" below 60% (already in config)
- [ ] **Add disgust + surprise** — expand to 6 FER classes using FER2013 dataset
- [ ] **Live camera feed** — MJPEG stream alongside inference overlay

---

## License

MIT — free to use, modify, and build upon. Attribution appreciated.

---

*Built by [your name] · ESP32-CAM + TensorFlow Lite Micro + FreeRTOS*