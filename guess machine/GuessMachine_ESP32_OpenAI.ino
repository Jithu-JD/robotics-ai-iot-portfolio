#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "secrets.h"

// ===================== LCD =====================
LiquidCrystal_I2C lcd(0x27, 16, 2);  // try 0x3F if needed

// ===================== PINS =====================
static const int PIN_YES = 18;   // button to GND
static const int PIN_NO = 19;    // button to GND
static const int PIN_RESET = 4;  // button to GND

static const int LED_G = 25;
static const int LED_Y = 26;
static const int LED_R = 27;

static const int BUZZ = 23;

// ESP32 default I2C pins
static const int SDA_PIN = 21;
static const int SCL_PIN = 22;

// ===================== INPUT =====================
static const unsigned long DEBOUNCE_MS = 170;
static unsigned long lastPressMs = 0;

enum Ans { A_NO = 0,
           A_YES = 1,
           A_RESET = 2 };

// ===================== HTTP debug =====================
int gLastHttpCode = 0;
String gLastHttpError = "";

// ===================== UI =====================
void lcd2(const String& a, const String& b) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(a.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(b.substring(0, 16));
}

static inline void beep(int freq = 2500, int ms = 60) {
  tone(BUZZ, freq, ms);
  delay(ms);
  noTone(BUZZ);
}

static inline void beepWrong() {
  tone(BUZZ, 600, 160);
  delay(180);
  tone(BUZZ, 450, 220);
  delay(240);
  noTone(BUZZ);
}

Ans readAns() {
  while (true) {
    unsigned long now = millis();
    if (now - lastPressMs < DEBOUNCE_MS) {
      delay(5);
      continue;
    }

    if (digitalRead(PIN_RESET) == LOW) {
      lastPressMs = now;
      beep(2600, 40);
      return A_RESET;
    }
    if (digitalRead(PIN_YES) == LOW) {
      lastPressMs = now;
      beep(2800, 40);
      return A_YES;
    }
    if (digitalRead(PIN_NO) == LOW) {
      lastPressMs = now;
      beep(2000, 40);
      return A_NO;
    }

    delay(10);
  }
}
void scrollQuestion(const String& question, const String& line2) {
  if (question.length() <= 16) {
    lcd2(question, line2);
    return;
  }

  for (int i = 0; i <= question.length() - 16; i++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(question.substring(i, i + 16));
    lcd.setCursor(0, 1);
    lcd.print(line2);
    delay(350);

    // stop scrolling if user presses a button
    if (digitalRead(PIN_YES) == LOW || digitalRead(PIN_NO) == LOW || digitalRead(PIN_RESET) == LOW) {
      break;
    }
  }
}

void freezeGuessScroll(const String& title, const String& guess) {
  // If short, just show it
  if (guess.length() <= 16) {
    freezeUntilReset(title, guess);
    return;
  }

  // Scroll line-2 until RESET
  int maxPos = guess.length() - 16;
  int pos = 0;

  while (digitalRead(PIN_RESET) != LOW) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(title.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(guess.substring(pos, pos + 16));

    pos++;
    if (pos > maxPos) pos = 0;

    delay(350);
  }
}


void setLEDProgress(int step, int maxSteps) {
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_Y, LOW);
  digitalWrite(LED_R, LOW);

  float t = (maxSteps <= 0) ? 0.0f : (float)step / (float)maxSteps;
  if (t < 0.34f) digitalWrite(LED_R, HIGH);
  else if (t < 0.75f) digitalWrite(LED_Y, HIGH);
  else digitalWrite(LED_G, HIGH);
}

void freezeUntilReset(const String& line1, const String& line2) {
  lcd2(line1, line2);
  while (digitalRead(PIN_RESET) != LOW) delay(30);
}

