/*********** Includes ***************************************************************/
#include "Arduino.h"
#include "PIRSensor.h"
#include "BGsmShield.h"
#include "Phonebook.h"
#include <string.h>
#include <TimeLib.h>
//#include "IntegerQueue.h"

/*********** Macros and defines ******************************************************/
#define DEBUG_ALARM 	0
#define DEBUG_SMS 		0

#if DEBUG_ALARM
#define PRINT_DEBUG_ALARM(x)  Serial.print(x);
#define PRINTLN_DEBUG_ALARM(x)  Serial.println(x);
#else
#define PRINT_DEBUG_ALARM(x)
#define PRINTLN_DEBUG_ALARM(x)
#endif

#if DEBUG_SMS
#define PRINT_SMS_ALARM(x)  Serial.print(x);
#define PRINTLN_SMS_ALARM(x)  Serial.println(x);
#else
#define PRINT_SMS_ALARM(x)
#define PRINTLN_SMS_ALARM(x)
#endif

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
void errorHandler(const __FlashStringHelper *msg = NULL);
void armAlarm(bool arm, char* num);

/*********** User Code **************************************************************/

void setup() {
	Serial.begin(57600);

	sensor1 = new PIRSensor(sensor1Pin, 2.5, sensorCallback);
	sensor2 = new PIRSensor(sensor2Pin, 3.0, sensorCallback);
	bgsm = new BGsmShield();
	bgsm->SetIncomeVoiceCallback(incomingVoiceCallback);
	bgsm->SetIncomeSmsCallback(incomingSmsCallback);

	Serial.print(F("Init GSM... "));

	if (!bgsm->switchOn())
		errorHandler(F("Uklad GSM nie wlacza sie"));

	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("AT", 2000, 10, "OK", 3))
		errorHandler(F("Brak odpowiedzi BGSM"));

	//Wy³acz echo
	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("ATE0", 2000, 10, "OK", 3))
		errorHandler(F("Echo off error"));

	//Wpisz PIN
//	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("AT+CPIN=\"5007\"", 2000, 1000, "OK", 1))
//		errorHandler(F("Blad PIN NOK");

	//Tryb tekstowy
	if (AT_RESP_OK != bgsm->SendATCmdWaitResp("AT+CMGF=1", 2000, 10, "OK", 1))
		errorHandler(F("Text mode error"));

	delay(1000);

	//Usun wszystkie SMS
	if (AT_RESP_OK
			!= bgsm->SendATCmdWaitResp("AT+CMGD=1,4", 2000, 10, "OK", 10))
		errorHandler(F("Del sms error"));

	Serial.println(F("Done"));

	Serial.print(F("StoreSMS... "));

	if (0 > (smsNumAlaram1 = bgsm->StoreSMS(F("!  ALARM 1  !"))))
		errorHandler();
	if (0 > (smsNumAlaram2 = bgsm->StoreSMS(F("!  ALARM 2  !"))))
		errorHandler();
	if (0 > (smsNumArmed = bgsm->StoreSMS(F("UZBROJONY"))))
		errorHandler();
	if (0 > (smsNumDisarmed = bgsm->StoreSMS(F("ROZBROJONY"))))
		errorHandler();
	if (0 > (smsNumMemberAdd = bgsm->StoreSMS(F("DODANO"))))
		errorHandler();
	if (0 > (smsNumMemberDelete = bgsm->StoreSMS(F("USUNIETO"))))
		errorHandler();
	if (0 > (smsNumError = bgsm->StoreSMS(F("BLAD"))))
		errorHandler();
	if (0
			> (smsNumFixedMember = bgsm->StoreSMS(
					F("JESTES STALYM UZYTKOWNIKIEM"))))
		errorHandler();
	if (0
			> (smsNumStrangerStart = bgsm->StoreSMS(
					F("NIE JESTES DODANY DO LISTY UZYTKOWNIKOW"))))
		errorHandler();

	Serial.println(F("Done"));

	bgsm->SetDebugNumber(phonebook.phoneNumbers[0]);

	BGsmShield::sp_bgsm = bgsm;
	setSyncProvider(BGsmShield::GetTime);
	//Sync interval to 1 hour
	setSyncInterval(3600);

	bgsm->SendSMSBegin(bgsm->GetDebugNumber());
	bgsm->SendSMSAttachText(F("START URZADZENIA"));
	bgsm->SendSMSEnd();

}

void loop() {
	sensor1->Loop();
	sensor2->Loop();
	bgsm->Loop();
}

