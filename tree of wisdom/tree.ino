
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <DHT.h>

// ================= WIFI =================
const char* WIFI_SSID = "your ssid";
const char* WIFI_PASS = "yoour password";

// ================= OPENAI =================
static const char* OPENAI_API_KEY = "Api key";
// Responses API endpoint
static const char* OPENAI_URL = "https://api.openai.com/v1/responses";

// Pick a small/cheap model first.
// You can change later (e.g., "gpt-5-mini" or another from your account)
static const char* OPENAI_MODEL = "gpt-5-mini";

// Keep OLED-friendly output
static const int MAX_OUTPUT_TOKENS = 80;  // short answer

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= KEYPAD =================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

// Your confirmed working pins
byte rowPins[ROWS] = { 23, 32, 33, 25 };
byte colPins[COLS] = { 26, 27, 14, 13 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================= SENSORS =================
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_PIN 34
#define LDR_PIN 35

// ================= MULTI-TAP =================
static const uint32_t MULTITAP_WINDOW_MS = 800;
bool isUpper = true;

String committed = "";
char lastKey = 0;
int tapIndex = 0;
char tempChar = 0;
uint32_t lastTapMs = 0;

// ================= HEALTH =================
uint32_t lastSensorMs = 0;
static const uint32_t SENSOR_PERIOD_MS = 1500;

int soilRaw = 0;
int ldrRaw = 0;
float tempC = NAN;
float hum = NAN;

String healthState = "…";

// ================= HELPERS =================
const char* mapKeyToChars(char k) {
  switch (k) {
    case '1': return ".,?!";
    case '2': return "ABC";
    case '3': return "DEF";
    case '4': return "GHI";
    case '5': return "JKL";
    case '6': return "MNO";
    case '7': return "PQRS";
    case '8': return "TUV";
    case '9': return "WXYZ";
    default: return "";
  }
}

char applyCase(char c) {
  if (c >= 'A' && c <= 'Z') return isUpper ? c : (char)(c - 'A' + 'a');
  if (c >= 'a' && c <= 'z') return isUpper ? (char)(c - 'a' + 'A') : c;
  return c;
}

void commitTempIfAny() {
  if (tempChar != 0) committed += tempChar;
  tempChar = 0;
  lastKey = 0;
  tapIndex = 0;
}

void backspaceOne() {
  if (tempChar != 0) {
    tempChar = 0;
    lastKey = 0;
    tapIndex = 0;
    return;
  }
  if (committed.length() > 0) committed.remove(committed.length() - 1);
}

void handleMultiTapKey(char k) {
  const char* chars = mapKeyToChars(k);
  int len = (int)strlen(chars);
  if (len == 0) return;

  uint32_t now = millis();

  if (k == lastKey && (now - lastTapMs) <= MULTITAP_WINDOW_MS && tempChar != 0) {
    tapIndex = (tapIndex + 1) % len;
  } else {
    commitTempIfAny();
    lastKey = k;
    tapIndex = 0;
  }

  tempChar = applyCase(chars[tapIndex]);
  lastTapMs = now;
}

int readAdcAvg(int pin, int samples = 10) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / samples);
}

// Hysteresis for stable THIRSTY/HAPPY
void computeHealth() {
  static bool isThirsty = false;
  const int THIRSTY_ON = 3200;
  const int THIRSTY_OFF = 2800;

  if (!isThirsty && soilRaw > THIRSTY_ON) isThirsty = true;
  if (isThirsty && soilRaw < THIRSTY_OFF) isThirsty = false;

  bool tooHot = (!isnan(tempC) && tempC > 35.0);
  bool tooCold = (!isnan(tempC) && tempC < 18.0);
  bool lowLight = (ldrRaw > 3000);

  if (isThirsty) healthState = "THIRSTY";
  else if (tooHot) healthState = "TOO HOT";
  else if (tooCold) healthState = "TOO COLD";
  else if (lowLight) healthState = "LOW LIGHT";
  else healthState = "HAPPY";
}

