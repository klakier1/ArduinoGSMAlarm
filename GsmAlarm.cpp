/*********** Includes ***************************************************************/
#include "Arduino.h"
#include "PIRSensor.h"
#include "BGsmShield.h"
#include "Phonebook.h"
#include <string.h>
#include <TimeLib.h>
//#include "IntegerQueue.h"

getExternalTime qwe(){
	return 0;
}

/*********** Constants ***************************************************************/
const int sensor1Pin = 8;
const int sensor2Pin = 9;
const int ledPin = 13;      // the number of the LED pin

/*********** Global Variables ********************************************************/
PIRSensor *sensor1;
PIRSensor *sensor2;
BGsmShield *bgsm;
Phonebook phonebook;

//Handles for most used SMS responses stored in SIM
int smsNumAlaram1;
int smsNumAlaram2;
int smsNumArmed;
int smsNumDisarmed;
int smsNumMemberAdd;
int smsNumMemberDelete;
int smsNumError;
int smsNumFixedMember;
int smsNumStrangerStart;  //jak ktos nie dodany wysle start

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
	if (0
			> (smsNumStrangerStart = bgsm->StoreSMS(
					"NIE JESTES DODANY DO LISTY UZYTKOWNIKOW")))
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
			if (bgsm->SendSMSBegin(num)) {
				//armed or disarmed
				if (sensor1->IsArmed() && sensor2->IsArmed())
					bgsm->SendSMSAttachText("UZBROJONY");
				else
					bgsm->SendSMSAttachText("ROZBROJONY");

				bgsm->SendSMSAttachText("\n");

				//list of members
				for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
					bgsm->SendSMSAttachInt(i);
					bgsm->SendSMSAttachText(") ");
					if (phonebook.phoneNumbers[i] != NULL) {
						bgsm->SendSMSAttachText(phonebook.phoneNumbers[i]);
					} else {
						bgsm->SendSMSAttachText("-------");
					}
					bgsm->SendSMSAttachText("\n");
				}
				//length of queue
				bgsm->SendSMSAttachText("QUEUE:");
				bgsm->SendSMSAttachInt(bgsm->_smsQueue.Size());
				bgsm->SendSMSAttachText("\n");
				//send
				bgsm->SendSMSEnd();
			}
		} else if (memcmp("Start", msg, 5) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumArmed);
			sensor1->Arm(true);
			sensor2->Arm(true);

		} else if (memcmp("Stop", msg, 4) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumDisarmed);
			sensor1->Arm(false);
			sensor2->Arm(false);

		} else if (memcmp("AddNum ", msg, 7) == 0) {
			//bgsm->SendSMSFromStorage(num, smsNumMemberAdd);
			int addResult = 0;

			if (msg_len >= 16) {
				bool isCorrectNumber = true;
				for (int i = 7; i < 16; i++) {
					if (!((*(msg + i) >= '0') && (*(msg + i) <= '9'))) {
						isCorrectNumber = false;
					}
				}

				bgsm->SendSMSBegin(num);

				if (isCorrectNumber) {
					if ((addResult = phonebook.Add(msg + 7)) != 0) {
						bgsm->SendSMSAttachText("ADDNUM PHONEBOOK RESULT ");
						bgsm->SendSMSAttachInt(addResult);
					} else {
						bgsm->SendSMSAttachText(
								"ADDNUM PHONEBOOK RESULT ERROR");
					}
				} else {
					bgsm->SendSMSAttachText("ADDNUM INCORRECT NUM");
				}
			} else {
				bgsm->SendSMSAttachText("ADDNUM WRONG LENGTH");
			}

			bgsm->SendSMSEnd();

			//send feedback message
			if (addResult) {
				Serial.print(phonebook.phoneNumbers[addResult]);
				if (bgsm->SendSMSBegin(phonebook.phoneNumbers[addResult])) {
					bgsm->SendSMSAttachText("DODANY PRZEZ ");
					bgsm->SendSMSAttachText(num);
					bgsm->SendSMSEnd();
				} else {
					Serial.println("Send feedback message error");
				}
			}

		} else if (memcmp("DelNum ", msg, 7) == 0) {

			bool deleteResult = false;
			char* storeNumberToDelete = (char*) malloc(
			PHONEBOOK_NUMBER_LEN + 1);
			storeNumberToDelete[PHONEBOOK_NUMBER_LEN] = NULL;

			bgsm->SendSMSBegin(num);
			bgsm->SendSMSAttachText("DELNUM ");

			int numToDelete = atoi(msg + 7);
			if (numToDelete >= PHONEBOOK_FIXED_NUMBERS_COUNT
					&& numToDelete < PHONEBOOK_CAPACITY) {
				if (phonebook.phoneNumbers[numToDelete] != NULL) {

					bgsm->SendSMSAttachText(
							phonebook.phoneNumbers[numToDelete]);
					strcpy(storeNumberToDelete,
							phonebook.phoneNumbers[numToDelete]);

					if (deleteResult = phonebook.Delete(numToDelete)) { // @suppress("Assignment in condition")
						bgsm->SendSMSAttachText(" OK");
					} else {
						bgsm->SendSMSAttachText(" ERROR");
					}

				} else {
					bgsm->SendSMSAttachText("POSITION EMPTY");
				}
			} else {
				bgsm->SendSMSAttachText("POSITION ERROR");
			}

			bgsm->SendSMSEnd();

			//send feedback message
			if (deleteResult) {
				Serial.print(storeNumberToDelete);
				if (bgsm->SendSMSBegin(storeNumberToDelete)) {
					bgsm->SendSMSAttachText("USUNIETY PRZEZ ");
					bgsm->SendSMSAttachText(num);
					bgsm->SendSMSEnd();
				} else {
					Serial.println("Send feedback message error");
				}
			}

			free(storeNumberToDelete);

		} else {
			return false;
		}

		break;
	case MEMBER: {

		if (memcmp("Dodaj", msg, 5) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumMemberAdd);

		} else if (memcmp("Usun", msg, 4) == 0) {
			if (phonebook.Delete(num)) {
				bgsm->SendSMSFromStorage(num, smsNumMemberDelete);

				bgsm->SendSMSBegin(bgsm->GetDebugNumber());
				bgsm->SendSMSAttachText("DELETE OK ");
				bgsm->SendSMSAttachText(num);
				bgsm->SendSMSEnd();
			} else {
				bgsm->SendSMSFromStorage(num, smsNumError);

				bgsm->SendSMSBegin(bgsm->GetDebugNumber());
				bgsm->SendSMSAttachText("DELETE ERROR ");
				bgsm->SendSMSAttachText(num);
				bgsm->SendSMSEnd();
			}

		} else if (memcmp("Start", msg, 5) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumArmed);
			sensor1->Arm(true);
			sensor2->Arm(true);

			bgsm->SendSMSBegin(bgsm->GetDebugNumber());
			bgsm->SendSMSAttachText("ARMED BY ");
			bgsm->SendSMSAttachText(num);
			bgsm->SendSMSEnd();

		} else if (memcmp("Stop", msg, 4) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumDisarmed);
			sensor1->Arm(false);
			sensor2->Arm(false);

			bgsm->SendSMSBegin(bgsm->GetDebugNumber());
			bgsm->SendSMSAttachText("DISARMED BY ");
			bgsm->SendSMSAttachText(num);
			bgsm->SendSMSEnd();

		} else if (memcmp("Status", msg, 6) == 0) {
			if (bgsm->SendSMSBegin(num)) {
				//armed or disarmed
				if (sensor1->IsArmed() && sensor2->IsArmed())
					bgsm->SendSMSAttachText("UZBROJONY");
				else
					bgsm->SendSMSAttachText("ROZBROJONY");

				bgsm->SendSMSAttachText("\n");

				//list of members
				for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
					bgsm->SendSMSAttachInt(i);
					bgsm->SendSMSAttachText(") ");
					if (phonebook.phoneNumbers[i] != NULL) {
						bgsm->SendSMSAttachText(phonebook.phoneNumbers[i]);
					} else {
						bgsm->SendSMSAttachText("-------");
					}
					bgsm->SendSMSAttachText("\n");
				}
				//send
				bgsm->SendSMSEnd();
			}

		} else {
			return false;
		}

		break;
	}
	case STRANGER: {

		if (memcmp("Dodaj", msg, 5) == 0) {
			if (phonebook.Add(num)) {
				bgsm->SendSMSFromStorage(num, smsNumMemberAdd);

				bgsm->SendSMSBegin(bgsm->GetDebugNumber());
				bgsm->SendSMSAttachText("ADD REQ OK ");
				bgsm->SendSMSAttachText(num);
				bgsm->SendSMSEnd();
			} else {
				bgsm->SendSMSFromStorage(num, smsNumError);

				bgsm->SendSMSBegin(bgsm->GetDebugNumber());
				bgsm->SendSMSAttachText("ADD REQ ERROR ");
				bgsm->SendSMSAttachText(num);
				bgsm->SendSMSEnd();
			}

		} else if (memcmp("Start", msg, 5) == 0 || memcmp("Stop", msg, 4) == 0
				|| memcmp("Status", msg, 6) == 0) {
			bgsm->SendSMSFromStorage(num, smsNumStrangerStart);

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

