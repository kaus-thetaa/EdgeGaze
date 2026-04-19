/*
  camera_utils.h  — EdgeWatch
  ----------------------------
  Camera init for AI Thinker ESP32-CAM (OV2640).
  Provides initCamera() and resizeToGrayscale48().
*/

#pragma once
#include "esp_camera.h"

// AI Thinker pin map
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

bool initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = CAM_PIN_D0;
  cfg.pin_d1       = CAM_PIN_D1;
  cfg.pin_d2       = CAM_PIN_D2;
  cfg.pin_d3       = CAM_PIN_D3;
  cfg.pin_d4       = CAM_PIN_D4;
  cfg.pin_d5       = CAM_PIN_D5;
  cfg.pin_d6       = CAM_PIN_D6;
  cfg.pin_d7       = CAM_PIN_D7;
  cfg.pin_xclk     = CAM_PIN_XCLK;
  cfg.pin_pclk     = CAM_PIN_PCLK;
  cfg.pin_vsync    = CAM_PIN_VSYNC;
  cfg.pin_href     = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn     = CAM_PIN_PWDN;
  cfg.pin_reset    = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_GRAYSCALE;
  cfg.frame_size   = FRAMESIZE_QVGA;   // 320×240
  cfg.jpeg_quality = 12;
  cfg.fb_count     = 2;
  cfg.fb_location  = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode    = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("[Camera] Init failed");
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_brightness(s, 1);
  s->set_contrast(s, 1);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 1);

  return true;
}

bool resizeToGrayscale48(const uint8_t* src, size_t src_len,
                          uint16_t src_w, uint16_t src_h,
                          uint8_t* dst) {
  const uint16_t DST = 48;
  if (!src || !dst || src_len < (size_t)(src_w * src_h)) return false;

  float sx = (float)src_w / DST;
  float sy = (float)src_h / DST;

  for (uint16_t dy = 0; dy < DST; dy++) {
    for (uint16_t dx = 0; dx < DST; dx++) {
      uint16_t px = (uint16_t)(dx * sx);
      uint16_t py = (uint16_t)(dy * sy);
      if (px >= src_w) px = src_w - 1;
      if (py >= src_h) py = src_h - 1;
      dst[dy * DST + dx] = src[py * src_w + px];
    }
  }
  return true;
}
