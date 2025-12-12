// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Original Repository: https://github.com/okalachev/flix
// Modified to include ESP32-S3 Camera functionality with optimized streaming

#include "vector.h"
#include "quaternion.h"
#include "util.h"
#include "esp_camera.h"

// ============================
// Camera Configuration
// ============================
#define CAMERA_ENABLED 1
#define CAMERA_STREAMING_ENABLED 1

// Camera pin configuration for ESP32-S3
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5

#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

// ============================
// Existing Flix Variables
// ============================
#define SERIAL_BAUDRATE 115200
#define WIFI_ENABLED 1

double t = NAN;
float dt;
float controlRoll, controlPitch, controlYaw, controlThrottle;
float controlMode = NAN;
Vector gyro;
Vector acc;
Vector rates;
Quaternion attitude;
bool landed;
float motors[4];

// Camera variables
bool cameraInitialized = false;
TaskHandle_t streamServerTaskHandle = NULL;

// ============================
// Camera Initialization
// ============================
#if CAMERA_ENABLED
bool setupCamera() {
  camera_config_t config;
  
  // Camera configuration for OV5640 - optimized for low resource usage
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; // Lower frequency for stability
  config.frame_size = FRAMESIZE_QVGA; // 320x2410 - минимальное разрешение
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 30; // Low quality for performance
  config.fb_count = 1; // Only one buffer to save memory

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  // Optimized sensor settings for OV5640
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    // Basic image settings
    s->set_brightness(s, 0);     // Maximum brightness
    s->set_contrast(s, 0);       // Maximum contrast
    s->set_saturation(s, 1);    // Minimum saturation to reduce green tint
    s->set_special_effect(s, 0); // No effect
    
    // Color and white balance
    s->set_whitebal(s, 1);       // Auto white balance
    s->set_awb_gain(s, 1);       // Auto WB gain
    s->set_wb_mode(s, 0);        // Auto mode

    // Exposure and gain
    s->set_exposure_ctrl(s, 1);  // Auto exposure
    s->set_aec2(s, 0);           // No night mode
    s->set_ae_level(s, 1);       // Medium exposure level
    s->set_gain_ctrl(s, 1);      // Auto gain
    s->set_agc_gain(s, 1);       // Medium gain
    
    // Image correction
    s->set_bpc(s, 1);            // Bad pixel correction
    s->set_wpc(s, 1);            // White pixel correction
    s->set_raw_gma(s, 1);        // Gamma correction
    s->set_lenc(s, 1);           // Lens correction
    s->set_hmirror(s, 0);        // No horizontal mirror
    s->set_vflip(s, 0);          // Vertical flip

    s->set_colorbar(s,0);        //No color lines
  }

  Serial.println("OV5640 camera initialized with optimized low-res settings");
  return true;
}
#endif

// ============================
// Modified Setup Function
// ============================
void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  print("Initializing flix with optimized camera streaming\n");
  disableBrownOut();
  setupParameters();
  setupMotors();
  
  // Initialize camera
#if CAMERA_ENABLED
  cameraInitialized = setupCamera();
#endif

#if WIFI_ENABLED
  setupWiFi();
#endif

  setupIMU();
  print("Initialization complete\n");

  TaskHandle_t tsk = xTaskGetHandle("loopTask");
  vTaskPrioritySet(tsk, 3);
  tsk = xTaskGetHandle("cam_task");
  vTaskPrioritySet(tsk, 2);

}

// ============================
// Modified Loop Function
// ============================
void loop() {
  // Main flight control loop (high priority)
  readIMU();
  step();
  readRC();
  estimate();
  control();
  sendMotors();
  handleInput();

#if WIFI_ENABLED
  processMavlink();
#endif

  logData();
  syncParameters();
}