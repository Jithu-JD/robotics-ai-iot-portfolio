#include <Servo.h>

#define SERVO1_PIN 11
#define SERVO2_PIN 10
#define WATER_SENSOR A0
#define RED_LED 7

Servo servo1;
Servo servo2;

int threshold = 400;
bool bridgeRaised = false;

void setup() {
  Serial.begin(9600);

  pinMode(RED_LED, OUTPUT);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  servo1.write(20);
  servo2.write(20);
}

void loop() {

  int waterValue = analogRead(WATER_SENSOR);

  if (waterValue > threshold && !bridgeRaised) {

    for (int pos = 20; pos <= 100; pos++) {
      servo1.write(pos);
      servo2.write(pos);
      delay(25);
    }

    digitalWrite(RED_LED, HIGH);
    bridgeRaised = true;
  }

  if (waterValue <= threshold && bridgeRaised) {

    for (int pos = 100; pos >= 20; pos--) {
      servo1.write(pos);
      servo2.write(pos);
      delay(25);
    }

    digitalWrite(RED_LED, LOW);
    bridgeRaised = false;
  }
}