// ===================== WiFi =====================
void wifiConnect() {
  lcd2("WiFi connecting", "Please wait...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    dots = (dots + 1) % 4;
    lcd.setCursor(0, 1);
    lcd.print("Wait");
    for (int i = 0; i < dots; i++) lcd.print(".");
    lcd.print("        ");
    delay(400);
  }

  lcd2("WiFi connected!", WiFi.localIP().toString());
  delay(900);
}

// ===================== OpenAI Responses API =====================
// NOTE: Structured outputs moved from response_format -> text.format  :contentReference[oaicite:1]{index=1}
String openaiRequestRaw(const String& qaHistory, int qAsked, int maxQ) {
  gLastHttpCode = 0;
  gLastHttpError = "";

  WiFiClientSecure client;
  client.setInsecure();   // lab testing
  client.setTimeout(30);  // 30 seconds socket timeout


  HTTPClient https;
  https.begin(client, "https://api.openai.com/v1/responses");
  https.setTimeout(30000);  // 30 seconds read timeout
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", String("Bearer ") + OPENAI_KEY);

  // ---- request body ----
  StaticJsonDocument<8192> body;
  body["model"] = "gpt-4o-mini";

  String instructions =
    "You are an Akinator-style guessing AI.\n"
    "The user answers ONLY YES or NO.\n"
    "You must guess what the user is thinking.\n\n"

    "HARD RULES:\n"
    "1) Ask ONLY one YES/NO question at a time.\n"
    "2) Never ask open questions. Never ask for the name.\n"
    "3) Keep questions <= 60 characters.\n"
    "4) No safety-irrelevant weirdness: do not ask about microbes/fungi.\n"
    "5) Do not assume. Confirm with questions.\n\n"

    "CORE STRATEGY:\n"
    "You must behave like a search algorithm:\n"
    "• Keep multiple hypotheses.\n"
    "• Ask questions that split the remaining possibilities.\n"
    "• Prefer high-information questions.\n\n"

    "PHASE 1: TOP-LEVEL CLASSIFICATION (use in this order):\n"
    "Q1) Is it a living thing?\n"
    "If YES:\n"
    "  Q2) Is it a human?\n"
    "  If NO: Q3) Is it an animal?\n"
    "If NO:\n"
    "  Q4) Is it a non-living physical object?\n"
    "  If NO: Q5) Is it a fictional character?\n"
    "  If NO: Q6) Is it a place?\n"
    "  If NO: Q7) Is it an abstract thing (idea/subject)?\n\n"

    "PHASE 2: NARROWING (after a bucket is confirmed):\n"
    "Ask only bucket-relevant questions that split options.\n"
    "Examples:\n"
    "• Human: profession/field first (sports/music/actor/etc), then details.\n"
    "• Animal: class (bird/mammal/reptile/fish/insect), then features.\n"
    "• Object: category (device/food/tool/vehicle), then features.\n"
    "• Fictional: medium (anime/movie/game), then role/type.\n\n"

    "CRITICAL DISAMBIGUATION RULE:\n"
    "If there are close alternatives, you MUST ask a discriminator question.\n"
    "Do not guess between similar items without a distinguishing feature.\n"
    "Examples:\n"
    "• Tiger vs Lion: stripes? mane?\n"
    "• Parrot vs Pigeon: can mimic speech?\n"
    "• Messi vs Ronaldo: plays for Portugal? (or equivalent)\n"
    "• BTS member vs solo artist: part of BTS?\n\n"

    "PRE-GUESS VERIFICATION (MANDATORY):\n"
    "Before you output a guess, you MUST ask ONE final YES/NO verification\n"
    "question that would clearly confirm your top guess.\n"
    "Example: 'Does it have stripes?' before guessing Tiger.\n"
    "Only after that verification, output type='guess'.\n\n"

    "GUESSING POLICY:\n"
    "• Do NOT guess early unless you have asked a discriminator or verification.\n"
    "• If qAsked is near the limit, switch to verification then guess.\n"
    "• When qAsked >= "
    + String(maxQ) + ", output type='guess' immediately.\n"
                     "• If forced and unsure, make the best guess.\n\n"

                     "OUTPUT FORMAT:\n"
                     "Return ONLY JSON:\n"
                     "{\"type\":\"question\",\"text\":\"...\"}\n"
                     "or\n"
                     "{\"type\":\"guess\",\"text\":\"...\"}\n\n"

                     "Q/A so far:\n"
    + qaHistory + "\n\n"
                  "Now output the next step.";



  body["input"] = instructions;
  body["max_output_tokens"] = 120;

  // ---- Structured Outputs via text.format (json_schema) ----
  JsonObject text = body.createNestedObject("text");
  JsonObject format = text.createNestedObject("format");

  format["type"] = "json_schema";
  format["name"] = "guess_machine_step";
  format["strict"] = true;

  JsonObject schema = format.createNestedObject("schema");
  schema["type"] = "object";

  JsonObject props = schema.createNestedObject("properties");

  JsonObject pType = props.createNestedObject("type");
  pType["type"] = "string";
  JsonArray typeEnum = pType.createNestedArray("enum");
  typeEnum.add("question");
  typeEnum.add("guess");

  JsonObject pText = props.createNestedObject("text");
  pText["type"] = "string";
  pText["maxLength"] = 60;

  JsonArray req = schema.createNestedArray("required");
  req.add("type");
  req.add("text");

  schema["additionalProperties"] = false;

  String payload;
  serializeJson(body, payload);

  int code = https.POST(payload);
  gLastHttpCode = code;
  String resp = https.getString();
  https.end();

  if (code != 200) {
    DynamicJsonDocument edoc(8192);
    if (!deserializeJson(edoc, resp) && edoc.containsKey("error")) {
      gLastHttpError = String((const char*)(edoc["error"]["message"] | "API error"));
    }
  }
  return resp;
}

