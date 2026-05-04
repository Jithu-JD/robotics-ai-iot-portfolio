#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ===================== WiFi =====================
const char* WIFI_SSID = "Primary_network";
const char* WIFI_PASS = "Ka@629262";

// ===================== HTTP Server =====================
WebServer server(81);

// ===================== OLED (SSD1306) =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ESP32-CAM I2C pins (AI-Thinker commonly used)
static const int I2C_SDA = 15;
static const int I2C_SCL = 14;

// ===================== Camera Pins (ESP32-CAM AI-Thinker) =====================
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

// ===================== UI / State =====================
String lastLabel = "None";
float  lastScore = 0.0f;

unsigned long lastAiMillis = 0;
const unsigned long RESULT_HOLD_MS = 12000;
const unsigned long READY_REFRESH_MS = 1500;
unsigned long lastDrawMillis = 0;

// ===================== Nutrition "database" =====================
struct Nutrition {
  const char* name;
  int calories;
  float sugar_g;
  float fiber_g;
  int vitaminC_mg;
};

Nutrition lookupNutrition(const String& label) {
  String L = label;
  L.toLowerCase();

  if (L == "banana")  return {"Banana", 89, 12.2, 2.6,  9};
  if (L == "apple")   return {"Apple",  52, 10.4, 2.4,  5};
  if (L == "orange")  return {"Orange", 47,  9.4, 2.4, 53};
  if (L == "mango")   return {"Mango",  60, 13.7, 1.6, 36};
  if (L == "grapes")  return {"Grapes", 69, 15.5, 0.9,  4};

  return {"Unknown", 0, 0, 0, 0};
}

// ===================== CORS / PNA =====================
// This is the key to make fetch() from 127.0.0.1 -> 192.168.x.x work in modern Chrome.
void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader(
    "Access-Control-Allow-Headers",
    "Content-Type, Access-Control-Request-Private-Network"
  );
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
  server.sendHeader("Cache-Control", "no-store");
}

// Preflight handler
void handleOptions() {
  sendCORSHeaders();
  server.send(204); // No Content
}

// ===================== OLED Helpers =====================
void drawHeader(const char* title) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(title);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void oledDebug(const String& msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("DEBUG");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 14);
  display.println(msg);
  display.display();
}


void drawReadyScreen(const char* ipStr) {
  drawHeader("AI Fruit Scanner");
  display.setCursor(0, 14);
  display.println("Status: READY");
  display.println("Put fruit in window");
  display.println("PC is thinking...");
  display.setCursor(0, 52);
  display.print("IP: ");
  display.println(ipStr);
  display.display();
}

void drawScanningScreen() {
  drawHeader("AI Fruit Scanner");
  display.setCursor(0, 14);
  display.println("Status: SCANNING");
  display.println("Hold fruit steady");
  display.println("...");
  display.display();
}

void drawResultScreen(const String& label, float score) {
  Nutrition n = lookupNutrition(label);

  drawHeader("AI Result");
  display.setCursor(0, 14);

  display.print("Fruit: ");
  display.println(n.name);

  display.print("Confidence: ");
  display.print((int)(score * 100));
  display.println("%");

  display.println("");
  display.println("Per 100g (approx):");

  display.print("Cal: ");
  display.print(n.calories);
  display.print("  Sugar: ");
  display.print(n.sugar_g, 1);
  display.println("g");

  display.print("Fiber: ");
  display.print(n.fiber_g, 1);
  display.print("g  VitC: ");
  display.print(n.vitaminC_mg);
  display.println("mg");

  display.display();
}
void serialPrintResult(const String& label, float score) {
  Nutrition n = lookupNutrition(label);

  Serial.println("=== AI RESULT ===");
  Serial.print("Fruit: ");
  Serial.println(n.name);

  Serial.print("Confidence: ");
  Serial.print((int)(score * 100));
  Serial.println("%");

  Serial.print("Calories: ");
  Serial.print(n.calories);
  Serial.println(" kcal");

  Serial.print("Sugar: ");
  Serial.print(n.sugar_g, 1);
  Serial.println(" g");

  Serial.print("Fiber: ");
  Serial.print(n.fiber_g, 1);
  Serial.println(" g");

  Serial.print("Vitamin C: ");
  Serial.print(n.vitaminC_mg);
  Serial.println(" mg");

  Serial.println("=================");
}

