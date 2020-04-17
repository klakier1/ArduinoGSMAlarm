/*********** Includes ***************************************************************/
#include "Arduino.h"
#include "PIRSensor.h"
#include "BGsmShield.h"
#include "Phonebook.h"
#include <string.h>
//#include "IntegerQueue.h"

/*********** Constants ***************************************************************/
const int sensor1Pin = 8;
const int sensor2Pin = 9;
const int ledPin = 13;      // the number of the LED pin

/*********** Global Variables ********************************************************/
PIRSensor *sensor1;
PIRSensor *sensor2;
BGsmShield *bgsm;
Phonebook phonebook;

int smsNumAlaram1;
int smsNumAlaram2;
int smsNumArmed;
int smsNumDisarmed;
int smsNumMemberAdd;
int smsNumMemberDelete;
int smsNumError;
int smsNumFixedMember;

/*********** Function Prototypes ****************************************************/
void sensorCallback(AlarmEvent event, PIRSensor *sensor, void *args);
void incomingVoiceCallback(char *num, size_t num_len);
bool incomingSmsCallback(char *num, size_t num_len, char *msg, size_t msg_len);
void errorHandler(String msg = "error");

/*********** User Code **************************************************************/
void setup() {
	Serial.begin(57600);

	sensor1 = new PIRSensor(sensor1Pin, 3.0, sensorCallback);
	sensor2 = new PIRSensor(sensor2Pin, 3.0, sensorCallback);
	bgsm = new BGsmShield();
	bgsm->SetIncomeVoiceCallback(incomingVoiceCallback);
	bgsm->SetIncomeSmsCallback(incomingSmsCallback);

	Serial.print("Init GSM... ");

	if (!bgsm->switchOn())
		errorHandler("Uklad GSM nie wlacza sie");

	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("AT", 2000, 10, "OK", 3))
		errorHandler("Brak odpowiedzi na test komunikacji");

	//Wy³acz echo
	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("ATE0", 2000, 10, "OK", 3))
		errorHandler("Echo off error");

	//Wpisz PIN
//	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("AT+CPIN=\"5007\"", 2000, 1000, "OK", 1))
//		errorHandler("Blad PIN NOK");

	//Tryb tekstowy
	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("AT+CMGF=1", 2000, 10, "OK", 1))
		errorHandler("text mode error");

	delay(1000);

	//Usun wszystkie SMS
	if (AT_RESP_OK
			!= bgsm->SendATCmdWaitResp("AT+CMGD=1,4", 2000, 10, "OK", 10))
		errorHandler("delete sms error");

	Serial.println("Done");

	Serial.print("StoreSMS... ");

	if (0 > (smsNumAlaram1 = bgsm->StoreSMS("!  ALARM 1  !")))
		errorHandler();
	if (0 > (smsNumAlaram2 = bgsm->StoreSMS("!  ALARM 2  !")))
		errorHandler();
	if (0 > (smsNumArmed = bgsm->StoreSMS("UZBROJONY")))
		errorHandler();
	if (0 > (smsNumDisarmed = bgsm->StoreSMS("ROZBROJONY")))
		errorHandler();
	if (0 > (smsNumMemberAdd = bgsm->StoreSMS("DODANO")))
		errorHandler();
	if (0 > (smsNumMemberDelete = bgsm->StoreSMS("USUNIETO")))
		errorHandler();
	if (0 > (smsNumError = bgsm->StoreSMS("BLAD")))
		errorHandler();
	if (0 > (smsNumFixedMember = bgsm->StoreSMS("JESTES STALYM UZYTKOWNIKIEM")))
		errorHandler();

	Serial.println("Done");

	bgsm->SetDebugNumber(phonebook.phoneNumbers[0]);

	phonebook.Print();
}

void loop() {
	sensor1->Loop();
	sensor2->Loop();
	bgsm->Loop();
}

