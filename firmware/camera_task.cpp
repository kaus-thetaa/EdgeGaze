/*
 * EdgeGaze — camera_task.cpp
 * Initialises the OV2640 camera, grabs QVGA frames, crops to centre square,
 * downsizes to 48×48 grayscale, and pushes raw bytes to g_frame_queue.
 * Runs on Core 0, priority 4.
 */

#include "config.h"
#include "camera_task.h"
#include "esp_camera.h"
#include "img_convertor.h"  // ESP32 camera library utility

static bool initCamera() {
    camera_config_t cfg = {};
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
    cfg.pin_sscb_sda = CAM_PIN_SIOD;
    cfg.pin_sscb_scl = CAM_PIN_SIOC;
    cfg.pin_pwdn     = CAM_PIN_PWDN;
    cfg.pin_reset    = CAM_PIN_RESET;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_GRAYSCALE;  // 1 byte/pixel → simpler resize
    cfg.frame_size   = FRAMESIZE_QVGA;       // 320×240
    cfg.fb_count     = 2;
    cfg.grab_mode    = CAMERA_GRAB_LATEST;

    if (psramFound()) {
        cfg.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        cfg.fb_location = CAMERA_FB_IN_DRAM;
        cfg.fb_count    = 1;
    }

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("[CAM] Init failed: 0x%x\n", err);
        return false;
    }

    // Sensor tuning for indoor face capture
    sensor_t* s = esp_camera_sensor_get();
    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, -1);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);    // auto WB
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 2);
    s->set_gainceiling(s, (gainceiling_t)6);

    Serial.println("[CAM] Initialised OK (QVGA grayscale)");
    return true;
}

/*
 * Nearest-neighbour downsample from src (srcW×srcH, 1bpp) to dst (dstW×dstH).
 * Crops a centre square first so faces aren't squished.
 */
static void downsampleGray(const uint8_t* src, int srcW, int srcH,
                            uint8_t* dst, int dstW, int dstH) {
    // Centre-square crop
    int cropSz  = (srcW < srcH) ? srcW : srcH;
    int cropX   = (srcW - cropSz) / 2;
    int cropY   = (srcH - cropSz) / 2;

    for (int dy = 0; dy < dstH; dy++) {
        for (int dx = 0; dx < dstW; dx++) {
            int sx = cropX + (dx * cropSz) / dstW;
            int sy = cropY + (dy * cropSz) / dstH;
            dst[dy * dstW + dx] = src[sy * srcW + sx];
        }
    }
}

void cameraTask(void* pvParams) {
    Serial.println("[CAM] Task started on Core 0");

    if (!initCamera()) {
        Serial.println("[CAM] Fatal: cannot init camera. Task halting.");
        vTaskDelete(NULL);
        return;
    }

    static uint8_t frame_buf[FRAME_BYTES];
    uint32_t frame_count = 0;
    uint32_t drop_count  = 0;

    for (;;) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[CAM] Frame grab failed");
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // Resize 320×240 → 48×48
        downsampleGray(fb->buf, fb->width, fb->height,
                       frame_buf, IMG_SIZE, IMG_SIZE);
        esp_camera_fb_return(fb);

        // Push to inference queue (non-blocking — drop if full)
        if (xQueueSend(g_frame_queue, frame_buf, 0) != pdTRUE) {
            drop_count++;
        }
        frame_count++;

        // Log every 300 frames (~10s at 30fps)
        if (frame_count % 300 == 0) {
            Serial.printf("[CAM] Frames: %u  Dropped: %u\n", frame_count, drop_count);
        }

        vTaskDelay(pdMS_TO_TICKS(33));  // ~30fps cap
    }
}