// Extract the JSON step (it may arrive as output_text containing JSON)
bool extractStepJSON(const String& raw, String& typeOut, String& textOut) {
  DynamicJsonDocument doc(60000);
  if (deserializeJson(doc, raw)) return false;

  // API-level error
  if (doc.containsKey("error") && !doc["error"].isNull()) {
    typeOut = "guess";
    textOut = "API error";
    return true;
  }


  if (!doc.containsKey("output")) return false;

  JsonArray output = doc["output"].as<JsonArray>();
  for (JsonVariant item : output) {
    if (item["type"] == "message" && item.containsKey("content")) {
      JsonArray content = item["content"].as<JsonArray>();
      for (JsonVariant c : content) {
        if (c["type"] == "output_text" && c.containsKey("text")) {

          // 🔑 This text itself is JSON
          const char* inner = c["text"];
          DynamicJsonDocument innerDoc(256);
          if (deserializeJson(innerDoc, inner)) return false;

          typeOut = String((const char*)innerDoc["type"]);
          textOut = String((const char*)innerDoc["text"]);
          return true;
        }
      }
    }
  }
  return false;
}


bool parseStep(const String& stepJson, String& typeOut, String& textOut) {
  DynamicJsonDocument sdoc(1024);
  if (deserializeJson(sdoc, stepJson)) return false;
  typeOut = String((const char*)sdoc["type"]);
  textOut = String((const char*)sdoc["text"]);
  return (typeOut.length() > 0 && textOut.length() > 0);
}

// ===================== GAME =====================
String qaHistory;
int qAsked = 0;
static const int MAX_Q = 20;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Booting Guess Machine...");

  pinMode(PIN_YES, INPUT_PULLUP);
  pinMode(PIN_NO, INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP);

  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(BUZZ, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.begin();
  lcd.backlight();

  wifiConnect();
}
bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  lcd2("WiFi lost", "Reconnecting");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 8000) return false;  // 8 sec timeout
    delay(300);
  }

  lcd2("WiFi restored", "Continuing...");
  delay(700);
  return true;
}