void sensorCallback(AlarmEvent event, PIRSensor *sensor, void *args) {
	switch (event) {
	case DetectionStart: {
		Serial.print("Detection On ");
		Serial.println(sensor->pinNumber);
		break;
	}
	case DetectionStop: {
		double *time = args;
		Serial.print("Detection Off ");
		Serial.print(*time);
		Serial.print("s  ");
		Serial.println(sensor->pinNumber);
		break;
	}
	case AlarmStart: {
		for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
			char *number = phonebook.phoneNumbers[i];
			if (number != NULL) {
				if (sensor == sensor1)
					bgsm->SendSMSFromStorage(number, smsNumAlaram1);
				else if (sensor == sensor2)
					bgsm->SendSMSFromStorage(number, smsNumAlaram2);
			}
		}
		Serial.print("Alarm ");
		Serial.println(sensor->pinNumber);
		break;
	}
	case AlarmStartNotArmed: {
		Serial.print("NOT ARMED Alarm ");
		Serial.println(sensor->pinNumber);
		break;
	}
	}

}

void incomingVoiceCallback(char *num, size_t num_len) {
	Serial.print("Dzwonil: ");
	Serial.println(num);
	bool test = phonebook.Contains(num);
	if (test)
		Serial.println("Jest w bazie");
	else
		Serial.println("BRAK w bazie");
}

bool incomingSmsCallback(char *num, size_t num_len, char *msg, size_t msg_len) {
	Serial.print("Sms od: ");
	Serial.println(num);
	Serial.println(msg);
	number_status status = phonebook.Contains(num);

	switch (status) {
	case FIXED_MEMBER:

		if (memcmp("Dodaj", msg, 5) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumFixedMember);

		} else if (memcmp("Usun", msg, 4) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumFixedMember);

		} else if (memcmp("Status", msg, 6) == 0) {
			if (sensor1->IsArmed() && sensor2->IsArmed())
				bgsm->SendSMSFromStorage(num, smsNumArmed);
			else
				bgsm->SendSMSFromStorage(num, smsNumDisarmed);

		} else if (memcmp("Config", msg, 6) == 0) {
			if (bgsm->SendSMSBegin(num)) {
				//list of members
				for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
					if (phonebook.phoneNumbers[i] != NULL) {
						bgsm->SendSMSAttachText(phonebook.phoneNumbers[i]);
						bgsm->SendSMSAttachText("\n");
					}
				}
				//length of queue
				bgsm->SendSMSAttachText("QUEUE:");
				bgsm->SendSMSAttachInt(bgsm->_smsQueue.Size());
				bgsm->SendSMSAttachText("\n");
				//send
				bgsm->SendSMSEnd();
			}
		}

		else if (memcmp("Start", msg, 5) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumArmed);
			sensor1->Arm(true);
			sensor2->Arm(true);

		} else if (memcmp("Stop", msg, 4) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumDisarmed);
			sensor1->Arm(false);
			sensor2->Arm(false);

		} else {
			return false;
		}

		break;
	case MEMBER: {

		if (memcmp("Dodaj", msg, 5) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumMemberAdd);

		} else if (memcmp("Usun", msg, 4) == 0) {
			if (phonebook.Delete(num))
				bgsm->SendSMSFromStorage(num, smsNumMemberDelete);
			else
				bgsm->SendSMSFromStorage(num, smsNumError);

		} else if (memcmp("Status", msg, 6) == 0) {
			if (sensor1->IsArmed() && sensor2->IsArmed())
				bgsm->SendSMSFromStorage(num, smsNumArmed);
			else
				bgsm->SendSMSFromStorage(num, smsNumDisarmed);

		} else if (memcmp("Start", msg, 5) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumArmed);
			sensor1->Arm(true);
			sensor2->Arm(true);

		} else if (memcmp("Stop", msg, 4) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumDisarmed);
			sensor1->Arm(false);
			sensor2->Arm(false);

		} else {
			return false;
		}

		break;
	}
	case STRANGER: {

		if (memcmp("Dodaj", msg, 5) == 0) {
			if (phonebook.Add(num))
				bgsm->SendSMSFromStorage(num, smsNumMemberAdd);
			else
				bgsm->SendSMSFromStorage(num, smsNumError);

		} else {
			return false;
		}

		break;
	}
	}

	phonebook.Print();
	return true;

}

void errorHandler(String msg) {
	while (1) {
		Serial.println(msg);
		digitalRead(ledPin) ?
				digitalWrite(ledPin, LOW) : digitalWrite(ledPin, HIGH);
		delay(1000);
	}

}