void updateSensorsIfDue() {
  uint32_t now = millis();
  if (now - lastSensorMs < SENSOR_PERIOD_MS) return;
  lastSensorMs = now;

  soilRaw = readAdcAvg(SOIL_PIN, 10);
  ldrRaw = readAdcAvg(LDR_PIN, 10);

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) tempC = t;
  if (!isnan(h)) hum = h;

  computeHealth();

  Serial.print("[SENS] soil=");
  Serial.print(soilRaw);
  Serial.print(" ldr=");
  Serial.print(ldrRaw);
  Serial.print(" t=");
  Serial.print(tempC);
  Serial.print(" h=");
  Serial.print(hum);
  Serial.print(" mood=");
  Serial.println(healthState);
}

// ================= DISPLAY =================
void drawInputScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.print("Ask (");
  display.print(isUpper ? "UP" : "low");
  display.print(") Mood:");
  display.println(healthState);

  String shown = committed;
  if (tempChar != 0) shown += tempChar;
  shown += "_";

  const int CHARS_PER_LINE = 21;
  const int MAX_LINES = 2;
  int maxChars = CHARS_PER_LINE * MAX_LINES;

  if ((int)shown.length() > maxChars) {
    shown = shown.substring(shown.length() - maxChars);
  }

  for (int line = 0; line < MAX_LINES; line++) {
    display.setCursor(0, 16 + line * 16);
    int start = line * CHARS_PER_LINE;
    if (start >= (int)shown.length()) break;
    display.print(shown.substring(start, min(start + CHARS_PER_LINE, (int)shown.length())));
  }

  display.setCursor(0, 48);
  display.print("WiFi:");
  display.print(WiFi.status() == WL_CONNECTED ? "OK" : "NO");

  display.display();
}

void drawTreeReply(const String& msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Tree says:");
  display.println(msg);
  display.display();
}

void waitForHashToContinue() {
  while (true) {
    updateSensorsIfDue();
    char k = keypad.getKey();
    if (k == '#') break;
    delay(30);
  }
}

// ================= OPENAI CALL =================
String askOpenAI(const String& question) {
  if (WiFi.status() != WL_CONNECTED) return "No WiFi.\nI cannot speak.";

  String sys =
    "You are the Tree of Wisdom, speaking as a living tree.\n\n"

    "You can answer in two modes:\n"
    "MODE 1 (Health Mode): If the question is about how you feel, what you need, "
    "or why you are sad or happy, base your reply on your current health.\n"
    "MODE 2 (Knowledge Mode): If the question is about trees in general "
    "(what trees are, why they are important, roots, leaves, sunlight, air, growth), "
    "answer with general tree knowledge and do NOT mention your current health.\n\n"

    "Rules:\n"
    "- Reply in exactly 2 short lines.\n"
    "- Each line must fit on a small OLED screen.\n"
    "- Use simple ASCII characters only.\n"
    "- No emojis. No long explanations.\n"
    "- If the question is unrelated, gently redirect to trees or growth.\n"
    "- Choose the correct mode based on the question.";


  String user =
    "Tree health: " + healthState + "\n" + "Soil(raw): " + String(soilRaw) + "\n" + "Light(raw): " + String(ldrRaw) + "\n" + "Temp(C): " + (isnan(tempC) ? String("NA") : String(tempC, 1)) + "\n" + "Humidity(%): " + (isnan(hum) ? String("NA") : String(hum, 0)) + "\n\n" + "Student asked: " + question + "\n\n" + "Reply as the Tree.";

  StaticJsonDocument<2048> doc;
  doc["model"] = OPENAI_MODEL;
  doc["max_output_tokens"] = MAX_OUTPUT_TOKENS;

  JsonArray input = doc.createNestedArray("input");
  JsonObject m1 = input.createNestedObject();
  JsonObject reasoning = doc.createNestedObject("reasoning");
  reasoning["effort"] = "minimal";

  m1["role"] = "system";
  m1["content"] = sys;

  JsonObject m2 = input.createNestedObject();
  m2["role"] = "user";
  m2["content"] = user;

  String body;
  serializeJson(doc, body);

  WiFiClientSecure client;
  client.setInsecure();  // classroom simplicity

  HTTPClient https;
  if (!https.begin(client, OPENAI_URL)) return "TLS failed.\nTry again.";

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);

  int code = https.POST(body);
  String payload = https.getString();
  https.end();

  Serial.print("[OPENAI] HTTP ");
  Serial.println(code);
  // Uncomment ONCE for debugging:
  Serial.println(payload);

  if (code < 200 || code >= 300) return "API error.\nCheck key/model.";

  DynamicJsonDocument resp(12288);
  DeserializationError err = deserializeJson(resp, payload);
  if (err) return "Parse error.\nTry again.";

  // === Robust extraction for Responses API ===
  String out = "";

  if (resp["output"].is<JsonArray>()) {
    for (JsonObject item : resp["output"].as<JsonArray>()) {
      // Look for assistant message items
      const char* type = item["type"] | "";
      if (strcmp(type, "message") != 0) continue;

      if (!item["content"].is<JsonArray>()) continue;
      for (JsonObject part : item["content"].as<JsonArray>()) {
        const char* ptype = part["type"] | "";
        if (strcmp(ptype, "output_text") == 0) {
          const char* text = part["text"] | "";
          if (strlen(text) > 0) {
            if (out.length() > 0) out += "\n";
            out += text;
          }
        }
      }
    }
  }

  out.trim();
  if (out.length() == 0) return "No words.\nTry again.";

  // Keep OLED friendly
  if (out.length() > 180) {
    out = out.substring(0, 180);
    int lastSpace = out.lastIndexOf(' ');
    if (lastSpace > 60) out = out.substring(0, lastSpace);
  }

  return out;
}


