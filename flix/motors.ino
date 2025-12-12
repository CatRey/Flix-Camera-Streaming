// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Motors output control using MOSFETs
// In case of using ESCs, change PWM_STOP, PWM_MIN and PWM_MAX to appropriate values in μs, decrease PWM_FREQUENCY (to 400)

#include "util.h"
#define WHITE 0
#if WHITE
#define MOTOR_0_PIN 42 // rear left
#define MOTOR_1_PIN 11 // rear right
#define MOTOR_2_PIN 7 // front right
#define MOTOR_3_PIN 21 // front left
#else
#define MOTOR_0_PIN 42 // rear left
#define MOTOR_1_PIN 46 // rear right
#define MOTOR_2_PIN 45 // front right
#define MOTOR_3_PIN 21 // front left
#endif
#define PWM_FREQUENCY 20000
#define PWM_RESOLUTION 10
#define PWM_STOP 0
#define PWM_MIN 0
#define PWM_MAX 1000000 / PWM_FREQUENCY

// Motors array indexes:
const int MOTOR_REAR_LEFT = 0;
const int MOTOR_REAR_RIGHT = 1;
const int MOTOR_FRONT_RIGHT = 2;
const int MOTOR_FRONT_LEFT = 3;

void setupMotors() {
	print("Setup Motors\n");

	// configure pins
	analogWriteResolution(MOTOR_0_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_0_PIN, PWM_FREQUENCY);
  analogWriteResolution(MOTOR_1_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_1_PIN, PWM_FREQUENCY);
  analogWriteResolution(MOTOR_2_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_2_PIN, PWM_FREQUENCY);
  analogWriteResolution(MOTOR_3_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_3_PIN, PWM_FREQUENCY);

	sendMotors();
	print("Motors initialized\n");
}

int getDutyCycle(float value) {
	value = constrain(value, 0, 1);
	float pwm = mapff(value, 0, 1, PWM_MIN, PWM_MAX);
	if (value == 0) pwm = PWM_STOP;
	float duty = mapff(pwm, 0, 1000000 / PWM_FREQUENCY, 0, (1 << PWM_RESOLUTION) - 1);
	return round(duty);
}

void sendMotors() {
  analogWriteResolution(MOTOR_0_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_0_PIN, PWM_FREQUENCY);
  analogWriteResolution(MOTOR_1_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_1_PIN, PWM_FREQUENCY);
  analogWriteResolution(MOTOR_2_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_2_PIN, PWM_FREQUENCY);
  analogWriteResolution(MOTOR_3_PIN,PWM_RESOLUTION);
  analogWriteFrequency(MOTOR_3_PIN, PWM_FREQUENCY);
	analogWrite(MOTOR_0_PIN, getDutyCycle(motors[0]));
	analogWrite(MOTOR_1_PIN, getDutyCycle(motors[1]));
	analogWrite(MOTOR_2_PIN, getDutyCycle(motors[2]));
  analogWrite(MOTOR_3_PIN, getDutyCycle(motors[3]));
}

bool motorsActive() {
	return motors[0] != 0 || motors[1] != 0 || motors[2] != 0 || motors[3] != 0;
}

void testMotor(int n) {
	print("Testing motor %d\n", n);
	motors[n] = 1;
	delay(50); // ESP32 may need to wait until the end of the current cycle to change duty https://github.com/espressif/arduino-esp32/issues/5306
	sendMotors();
	pause(3);
	motors[n] = 0;
	sendMotors();
	print("Done\n");
}
