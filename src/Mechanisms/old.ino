/** @file old.ino
 * @brief TMC2209 brench test that Harsh was doing. It's very, very basic. This is old code that should be ported over to the mechanism's HAL (it will basically need to be entirely rewritten).
 *
 */

#include <Arduino.h>

constexpr uint8_t DIR_PIN  = 4;
constexpr uint8_t STEP_PIN = 5;
constexpr uint8_t EN_PIN   = 6;

// Time between step pulses.
// 2000 us = 500 microsteps per second.
constexpr uint32_t STEP_INTERVAL_US = 8000;
constexpr uint32_t STEP_PULSE_US = 5;

bool motorRunning = false;
uint32_t previousStepTime = 0;

void startMotor(bool direction)
{
  digitalWrite(EN_PIN, HIGH);  // Disable while changing direction
  digitalWrite(DIR_PIN, direction ? HIGH : LOW);
  delayMicroseconds(10);       // DIR setup time

  digitalWrite(EN_PIN, LOW);   // Enable driver
  motorRunning = true;
  previousStepTime = micros();
}

void stopMotor()
{
  motorRunning = false;
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(EN_PIN, HIGH);  // Disable and release motor
}

void setup()
{
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);  // Start disabled

  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);

  digitalWrite(DIR_PIN, LOW);
  digitalWrite(STEP_PIN, LOW);

  Serial.begin(115200);
  delay(1000);

  Serial.println("Continuous TMC2209 test ready");
  Serial.println("F = forward");
  Serial.println("R = reverse");
  Serial.println("X = stop and release");
}

void loop()
{
  // Read commands without interrupting the stepping loop
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == 'F' || command == 'f') {
      startMotor(true);
      Serial.println("Running forward");
    }

    else if (command == 'R' || command == 'r') {
      startMotor(false);
      Serial.println("Running reverse");
    }

    else if (command == 'X' || command == 'x') {
      stopMotor();
      Serial.println("Stopped");
    }
  }

  // Generate continuous STEP pulses
  if (motorRunning) {
    uint32_t currentTime = micros();

    if (currentTime - previousStepTime >= STEP_INTERVAL_US) {
      previousStepTime = currentTime;

      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(STEP_PULSE_US);
      digitalWrite(STEP_PIN, LOW);
    }
  }
}