void loop() {
  lcd2("GUESS MACHINE", "YES to start");
  while (true) {
    Ans a = readAns();
    if (a == A_YES) break;
  }

  lcd2("Think of ANY", "thing/person");
  delay(900);
  lcd2("Ready?", "Press YES");
  while (true) {
    Ans a = readAns();
    if (a == A_RESET) return;
    if (a == A_YES) break;
  }

  qaHistory = "";
  qAsked = 0;

  while (true) {
    if (digitalRead(PIN_RESET) == LOW) return;

    setLEDProgress(qAsked, MAX_Q);

    lcd2("Asking AI...", "Please wait");
    String extraHint = "";
    if (qAsked >= 8) extraHint = "\nSYSTEM: You may GUESS now if you are confident.\n";

    String raw = openaiRequestRaw(qaHistory + extraHint, qAsked, MAX_Q);

    Serial.println("HTTP Code: " + String(gLastHttpCode));
    if (gLastHttpError.length()) Serial.println("HTTP Error: " + gLastHttpError);
    Serial.println(raw);

    if (gLastHttpCode != 200) {

      // Special handling for network drop
      if (gLastHttpCode == -1) {
        lcd2("Network issue", "Retrying...");
        delay(500);

        if (!ensureWiFi()) {
          freezeUntilReset("WiFi failed", "Press RESET");
          return;
        }

        // Retry SAME step without incrementing qAsked
        continue;
      }

      // Real API errors (quota, auth, etc.)
      String l1 = "HTTP " + String(gLastHttpCode);
      String l2 = gLastHttpError.length()
                    ? gLastHttpError.substring(0, 16)
                    : "API problem";
      freezeUntilReset(l1, l2);
      return;
    }


    String type, text;
    if (!extractStepJSON(raw, type, text)) {
      freezeUntilReset("Parse error", "Press RESET");
      return;
    }
    // ✅ Hard enforcement at limit: if max questions reached, force a guess
    if (qAsked >= MAX_Q && type != "guess") {
      lcd2("Forcing guess", "Wait...");
      delay(300);

      String raw2 = openaiRequestRaw(
        qaHistory + "\nSYSTEM: You MUST GUESS NOW. Do not ask more questions.\n",
        qAsked,
        MAX_Q);

      if (!extractStepJSON(raw2, type, text)) {
        freezeUntilReset("Parse error", "Press RESET");
        return;
      }
    }
    if (type == "guess") {

      // Show guess (scroll if needed)
      // Show guess for a few seconds (scrolling if needed)
      unsigned long start = millis();
      int maxPos = max(0, (int)text.length() - 16);
      int pos = 0;

      while (millis() - start < 4000) {  // show for ~4 seconds
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("My guess is:");
        lcd.setCursor(0, 1);
        lcd.print(text.substring(pos, pos + 16));

        pos++;
        if (pos > maxPos) pos = 0;

        delay(350);
      }


      // Ask for confirmation
      lcd2("Am I right?", "YES / NO");

      Ans result = readAns();

      if (result == A_YES) {
        // ✅ AI wins
        digitalWrite(LED_G, HIGH);
        digitalWrite(LED_Y, LOW);
        digitalWrite(LED_R, LOW);

        beep(3000, 120);
        delay(100);
        beep(3500, 120);

        freezeUntilReset("AI WINS 🎉", "I guessed it!");
      } else if (result == A_NO) {
        // ❌ AI loses
        digitalWrite(LED_G, LOW);
        digitalWrite(LED_Y, LOW);
        digitalWrite(LED_R, HIGH);

        beepWrong();

        freezeUntilReset("AI LOSES 😢", "Try again");
      }

      return;
    }


    // Ask one question
    scrollQuestion(text, "YES / NO");
    Ans a = readAns();
    if (a == A_RESET) return;

    qAsked++;
    String ansText = (a == A_YES) ? "YES" : "NO";
    qaHistory += "Q: " + text + "\nA: " + ansText + "\n\n";

    // keep history bounded
    if (qaHistory.length() > 1400) {
      qaHistory = qaHistory.substring(qaHistory.length() - 1100);
    }
  }
}
