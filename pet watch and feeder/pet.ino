
#define BLYNK_TEMPLATE_ID "TMPL3GE8_7Ck0"
#define BLYNK_TEMPLATE_NAME "pet watch"
#define BLYNK_AUTH_TOKEN "3ovOSP7LyyS7xHIjUekBpHoqdtIYrtuH"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "esp_camera.h"
#include <ESP32Servo.h>
#include "esp_sleep.h"

char ssid[] = "Jithu";
char pass[] = "kichu@1234";

Servo feederServo;
#define SERVO_PIN 12

// ===== Camera Pins AI Thinker =====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

BlynkTimer timer;

bool autoFeedEnabled = false;
unsigned long lastFeed = 0;
unsigned long feedInterval = 6UL * 60UL * 60UL * 1000UL; // 6 hours

uint32_t lastBrightness = 0;

// ===== Feed Function =====
void feedNow() {
  feederServo.write(90);
  delay(1200);
  feederServo.write(0);
  Blynk.virtualWrite(V3, "Fed!");
  lastFeed = millis();
}

// ===== Blynk Controls =====
BLYNK_WRITE(V0) { if(param.asInt()) feedNow(); }

BLYNK_WRITE(V1) {
  if(param.asInt()) {
    Blynk.logEvent("snapshot", "Snapshot requested");
    Blynk.virtualWrite(V3, "Snapshot triggered");
  }
}

BLYNK_WRITE(V2) {
  autoFeedEnabled = param.asInt();
}

// ===== Camera Init =====
void initCam() {
  camera_config_t config;
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_camera_init(&config);
}

// ===== Motion / Pet Detection =====
void detectPet() {
  camera_fb_t *fb = esp_camera_fb_get();
  if(!fb) return;

  uint32_t sum = 0;
  for(int i=0;i<fb->len;i+=50) sum += fb->buf[i];

  uint32_t bright = sum / (fb->len/50 + 1);

  if(abs((int)bright - (int)lastBrightness) > 15) {
    Blynk.virtualWrite(V3, "Pet detected!");
    if(autoFeedEnabled) feedNow();
  }

  lastBrightness = bright;
  esp_camera_fb_return(fb);
}

// ===== Auto Feed Timer =====
void autoFeedCheck() {
  if(autoFeedEnabled && millis() - lastFeed > feedInterval) {
    feedNow();
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  feederServo.attach(SERVO_PIN);
  feederServo.write(0);

  initCam();

  WiFi.begin(ssid, pass);
  while(WiFi.status()!=WL_CONNECTED) delay(300);

  Serial.println(WiFi.localIP());

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(10000, detectPet);     // motion check
  timer.setInterval(60000, autoFeedCheck); // timer check
}

// ===== Loop =====
void loop() {
  Blynk.run();
  timer.run();

  // light sleep between cycles
  esp_sleep_enable_timer_wakeup(2 * 1000000);
  esp_light_sleep_start();
}
