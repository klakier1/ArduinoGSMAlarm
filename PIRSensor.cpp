/*
 * PIRSensor.cpp
 *
 *  Created on: 23 mar 2020
 *      Author: lukas
 */
#include <Arduino.h>
#include "PIRSensor.h"

PIRSensor::PIRSensor(int pinNumber) {
	this->pinNumber = pinNumber;
	pinMode(pinNumber, INPUT);
}

PIRSensor::PIRSensor(int pinNumber, double alarmDelay) :
		PIRSensor(pinNumber) {
	this->alarmDelay = alarmDelay;
}

PIRSensor::PIRSensor(int pinNumber, double alarmDelay,
		void (*callback)(AlarmEvent, PIRSensor*, void*)) :
		PIRSensor(pinNumber, alarmDelay) {
	this->alarmCallback = callback;
}

PIRSensor::~PIRSensor() {
	// TODO Auto-generated destructor stub
}

void PIRSensor::Loop(void) {
	checkEdge();

	if ((actEvent == Rising || (actEvent == Nc && sensorState == HIGH))
			&& !detectionStarted) {

		startTime = micros();
		detectionStarted = true;
		InvokeCallback(DetectionStart, NULL);

	} else if ((actEvent == Falling || actEvent == Nc) && detectionStarted
			&& !alarmActivated) {

		stopTime = micros();
		double time = stopTime - startTime;
		time /= 1000000.0;
		if (time > alarmDelay) {
			alarmActivated = true;
			if (armed) {
				InvokeCallback(AlarmStart, NULL);
			} else {
				InvokeCallback(AlarmStartNotArmed, NULL);
			}
		}
	}

	if ((actEvent == Falling || (actEvent == Nc && sensorState == LOW))
			&& detectionStarted) {

		stopTime = micros();
		double time = stopTime - startTime;
		time /= 1000000.0;
		alarmActivated = false;
		detectionStarted = false;
		InvokeCallback(DetectionStop, (void*) &time);

	}
}

void PIRSensor::checkEdge(void) {
	prevSensorState = sensorState;
	sensorState = digitalRead(pinNumber);

	if (sensorState > prevSensorState)
		actEvent = Rising;
	else if (sensorState < prevSensorState)
		actEvent = Falling;
	else
		actEvent = Nc;
}

void PIRSensor::InvokeCallback(AlarmEvent alarmEvent, void *args) {
	if (this->alarmCallback != NULL)
		alarmCallback(alarmEvent, this, args);
}

void PIRSensor::Arm(bool state) {
	armed = state;
}

bool PIRSensor::IsArmed(){
	return armed;
}
