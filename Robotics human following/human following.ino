#include <AFMotor.h>
#include <Servo.h>

// Motors
AF_DCMotor motor1(1); // Left front
AF_DCMotor motor2(2); // Left rear
AF_DCMotor motor3(3); // Right front
AF_DCMotor motor4(4); // Right rear

// Servo
Servo myServo;

// Ultrasonic pins
#define trigPin 9
#define echoPin 10

long duration;
int distance;

int leftDistance, rightDistance, centerDistance;

// Function to measure distance
int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  
  return distance;
}

// Movement functions
void moveForward() {
  motor1.setSpeed(200);
  motor2.setSpeed(200);
  motor3.setSpeed(200);
  motor4.setSpeed(200);

  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
}

void stopMoving() {
  motor1.run(RELEASE);
  motor2.run(RELEASE);
  motor3.run(RELEASE);
  motor4.run(RELEASE);
}

void turnLeft() {
  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
}

void turnRight() {
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(BACKWARD);
  motor4.run(BACKWARD);
}

void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  myServo.attach(11);
  myServo.write(90); // Center position

  delay(1000);
}

void loop() {

  // Look center
  myServo.write(90);
  delay(300);
  centerDistance = getDistance();
  Serial.print("Center: ");
  Serial.println(centerDistance);

  // Look left
  myServo.write(150);
  delay(300);
  leftDistance = getDistance();
  Serial.print("Left: ");
  Serial.println(leftDistance);

  // Look right
  myServo.write(30);
  delay(300);
  rightDistance = getDistance();
  Serial.print("Right: ");
  Serial.println(rightDistance);

  // Back to center
  myServo.write(90);

  // Decision logic
  if (centerDistance > 20 && centerDistance < 100) {
    moveForward();
  }
  else if (centerDistance <= 20) {
    stopMoving();
  }
  else {
    if (leftDistance > rightDistance) {
      turnLeft();
      delay(400);
    } else {
      turnRight();
      delay(400);
    }
  }

  delay(200);
}