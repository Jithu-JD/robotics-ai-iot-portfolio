#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== WIFI =====
const char* ssid = "StarHunterESP";
const char* password = "12345678";

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== SERVOS =====
Servo servoX;
Servo servoY;

#define SERVO_X_PIN 18
#define SERVO_Y_PIN 19
#define LASER_PIN   23

WebServer server(80);

// ===== STAR STRUCT =====
struct Star {
  String name;
  int x;
  int y;
};

Star stars[] = {
  {"Sirius", 40, 60},
  {"Betelgeuse", 90, 30},
  {"Polaris", 120, 70},
  {"Rigel", 60, 20},
  {"Vega", 150, 50}
};

int starCount = sizeof(stars) / sizeof(stars[0]);

// ===== WEB PAGE =====
String webpage() {
  String page = R"rawliteral(
  <html>
  <head>
  <title>Star Hunter</title>
  <style>
  body { font-family: Arial; text-align:center; background:black; color:white;}
  button { padding:15px; margin:10px; font-size:18px;}
  img { width:150px; }
  </style>
  </head>
  <body>
  <h1>🌟 Star Hunter</h1>
  )rawliteral";

  for (int i=0;i<starCount;i++) {
    page += "<div>";
    page += "<p>" + stars[i].name + "</p>";
    page += "<img src='https://upload.wikimedia.org/wikipedia/commons/thumb/5/5a/Star_symbol.svg/200px-Star_symbol.svg.png'><br>";
    page += "<button onclick=\"location.href='/star?name=" + stars[i].name + "'\">Select</button>";
    page += "</div><hr>";
  }

  page += "</body></html>";
  return page;
}

// ===== OLED DISPLAY =====
void showStar(String name) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,20);
  display.println(name);
  display.display();
}

// ===== MOVE LASER =====
void pointStar(String name) {
  for (int i=0;i<starCount;i++) {
    if (stars[i].name == name) {

      servoX.write(stars[i].x);
      servoY.write(stars[i].y);

      digitalWrite(LASER_PIN, HIGH);
      delay(2000);
      digitalWrite(LASER_PIN, LOW);

      showStar(name);
      break;
    }
  }
}

// ===== HANDLERS =====
void handleRoot() {
  server.send(200, "text/html", webpage());
}

void handleStar() {
  String starName = server.arg("name");
  pointStar(starName);
  server.send(200, "text/html", "<h2>Pointing to " + starName + "</h2><a href='/'>Back</a>");
}

// ===== SETUP =====
void setup() {

  Serial.begin(115200);

  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);

  servoX.attach(SERVO_X_PIN);
  servoY.attach(SERVO_Y_PIN);

  Wire.begin(21,22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  WiFi.softAP(ssid, password);
  Serial.println("AP Started");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/star", handleStar);
  server.begin();
}

// ===== LOOP =====
void loop() {
  server.handleClient();
}