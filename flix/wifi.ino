// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Original Repository: https://github.com/okalachev/flix

// Wi-Fi support with optimized camera streaming

#if WIFI_ENABLED

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_SSID "CatterFlix_Blue"
#define WIFI_PASSWORD "flixwifi"
#define WIFI_UDP_PORT 14550
#define WIFI_UDP_REMOTE_PORT 14550
#define WIFI_UDP_REMOTE_ADDR "255.255.255.255"

WiFiUDP udp;

// HTTP server handle
httpd_handle_t stream_httpd = NULL;

// Stream control variables
volatile bool isStreaming = false;
volatile uint32_t clientCount = 0;

// Forward declarations
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);
void streamServerTask(void* parameter);
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  struct timeval _timestamp;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[128];

  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

#if CONFIG_LED_ILLUMINATOR_ENABLED
  isStreaming = true;
  enable_led(true);
#endif

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      log_e("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _timestamp.tv_sec = fb->timestamp.tv_sec;
      _timestamp.tv_usec = fb->timestamp.tv_usec;
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          log_e("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      log_e("Send frame failed");
      break;
    }
    int64_t fr_end = esp_timer_get_time();

    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;

    frame_time /= 1000;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
#endif
    log_i(
      "MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)", (uint32_t)(_jpg_buf_len), (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time, avg_frame_time,
      1000.0 / avg_frame_time
    );
  }

#if CONFIG_LED_ILLUMINATOR_ENABLED
  isStreaming = false;
  enable_led(false);
#endif

  return res;
}


// HTTP handler for HTML page
static const char* html_page = 
"<html>"
"<head>"
"<title>Flix Camera Stream</title>"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<style>"
"body { margin: 0; background: #000; }"
"#stream-container { width: 100%; max-width: 320px; margin: 0 auto; }"
"img { width: 100%; image-rendering: pixelated; }"
".info { color: white; text-align: center; font-family: Arial; padding: 5px; font-size: 12px; }"
"</style>"
"</head>"
"<body>"
"<div id='stream-container'>"
"<div class='info'>Flix Camera Stream - 160x120 @ 10FPS</div>"
"<img src='/stream' />"
"<div class='info'>Optimized for flight performance</div>"
"</div>"
"</body>"
"</html>";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html_page, strlen(html_page));
}

// Start streaming server
void startStreamServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 80;
  config.max_open_sockets = 2; // Only allow 2 clients
  config.stack_size = 4096;    // Small stack for low priority
  config.task_priority = 1;    // Low priority task
  
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = index_handler,
      .user_ctx  = NULL
    };
    
    httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
    };
    
    httpd_register_uri_handler(stream_httpd, &index_uri);
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    
    Serial.println("HTTP streaming server started on port 80 (low priority)");
  } else {
    Serial.println("Error starting HTTP streaming server");
  }
}

// Separate task for stream server
void streamServerTask(void* parameter) {
  startStreamServer();
  
  // Keep task alive
  while (true) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setupWiFi() {
  print("Setup Wi-Fi\n");
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  udp.begin(WIFI_UDP_PORT);
  
  // Start streaming server in separate task with low priority
#if CAMERA_ENABLED && CAMERA_STREAMING_ENABLED
  xTaskCreatePinnedToCore(
    streamServerTask,
    "Stream Server",
    4096,  // Small stack
    NULL,
    1,     // Low priority (1), flight control has higher priority
    NULL,
    0      // Core 0
  );
#endif
  
  Serial.print("Camera Stream URL: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/");
  Serial.println("Stream optimized: 160x120 @ 10FPS, low priority");
}

void sendWiFi(const uint8_t *buf, int len) {
  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && WiFi.status() != WL_CONNECTED) return;
  udp.beginPacket(udp.remoteIP() ? udp.remoteIP() : WIFI_UDP_REMOTE_ADDR, WIFI_UDP_REMOTE_PORT);
  udp.write(buf, len);
  udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
  udp.parsePacket();
  return udp.read(buf, len);
}

#endif