#include <Servo.h>

// Define flame sensor pins
#define FLAME_SENSOR_LEFT A0
#define FLAME_SENSOR_CENTER A1
#define FLAME_SENSOR_RIGHT A2

// Define motor driver pins
#define MOTOR_L1 5  // Left Motor Forward
#define MOTOR_L2 6  // Left Motor Backward
#define MOTOR_R1 9  // Right Motor Forward
#define MOTOR_R2 10 // Right Motor Backward

// Define relay for water pump
#define RELAY_PIN 7

// Define servo motor pin
#define SERVO_PIN 3

// Create Servo object
Servo waterServo;

// Flame sensor threshold
#define FIRE_DETECTED_THRESHOLD 500  // Adjust based on sensor readings

void setup() {
    // Initialize Serial Monitor
    Serial.begin(9600);

    // Set motor pins as output
    pinMode(MOTOR_L1, OUTPUT);
    pinMode(MOTOR_L2, OUTPUT);
    pinMode(MOTOR_R1, OUTPUT);
    pinMode(MOTOR_R2, OUTPUT);

    // Set relay as output
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // Ensure relay (pump) is OFF at startup

    // Initialize servo motor
    waterServo.attach(SERVO_PIN);
    waterServo.write(90); // Set initial servo position to center

    // Set flame sensor pins as input
    pinMode(FLAME_SENSOR_LEFT, INPUT);
    pinMode(FLAME_SENSOR_CENTER, INPUT);
    pinMode(FLAME_SENSOR_RIGHT, INPUT);

    Serial.println("🚀 Fire Fighting Robot Initialized...");
}

void loop() {
    // Read flame sensor values
    int flameLeft = analogRead(FLAME_SENSOR_LEFT);
    int flameCenter = analogRead(FLAME_SENSOR_CENTER);
    int flameRight = analogRead(FLAME_SENSOR_RIGHT);

    // Print sensor values to Serial Monitor
    Serial.print("Flame Left: ");
    Serial.print(flameLeft);
    Serial.print(" | Flame Center: ");
    Serial.print(flameCenter);
    Serial.print(" | Flame Right: ");
    Serial.println(flameRight);

    // Check flame detection
    if (flameCenter < FIRE_DETECTED_THRESHOLD) {
        Serial.println("🔥 Fire Detected at Center! Moving Forward...");
        moveForward();
        activateWaterPump();
    }
    else if (flameLeft < FIRE_DETECTED_THRESHOLD) {
        Serial.println("🔥 Fire Detected at Left! Turning Left...");
        turnLeft();
        activateWaterPump();
    }
    else if (flameRight < FIRE_DETECTED_THRESHOLD) {
        Serial.println("🔥 Fire Detected at Right! Turning Right...");
        turnRight();
        activateWaterPump();
    }
    else {
        Serial.println("✅ No Fire Detected. Stopping...");
        stopMotors();
        deactivateWaterPump();  // Ensure pump is OFF when no fire is detected
    }

    delay(500); // Delay for stability
}

// Function to move forward
void moveForward() {
    digitalWrite(MOTOR_L1, HIGH);
    digitalWrite(MOTOR_L2, LOW);
    digitalWrite(MOTOR_R1, HIGH);
    digitalWrite(MOTOR_R2, LOW);
}

// Function to turn left
void turnLeft() {
    digitalWrite(MOTOR_L1, LOW);
    digitalWrite(MOTOR_L2, HIGH);
    digitalWrite(MOTOR_R1, HIGH);
    digitalWrite(MOTOR_R2, LOW);
    delay(500);  // Adjust timing
    stopMotors();
}

// Function to turn right
void turnRight() {
    digitalWrite(MOTOR_L1, HIGH);
    digitalWrite(MOTOR_L2, LOW);
    digitalWrite(MOTOR_R1, LOW);
    digitalWrite(MOTOR_R2, HIGH);
    delay(500);  // Adjust timing
    stopMotors();
}

// Function to stop motors
void stopMotors() {
    digitalWrite(MOTOR_L1, LOW);
    digitalWrite(MOTOR_L2, LOW);
    digitalWrite(MOTOR_R1, LOW);
    digitalWrite(MOTOR_R2, LOW);
}

// Function to activate water pump and move servo
void activateWaterPump() {
    Serial.println("🚰 Activating Water Pump...");
    digitalWrite(RELAY_PIN, HIGH);  // Turn ON the water pump

    // Move the servo from left to right
    for (int pos = 60; pos <= 120; pos += 5) {  
        waterServo.write(pos);
        delay(100);
    }

    // Move the servo from right to left
    for (int pos = 120; pos >= 60; pos -= 5) {  
        waterServo.write(pos);
        delay(100);
    }

    delay(2000);  // Pump stays on for 2 seconds
    deactivateWaterPump();  // Turn off the water pump
}

// Function to deactivate water pump
void deactivateWaterPump() {
    Serial.println("✅ Deactivating Water Pump...");
    digitalWrite(RELAY_PIN, LOW);  // Turn OFF the water pump
    waterServo.write(90);  // Reset servo to center position
}


