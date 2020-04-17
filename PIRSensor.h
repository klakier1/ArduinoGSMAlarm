/*
 * PIRSensor.h
 *
 *  Created on: 23 mar 2020
 *      Author: lukas
 */

#ifndef PIRSENSOR_H_
#define PIRSENSOR_H_

enum EdgeEvent{
  Nc,				//no change
  Rising,
  Falling
};

enum AlarmEvent{
	DetectionStart,
	DetectionStop,
	AlarmStart,
	AlarmStop,
	AlarmStartNotArmed,
};

class PIRSensor {
public:
	PIRSensor(int pinNumber);
	PIRSensor(int pinNumber, double alarmDelay);
	PIRSensor(int pinNumber, double alarmDelay, void (*callback)(AlarmEvent, PIRSensor*, void*));
	virtual ~PIRSensor();
	void Loop(void);
	void Arm(bool state);
	bool IsArmed();
	int pinNumber;

private:
	bool armed = false;
	bool detectionStarted = false;
	double alarmDelay = 3.0;
	int sensorState = LOW;
	int prevSensorState = LOW;
	EdgeEvent actEvent = Nc;
	volatile unsigned long startTime = 0;
	volatile unsigned long stopTime = 0;
	bool alarmActivated = false;

	void (*alarmCallback)(AlarmEvent, PIRSensor*, void*) = NULL;

	void checkEdge(void);
	void InvokeCallback(AlarmEvent alarmEvent, void *arg);

};

#endif /* PIRSENSOR_H_ */
