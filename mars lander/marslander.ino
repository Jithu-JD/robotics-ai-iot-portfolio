#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ---------------- WIFI ----------------
const char* ssid = "Jithu";
const char* password = "kichu@1234";
WiFiServer server(80);

// ---------------- OLED ----------------
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ---------------- BMP280 ----------------
Adafruit_BMP280 bmp;

// ---------------- DHT ----------------
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------------- UV ----------------
#define UV_PIN 34

// ---------------- TIMING ----------------
unsigned long lastUpdate = 0;

// ================= SETUP =================
void setup() {

  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  // OLED INIT
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not detected");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 20);
  display.println("Mars Lander");
  display.display();
  delay(1500);

  // SENSOR INIT
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found");
  }

  dht.begin();

  // WIFI CONNECT
  Serial.println("Connecting WiFi...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi");
  display.display();

  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi OK");
    display.println(WiFi.localIP());
    display.display();

  } else {
    Serial.println("\nWiFi FAILED");

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed");
    display.display();
  }

  server.begin();
  delay(2000);
}

// ================= LOOP =================
void loop() {

  // -------- READ SENSORS --------
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float p = bmp.readPressure() / 100.0;
  int uvRaw = analogRead(UV_PIN);
  float uv = (uvRaw * 3.3 / 4095.0) * 10.0;

  // -------- OLED UPDATE --------
  if (millis() - lastUpdate > 2000) {

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("MARS LANDER");

    display.print("Temp: ");
    if (isnan(t)) display.println("Err");
    else display.println(t);

    display.print("Hum : ");
    if (isnan(h)) display.println("Err");
    else display.println(h);

    display.print("Pres: ");
    display.println(p);

    display.print("UV  : ");
    display.println(uv);

    display.display();
    lastUpdate = millis();
  }

  // -------- WEB SERVER --------
  WiFiClient client = server.available();

  if (client) {
    client.readStringUntil('\r');
    client.flush();

    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html\n");

    client.println("<html><head>");
    client.println("<meta http-equiv='refresh' content='3'>");
    client.println("<style>");
    client.println("body{font-family:Arial;text-align:center;background:#000;color:#0f0;}");
    client.println(".box{border:1px solid #0f0;margin:10px;padding:15px;border-radius:8px;}");
    client.println("</style></head><body>");

    client.println("<h2>🚀 Mars Lander Monitor</h2>");
    client.println("<div class='box'>Temp: " + String(t) + " C</div>");
    client.println("<div class='box'>Humidity: " + String(h) + " %</div>");
    client.println("<div class='box'>Pressure: " + String(p) + " hPa</div>");
    client.println("<div class='box'>UV Index: " + String(uv) + "</div>");

    client.println("</body></html>");
    client.stop();
  }
}
