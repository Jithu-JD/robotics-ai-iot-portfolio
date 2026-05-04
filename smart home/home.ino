#define BLYNK_TEMPLATE_ID "TMPL3kETLXPvE"
#define BLYNK_TEMPLATE_NAME "Home Kitchen"
#define BLYNK_AUTH_TOKEN "xO3qm2Om0d1TzJZZFx2Dw_iMd5QhBshp"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

char ssid[] = "Jithu";
char pass[] = "kichu@1234";

#define DHTPIN 4
#define DHTTYPE DHT11
#define GAS_PIN 34
#define FIRE_PIN 35
#define RELAY_PUMP 26
#define RELAY_MOTOR 27
#define BUZZER 14
#define RED_LED 12
#define SERVO_PIN 18

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

DHT dht(DHTPIN, DHTTYPE);
Servo servo;
BlynkTimer timer;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// ---------------- BLYNK CONTROL ----------------

BLYNK_WRITE(V4) { digitalWrite(RELAY_MOTOR, param.asInt()); }
BLYNK_WRITE(V5) { digitalWrite(RELAY_PUMP, param.asInt()); }
BLYNK_WRITE(V6) { servo.write(param.asInt()); }


// ---------------- OLED PRINT ----------------

void oledShow(String a, String b, String c) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(a);
  display.println(b);
  display.println(c);
  display.display();
}


// ---------------- SENSOR TASK ----------------

void sendData() {

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    oledShow("DHT ERROR", "Check Sensor", "");
    return;
  }

  int gas = digitalRead(GAS_PIN);
  int fire = digitalRead(FIRE_PIN);

  Blynk.virtualWrite(V0, t);
  Blynk.virtualWrite(V1, h);

  // ----- FIRE -----
  if (fire == LOW) {

    digitalWrite(RELAY_PUMP, HIGH);
    digitalWrite(RELAY_MOTOR, HIGH);
    digitalWrite(BUZZER, HIGH);
    digitalWrite(RED_LED, HIGH);

    oledShow("FIRE ALERT!", "Pump ON", "Motor ON");

    Blynk.virtualWrite(V3, "FIRE");
  }

  // ----- GAS -----
  else if (gas == HIGH) {

    servo.write(120);
    digitalWrite(RELAY_MOTOR, HIGH);
    digitalWrite(RED_LED, HIGH);

    oledShow("GAS LEAK!", "Window Open", "Vent ON");

    Blynk.virtualWrite(V2, "GAS LEAK");
  }

  // ----- SAFE -----
  else {

    digitalWrite(BUZZER, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(RELAY_PUMP, LOW);

    oledShow("Temp: " + String(t),
             "Hum: " + String(h),
             "STATUS: SAFE");

    Blynk.virtualWrite(V2, "SAFE");
    Blynk.virtualWrite(V3, "SAFE");
  }
}


// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);

  pinMode(GAS_PIN, INPUT);
  pinMode(FIRE_PIN, INPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_MOTOR, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(RELAY_PUMP, LOW);
  digitalWrite(RELAY_MOTOR, LOW);
  digitalWrite(BUZZER, LOW);
  digitalWrite(RED_LED, LOW);

  dht.begin();
  servo.attach(SERVO_PIN);

  // I2C pins for ESP32
  Wire.begin(21, 22);

  // Try OLED init at 0x3C (most common)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found at 0x3C, trying 0x3D...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println("OLED NOT FOUND!");
      while (true);
    }
  }

  oledShow("HOME KITCHEN", "SYSTEM START", "Connecting...");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  oledShow("WiFi OK", "Blynk OK", "System Ready");

  timer.setInterval(2000L, sendData);
}


// ---------------- LOOP ----------------

void loop() {
  Blynk.run();
  timer.run();
}
