#define BLYNK_TEMPLATE_ID "TMPL3KLBwlf-p"
#define BLYNK_TEMPLATE_NAME "Smart mirror"
#define BLYNK_AUTH_TOKEN "2gmedTq7fWg6TdtuALUNtkaYtr5fMYyM"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"

char ssid[] = "KIRAN";
char pass[] = "Kiran@2025#";

Adafruit_SSD1306 display(128, 64, &Wire, -1);

#define TRIG 5
#define ECHO 18

String taskText="Discipline beats motivation";
String taskTime="";
String taskDate="";
String userName="User";

// -------- Blynk Receivers --------
BLYNK_WRITE(V1){
  taskText = param.asString();
  Serial.println("Task: " + taskText);
}

BLYNK_WRITE(V2){
  taskTime = param.asString();
  Serial.println("Time: " + taskTime);
}

BLYNK_WRITE(V3){
  taskDate = param.asString();
  Serial.println("Date: " + taskDate);
}

BLYNK_WRITE(V4){
  userName = param.asString();
  Serial.println("Name: " + userName);
}

// -------- Distance --------
long distanceCM() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long d = pulseIn(ECHO, HIGH, 30000);
  return d * 0.034 / 2;
}

// -------- Greeting --------
String greet(int h){
  if(h<12) return "Good Morning";
  else if(h<17) return "Good Afternoon";
  else return "Good Evening";
}

void setup(){
  Serial.begin(115200);

  pinMode(TRIG,OUTPUT);
  pinMode(ECHO,INPUT);

  Wire.begin(21,22);
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Connecting...");
  display.display();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // 🔥 IMPORTANT — sync last dashboard values
  Blynk.syncAll();

  configTime(19800,0,"pool.ntp.org");
}

void loop(){
  Blynk.run();

  long d = distanceCM();
  Serial.println(d);

  if(d > 0 && d < 60){
    struct tm t;
    if(!getLocalTime(&t)) return;

    display.clearDisplay();
    display.setCursor(0,0);

    display.println(greet(t.tm_hour));
    display.println(userName);

    display.printf("%02d-%02d-%04d\n",
                   t.tm_mday,
                   t.tm_mon+1,
                   t.tm_year+1900);

    display.println("-----------");
    display.println(taskText);
    display.println(taskTime);

    display.display();
  }
  else {
    display.clearDisplay();
    display.display();
  }

  delay(500);
}