void sensorCallback(AlarmEvent event, PIRSensor *sensor, void *args) {
	switch (event) {
	case DetectionStart: {
		PRINT_DEBUG_ALARM(F("Detect On "));PRINTLN_DEBUG_ALARM(sensor->pinNumber);
		break;
	}
	case DetectionStop: {
		double *time = args;
		PRINT_DEBUG_ALARM(F("Detect Off "));PRINT_DEBUG_ALARM(*time);PRINT_DEBUG_ALARM(F("s  "));PRINTLN_DEBUG_ALARM(sensor->pinNumber);
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

		//Make Call
		tmElements_t te;
		breakTime(now(), te);
		//Serial.println(te.Hour);
		//pomiedzy 20 a 6 dzwon
		if (!bgsm->IsCalling()) {
			if (te.Hour >= 20 || te.Hour < 6) {
				bgsm->MakeCall("791529222");
			}
		}
		//Debug Msg
		PRINT_DEBUG_ALARM(F("Alarm "));PRINTLN_DEBUG_ALARM(sensor->pinNumber);
		break;
	}
	case AlarmStartNotArmed: {
		PRINT_DEBUG_ALARM(F("NOT ARMED Alarm "));PRINTLN_DEBUG_ALARM(sensor->pinNumber);
		break;
	}
	}

}

void incomingVoiceCallback(char *num, size_t num_len) {
	Serial.print(F("Dzwonil: "));
	Serial.println(num);
	bool test = phonebook.Contains(num);
	if (test)
		Serial.println(F("Jest w bazie"));
	else
		Serial.println(F("BRAK w bazie"));
}