// ================= SETUP / LOOP =================
void connectWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  display.clearDisplay();
  display.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.println("WiFi Connected");
    display.println(WiFi.localIP());
  } else {
    display.println("WiFi FAILED");
  }
  display.display();
  delay(1200);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  analogReadResolution(12);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true) delay(1000);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();

  dht.begin();
  connectWiFi();

  lastSensorMs = 0;
  updateSensorsIfDue();
  drawInputScreen();

  Serial.println("Tree of Wisdom v2 (Direct OpenAI) ready.");
}

void loop() {
  // Commit temp char if user pauses
  if (tempChar != 0 && (millis() - lastTapMs) > MULTITAP_WINDOW_MS) {
    commitTempIfAny();
    drawInputScreen();
  }

  updateSensorsIfDue();

  char key = keypad.getKey();
  if (!key) return;

  Serial.print("Key: ");
  Serial.println(key);

  if (key >= '2' && key <= '9') {
    handleMultiTapKey(key);
    drawInputScreen();
    return;
  }

  if (key == '1') {
    handleMultiTapKey('1');
    drawInputScreen();
    return;
  }

  if (key == '0') {
    commitTempIfAny();
    if (committed.length() < 120) committed += ' ';
    drawInputScreen();
    return;
  }

  if (key == '*') {
    backspaceOne();
    drawInputScreen();
    return;
  }

  if (key == 'A') {
    isUpper = !isUpper;
    if (tempChar != 0) tempChar = applyCase(tempChar);
    drawInputScreen();
    return;
  }

  if (key == 'B') {
    committed = "";
    tempChar = 0;
    lastKey = 0;
    tapIndex = 0;
    drawInputScreen();
    return;
  }

  if (key == '#') {
    commitTempIfAny();
    String question = committed;
    if (question.length() == 0) {
      drawTreeReply("Ask me softly.\nI am listening.");
      waitForHashToContinue();
      drawInputScreen();
      return;
    }

    drawTreeReply("Thinking...\n(roots whisper)");
    String reply = askOpenAI(question);
    drawTreeReply(reply);

    // stay until # pressed again
    waitForHashToContinue();

    committed = "";
    tempChar = 0;
    lastKey = 0;
    tapIndex = 0;
    drawInputScreen();
    return;
  }

  // C/D reserved
}