#include <Arduino.h>
#include <ESP32Servo.h>

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LED_GREEN_PIN = 4;
const int LED_YELLOW_PIN = 22;
const int LED_RED_PIN = 23;
const int BUZZER_PIN = 21;
const int SERVO_RIGHT_PIN = 19;
const int SERVO_LEFT_PIN = 13;
const int TOUCH_RIGHT_PIN = 27;
const int TOUCH_LEFT_PIN = 26;
const int BUTTON_PIN = 25;

const int CW_US = 1000;
const int CCW_US = 2000;
const int STOP_US = 1500;
const unsigned long LEFT_DELAY_MS = 2500;

enum State { IDLE, RUNNING, FINISHED, REVERSING };
State state = IDLE;

Servo servoRight;
Servo servoLeft;

int baseRight = 0;
int baseLeft = 0;
bool rightRunning = false, leftRunning = false;
bool rightDone = false, leftDone = false;
bool rearmWait = false;
unsigned long rightStartMs = 0;

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999.0;
  return (duration * 0.0343) / 2.0;
}

bool touched(int pin, int base) {
  if (digitalRead(pin) == base) return false;
  delay(10);
  return digitalRead(pin) != base;
}

bool buttonDown() {
  if (digitalRead(BUTTON_PIN) == LOW) return true;
  delay(30);
  return digitalRead(BUTTON_PIN) == LOW;
}

void setRed(bool on) {
  digitalWrite(LED_RED_PIN, on);
  digitalWrite(BUZZER_PIN, on);
}

void leds(bool g, bool y, bool r) {
  digitalWrite(LED_GREEN_PIN, g);
  digitalWrite(LED_YELLOW_PIN, y);
  setRed(r);
}

void startRight() {
  servoRight.attach(SERVO_RIGHT_PIN, 1000, 2000);
  servoRight.writeMicroseconds(CW_US);
  digitalWrite(BUZZER_PIN, LOW);
  rightRunning = true;
  rightStartMs = millis();
  Serial.println("Right started");
}

void startLeft() {
  servoLeft.attach(SERVO_LEFT_PIN, 1000, 2000);
  servoLeft.writeMicroseconds(CW_US);
  leftRunning = true;
  Serial.println("Left started");
}

void stopRight() {
  servoRight.writeMicroseconds(STOP_US);
  servoRight.detach();
  rightRunning = false;
  rightDone = true;
  Serial.println("Right stopped");
}

void stopLeft() {
  servoLeft.writeMicroseconds(STOP_US);
  servoLeft.detach();
  leftRunning = false;
  leftDone = true;
  Serial.println("Left stopped");
}

void enterReverse() {
  if (!servoRight.attached()) servoRight.attach(SERVO_RIGHT_PIN, 1000, 2000);
  if (!servoLeft.attached())  servoLeft.attach(SERVO_LEFT_PIN, 1000, 2000);
  servoRight.writeMicroseconds(CCW_US);
  servoLeft.writeMicroseconds(CCW_US);
  rightRunning = leftRunning = false;
  leds(false, false, false);
  state = REVERSING;
  Serial.println("Reversing");
}

void leaveReverse() {
  servoRight.writeMicroseconds(STOP_US);
  servoLeft.writeMicroseconds(STOP_US);
  servoRight.detach();
  servoLeft.detach();
  rightDone = leftDone = false;
  rearmWait = true;
  state = IDLE;
  Serial.println("Back to initial state");
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TOUCH_RIGHT_PIN, INPUT);
  pinMode(TOUCH_LEFT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  delay(300);
  baseRight = digitalRead(TOUCH_RIGHT_PIN);
  baseLeft = digitalRead(TOUCH_LEFT_PIN);
  Serial.printf("Base R=%d L=%d\n", baseRight, baseLeft);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servoRight.setPeriodHertz(50);
  servoLeft.setPeriodHertz(50);
  leds(false, false, false);
}

void loop() {
  if (state != REVERSING && buttonDown()) enterReverse();

  switch (state) {

    case IDLE: {
      float d = readDistance();
      bool valid = (d > 0.0 && d < 900.0);
      Serial.println(d);
      if (rearmWait && (!valid || d >= 16.0)) rearmWait = false;

      if (!valid || d > 36.0)  leds(false, false, false);
      else if (d >= 30.0)      leds(true,  false, false);
      else if (d >= 22.0)      leds(false, true,  false);
      else if (d >= 16.0)      leds(false, false, true);
      else {
        leds(false, false, true);
        if (!rearmWait) {
          startRight();
          state = RUNNING;
        }
      }
      delay(60);
      break;
    }

    case RUNNING:
      if (!leftRunning && !leftDone && millis() - rightStartMs >= LEFT_DELAY_MS) startLeft();
      if (rightRunning && touched(TOUCH_RIGHT_PIN, baseRight)) stopRight();
      if (leftRunning && touched(TOUCH_LEFT_PIN, baseLeft)) stopLeft();

      if (rightDone && leftDone) {
        leds(false, false, false);
        state = FINISHED;
      }
      delay(5);
      break;

    case FINISHED:
      delay(20);
      break;

    case REVERSING:
      if (!buttonDown()) leaveReverse();
      delay(10);
      break;
  }
}