bool incomingSmsCallback(char *num, size_t num_len, char *msg, size_t msg_len) {
	Serial.print(F("Sms od: "));
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
					bgsm->SendSMSAttachText(F("UZBROJONY"));
				else
					bgsm->SendSMSAttachText(F("ROZBROJONY"));

				bgsm->SendSMSAttachText(F("\n"));

				//list of members
				for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
					bgsm->SendSMSAttachInt(i);
					bgsm->SendSMSAttachText(F(") "));
					if (phonebook.phoneNumbers[i] != NULL) {
						bgsm->SendSMSAttachText(phonebook.phoneNumbers[i]);
					} else {
						bgsm->SendSMSAttachText(F("-------"));
					}
					bgsm->SendSMSAttachText(F("\n"));
				}
				//length of queue
				bgsm->SendSMSAttachText(F("QUEUE:"));
				bgsm->SendSMSAttachInt(bgsm->_smsQueue.Size());
				bgsm->SendSMSAttachText(F("\n"));
				//send
				bgsm->SendSMSEnd();
			}
		} else if (memcmp("Start", msg, 5) == 0) {
			armAlarm(true, num);

		} else if (memcmp("Stop", msg, 4) == 0) {
			armAlarm(false, num);

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
						bgsm->SendSMSAttachText(F("ADDNUM PHONEBOOK RESULT "));
						bgsm->SendSMSAttachInt(addResult);
					} else {
						bgsm->SendSMSAttachText(
								F("ADDNUM PHONEBOOK RESULT ERROR"));
					}
				} else {
					bgsm->SendSMSAttachText(F("ADDNUM INCORRECT NUM"));
				}
			} else {
				bgsm->SendSMSAttachText(F("ADDNUM WRONG LENGTH"));
			}

			bgsm->SendSMSEnd();

			//send feedback message
			if (addResult) {
				Serial.print(phonebook.phoneNumbers[addResult]);
				if (bgsm->SendSMSBegin(phonebook.phoneNumbers[addResult])) {
					bgsm->SendSMSAttachText(F("DODANY PRZEZ "));
					bgsm->SendSMSAttachText(num);
					bgsm->SendSMSEnd();
				} else {
					Serial.println(F("Send feedback message error"));
				}
			}

		} else if (memcmp("DelNum ", msg, 7) == 0) {

			bool deleteResult = false;
			char *storeNumberToDelete = (char*) malloc(
			PHONEBOOK_NUMBER_LEN + 1);
			storeNumberToDelete[PHONEBOOK_NUMBER_LEN] = NULL;

			bgsm->SendSMSBegin(num);
			bgsm->SendSMSAttachText(F("DELNUM "));

			int numToDelete = atoi(msg + 7);
			if (numToDelete >= PHONEBOOK_FIXED_NUMBERS_COUNT
					&& numToDelete < PHONEBOOK_CAPACITY) {
				if (phonebook.phoneNumbers[numToDelete] != NULL) {

					bgsm->SendSMSAttachText(
							phonebook.phoneNumbers[numToDelete]);
					strcpy(storeNumberToDelete,
							phonebook.phoneNumbers[numToDelete]);

					if (deleteResult = phonebook.Delete(numToDelete)) { // @suppress("Assignment in condition")
						bgsm->SendSMSAttachText(F(" OK"));
					} else {
						bgsm->SendSMSAttachText(F(" ERROR"));
					}

				} else {
					bgsm->SendSMSAttachText(F("POSITION EMPTY"));
				}
			} else {
				bgsm->SendSMSAttachText(F("POSITION ERROR"));
			}

			bgsm->SendSMSEnd();

			//send feedback message
			if (deleteResult) {
				Serial.print(storeNumberToDelete);
				if (bgsm->SendSMSBegin(storeNumberToDelete)) {
					bgsm->SendSMSAttachText(F("USUNIETY PRZEZ "));
					bgsm->SendSMSAttachText(num);
					bgsm->SendSMSEnd();
				} else {
					Serial.println(F("Send feedback message error"));
				}
			}

			free(storeNumberToDelete);

		} else if (memcmp("Time", msg, 4) == 0) {
			tmElements_t te;
			breakTime(now(), te);
			bgsm->SendSMSBegin(bgsm->GetDebugNumber());
			bgsm->SendSMSAttachInt(te.Day);
			bgsm->SendSMSAttachText(F("/"));
			bgsm->SendSMSAttachInt(te.Month);
			bgsm->SendSMSAttachText(F("/"));
			bgsm->SendSMSAttachInt(tmYearToCalendar(te.Year));
			bgsm->SendSMSAttachText(F(" "));
			bgsm->SendSMSAttachInt(te.Hour);
			bgsm->SendSMSAttachText(F(":"));
			bgsm->SendSMSAttachInt(te.Minute);
			bgsm->SendSMSAttachText(F(":"));
			bgsm->SendSMSAttachInt(te.Second);
			bgsm->SendSMSEnd();
		} else if (memcmp("Call", msg, 4) == 0) {

			bgsm->MakeCall(bgsm->GetDebugNumber());

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
				bgsm->SendSMSAttachText(F("DELETE OK "));
				bgsm->SendSMSAttachText(num);
				bgsm->SendSMSEnd();
			} else {
				bgsm->SendSMSFromStorage(num, smsNumError);

				bgsm->SendSMSBegin(bgsm->GetDebugNumber());
				bgsm->SendSMSAttachText(F("DELETE ERROR "));
				bgsm->SendSMSAttachText(num);
				bgsm->SendSMSEnd();
			}

		} else if (memcmp("Start", msg, 5) == 0) {
			armAlarm(true, num);

		} else if (memcmp("Stop", msg, 4) == 0) {
			armAlarm(false, num);

		} else if (memcmp("Status", msg, 6) == 0) {
			if (bgsm->SendSMSBegin(num)) {
				//armed or disarmed
				if (sensor1->IsArmed() && sensor2->IsArmed())
					bgsm->SendSMSAttachText(F("UZBROJONY"));
				else
					bgsm->SendSMSAttachText(F("ROZBROJONY"));

				bgsm->SendSMSAttachText(F("\n"));

				//list of members
				for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
					bgsm->SendSMSAttachInt(i);
					bgsm->SendSMSAttachText(F(") "));
					if (phonebook.phoneNumbers[i] != NULL) {
						bgsm->SendSMSAttachText(phonebook.phoneNumbers[i]);
					} else {
						bgsm->SendSMSAttachText(F("-------"));
					}
					bgsm->SendSMSAttachText(F("\n"));
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
				bgsm->SendSMSAttachText(F("ADD REQ OK "));
				bgsm->SendSMSAttachText(num);
				bgsm->SendSMSEnd();
			} else {
				bgsm->SendSMSFromStorage(num, smsNumError);

				bgsm->SendSMSBegin(bgsm->GetDebugNumber());
				bgsm->SendSMSAttachText(F("ADD REQ ERROR "));
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

	//phonebook.Print();
	return true;

}

void armAlarm(bool arm, char* num) {

	sensor1->Arm(arm);
	sensor2->Arm(arm);

	//wyslij sms do uzbrajac¹cego
	if (arm)
		bgsm->SendSMSFromStorage(num, smsNumArmed);
	else
		bgsm->SendSMSFromStorage(num, smsNumDisarmed);

	//wyslij sms do pozosta³ych
	for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
		char *number = phonebook.phoneNumbers[i];
		if (number != NULL && strcmp(number, Phonebook::Normalize(num))) {
			bgsm->SendSMSBegin(number);
			if (arm)
				bgsm->SendSMSAttachText(F("UZBROJONY PRZEZ "));
			else
				bgsm->SendSMSAttachText(F("ROZBROJONY PRZEZ "));
			bgsm->SendSMSAttachText(num);
			bgsm->SendSMSEnd();
		}
	}
}

void errorHandler(const __FlashStringHelper *msg) {
	while (1) {
		if (msg)
			Serial.println(msg);
		else
			Serial.println(F("Unknown error"));
		digitalRead(ledPin) ?
				digitalWrite(ledPin, LOW) : digitalWrite(ledPin, HIGH);
		delay(1000);
	}

}