void maybeRedraw() {
  unsigned long now = millis();
  bool hasRecentAI = (now - lastAiMillis) < RESULT_HOLD_MS;

  if (hasRecentAI) {
    if (now - lastDrawMillis > 250) {
      drawResultScreen(lastLabel, lastScore);
      lastDrawMillis = now;
    }
    return;
  }

  if (now - lastDrawMillis > READY_REFRESH_MS) {
    String ipStr = WiFi.localIP().toString();
    drawReadyScreen(ipStr.c_str());
    lastDrawMillis = now;
  }
}

// ===================== Camera Setup =====================
bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;

  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Classroom-stable defaults
  config.frame_size   = FRAMESIZE_QVGA; // 320x240 (more reliable than VGA)
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  return (err == ESP_OK);
}

// ===================== Routes =====================

// /capture -> JPEG
void handleCapture() {
  sendCORSHeaders();

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");

  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

// /ai?label=Banana&score=0.92
void handleAI() {
  // 1. Received request
  Serial.println("---- /ai HIT ----");

  String label = server.hasArg("label") ? server.arg("label") : "Unknown";
  String scoreStr = server.hasArg("score") ? server.arg("score") : "0";

  Serial.print("Raw label: ");
  Serial.println(label);
  Serial.print("Raw score: ");
  Serial.println(scoreStr);

  sendCORSHeaders();

  float score = scoreStr.toFloat();
  score = constrain(score, 0.0, 1.0);

  Serial.print("Parsed score: ");
  Serial.println(score, 3);

  // 2. Nutrition lookup
  Nutrition n = lookupNutrition(label);
  Serial.print("Nutrition resolved as: ");
  Serial.println(n.name);

  lastLabel = label;
  lastScore = score;
  lastAiMillis = millis();

  // 3. Show quick OLED debug BEFORE full draw
  oledDebug("AI: " + label);

  delay(400); // short pause so debug text is visible

  // 4. Draw final nutrition screen
  Serial.println("Drawing nutrition screen on OLED");
  drawResultScreen(label, score);
  serialPrintResult(label, score);
  lastDrawMillis = millis();

  server.send(
    200,
    "application/json",
    String("{\"ok\":true,\"label\":\"") + label +
    "\",\"score\":" + String(score, 3) + "}"
  );

  Serial.println("---- /ai DONE ----");
}


// /stream -> MJPEG
// NOTE: This blocks while connected (normal for simple sketches).
// For best reliability, DON'T use /stream inside your prediction page.
// Use /capture preview instead.
void handleStream() {
  WiFiClient client = server.client();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Cache-Control: no-store");
  client.println();

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) break;

    client.println("--frame");
    client.println("Content-Type: image/jpeg");
    client.print("Content-Length: ");
    client.println(fb->len);
    client.println();

    client.write(fb->buf, fb->len);
    client.println();

    esp_camera_fb_return(fb);

    delay(60);
    yield();
  }
}

// ===================== Arduino =====================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  // OLED init
  Wire.begin(I2C_SDA, I2C_SCL);
  bool oledOK = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOK) {
    drawHeader("AI Fruit Scanner");
    display.setCursor(0, 14);
    display.println("Booting...");
    display.display();
  } else {
    Serial.println("OLED init failed (check wiring/address 0x3C/0x3D)");
  }

  // Camera init
  if (!setupCamera()) {
    Serial.println("Camera init failed");
    if (oledOK) {
      drawHeader("ERROR");
      display.setCursor(0, 14);
      display.println("Camera init failed");
      display.display();
    }
    while (true) delay(1000);
  }

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  if (oledOK) drawScanningScreen();

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // OPTIONS preflight handlers (PNA/CORS)
  server.on("/",        HTTP_OPTIONS, handleOptions);
  server.on("/capture", HTTP_OPTIONS, handleOptions);
  server.on("/ai",      HTTP_OPTIONS, handleOptions);
  server.on("/stream",  HTTP_OPTIONS, handleOptions);

  // GET routes
  server.on("/", HTTP_GET, []() {
    sendCORSHeaders();
    server.send(200, "text/plain",
      "ESP32-CAM AI Fruit Scanner\n\n"
      "Stream:   /stream\n"
      "Capture:  /capture\n"
      "AI input: /ai?label=Banana&score=0.92\n"
    );
  });

  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/ai",      HTTP_GET, handleAI);
  server.on("/stream",  HTTP_GET, handleStream);

  // Fallback
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      handleOptions();
      return;
    }
    server.send(404, "text/plain", "Not found");
  });

  server.begin();

  // READY screen
  if (oledOK) {
    String ipStr = WiFi.localIP().toString();
    drawReadyScreen(ipStr.c_str());
    lastDrawMillis = millis();
  }
}

void loop() {
  server.handleClient();
  maybeRedraw();
}
