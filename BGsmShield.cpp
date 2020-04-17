/*
 * BGsmShield.cpp
 *
 *  Created on: 24 mar 2020
 *      Author: lukas
 */

#include "BGsmShield.h"

BGsmShield::BGsmShield() :
		_cell(GSM_RX_PIN, GSM_TX_PIN) {
	pinMode(GSM_PWRKEY_PIN, OUTPUT);
	pinMode(GSM_RESET_PIN, OUTPUT);
	SetPowerKeyPin(HIGH);
	SetResetPin(HIGH);
	pinMode(GSM_STATUS_PIN, INPUT);
	_cell.begin(GSM_SERIAL_SPEED);
	delay(3000);	//2s to stabilize
}

BGsmShield::~BGsmShield() {
	_smsQueue = IntegerQueue();
}

void BGsmShield::SetPowerKeyPin(uint8_t state) {
	digitalWrite(GSM_PWRKEY_PIN, state);
}

void BGsmShield::SetResetPin(uint8_t state) {
	digitalWrite(GSM_RESET_PIN, state);
}

int BGsmShield::GetStatusPin() {
	return digitalRead(GSM_STATUS_PIN);
}

uint8_t BGsmShield::switchOn() {
	for (uint8_t i = 1; i <= 3; i++) {
		if (GetStatusPin() == HIGH) {
			return 1;
		} else {
			SetResetPin(HIGH);
			delay(100);
			SetPowerKeyPin(LOW);
			delay(1000);
			SetPowerKeyPin(HIGH);
		}
	}
	return 0;
}

uint8_t BGsmShield::switchOff() {
	for (uint8_t i = 1; i <= 3; i++) {
		if (GetStatusPin() == LOW) {
			return 1;
		} else {
			SetPowerKeyPin(HIGH);
			delay(100);
			SetResetPin(LOW);
			delay(1000);
			SetResetPin(HIGH);
		}
	}
	return 0;
}

void BGsmShield::SetDebugNumber(const char * num){
	_debugNumber = num;
}

void BGsmShield::ResetDebugNumber(){
	_debugNumber = NULL;
}

/**********************************************************
 Method sends AT command and waits for response

 return:
 AT_RESP_ERR_NO_RESP = -1,   // no response received
 AT_RESP_ERR_DIF_RESP = 0,   // response_string is different from the response
 AT_RESP_OK = 1,             // response_string was included in the response
 **********************************************************/
char BGsmShield::SendATCmdWaitResp(char const *AT_cmd_string,
		uint16_t start_comm_tmout, uint16_t max_interchar_tmout,
		char const *response_string, byte no_of_attempts) {
	byte status;
	char ret_val = AT_RESP_ERR_NO_RESP;
	byte i;

	for (i = 0; i < no_of_attempts; i++) {
		// delay 500 msec. before sending next repeated AT command
		// so if we have no_of_attempts=1 tmout will not occurred
		if (i > 0)
			delay(500);

		_cell.println(AT_cmd_string);
		status = WaitResp(start_comm_tmout, max_interchar_tmout);
		if (status == RX_FINISHED) {
			// something was received but what was received?
			// ---------------------------------------------
			if (IsStringReceived(response_string)) {
				ret_val = AT_RESP_OK;
				break;  // response is OK => finish
			} else
				ret_val = AT_RESP_ERR_DIF_RESP;
		} else {
			// nothing was received
			// --------------------
			ret_val = AT_RESP_ERR_NO_RESP;
		}

	}

	return (ret_val);
}

char BGsmShield::SendATCmdWaitResp(const __FlashStringHelper *AT_cmd_string,
		uint16_t start_comm_tmout, uint16_t max_interchar_tmout,
		char const *response_string, byte no_of_attempts) {
	byte status;
	char ret_val = AT_RESP_ERR_NO_RESP;
	byte i;

	for (i = 0; i < no_of_attempts; i++) {
		// delay 500 msec. before sending next repeated AT command
		// so if we have no_of_attempts=1 tmout will not occurred
		if (i > 0)
			delay(500);

		_cell.println(AT_cmd_string);
		status = WaitResp(start_comm_tmout, max_interchar_tmout);
		if (status == RX_FINISHED) {
			// something was received but what was received?
			// ---------------------------------------------
			if (IsStringReceived(response_string)) {
				ret_val = AT_RESP_OK;
				break;  // response is OK => finish
			} else
				ret_val = AT_RESP_ERR_DIF_RESP;
		} else {
			// nothing was received
			// --------------------
			ret_val = AT_RESP_ERR_NO_RESP;
		}

	}

	return (ret_val);
}

byte BGsmShield::WaitResp(uint16_t start_comm_tmout,
		uint16_t max_interchar_tmout, char const *expected_resp_string) {
	byte status;
	byte ret_val;

	RxInit(start_comm_tmout, max_interchar_tmout);
	// wait until response is not finished
	do {
		status = IsRxFinished();
	} while (status == RX_NOT_FINISHED);

	if (status == RX_FINISHED) {
		// something was received but what was received?
		// ---------------------------------------------

		if (IsStringReceived(expected_resp_string)) {
			// expected string was received
			// ----------------------------
			ret_val = RX_FINISHED_STR_RECV;
		} else {
			ret_val = RX_FINISHED_STR_NOT_RECV;
		}
	} else {
		// nothing was received
		// --------------------
		ret_val = RX_TMOUT_ERR;
	}
	return (ret_val);
}

byte BGsmShield::WaitResp(uint16_t start_comm_tmout,
		uint16_t max_interchar_tmout) {
	byte status;

	RxInit(start_comm_tmout, max_interchar_tmout);
	// wait until response is not finished
	do {
		status = IsRxFinished();
	} while (status == RX_NOT_FINISHED);
	return (status);
}

byte BGsmShield::IsRxFinished(void) {
	byte num_of_bytes;
	byte ret_val = RX_NOT_FINISHED;  // default not finished

	// Rx state machine
	// ----------------

	if (rx_state == RX_NOT_STARTED) {
		// Reception is not started yet - check tmout
		if (!_cell.available()) {
			// still no character received => check timeout
			/*
			 #ifdef DEBUG_GSMRX

			 DebugPrint("\r\nDEBUG: reception timeout", 0);
			 Serial.print((unsigned long)(millis() - prev_time));
			 DebugPrint("\r\nDEBUG: start_reception_tmout\r\n", 0);
			 Serial.print(start_reception_tmout);


			 #endif
			 */
			if ((unsigned long) (millis() - prev_time)
					>= start_reception_tmout) {
				// timeout elapsed => GSM module didn't start with response
				// so communication is takes as finished
				/*
				 #ifdef DEBUG_GSMRX
				 DebugPrint("\r\nDEBUG: RECEPTION TIMEOUT", 0);
				 #endif
				 */
				comm_buf[comm_buf_len] = 0x00;
				ret_val = RX_TMOUT_ERR;
			}
		} else {
			// at least one character received => so init inter-character
			// counting process again and go to the next state
			prev_time = millis(); // init tmout for inter-character space
			rx_state = RX_ALREADY_STARTED;
		}
	}

	if (rx_state == RX_ALREADY_STARTED) {
		// Reception already started
		// check new received bytes
		// only in case we have place in the buffer
		num_of_bytes = _cell.available();
		// if there are some received bytes postpone the timeout
		if (num_of_bytes)
			prev_time = millis();

		// read all received bytes
		while (num_of_bytes) {
			num_of_bytes--;
			if (comm_buf_len < COMM_BUF_LEN) {
				// we have still place in the GSM internal comm. buffer =>
				// move available bytes from circular buffer
				// to the rx buffer
				*p_comm_buf = _cell.read();

				p_comm_buf++;
				comm_buf_len++;
				comm_buf[comm_buf_len] = 0x00; // and finish currently received characters
											   // so after each character we have
											   // valid string finished by the 0x00
			} else {
				// comm buffer is full, other incoming characters
				// will be discarded
				// but despite of we have no place for other characters
				// we still must to wait until
				// inter-character tmout is reached

				// so just readout character from circular RS232 buffer
				// to find out when communication id finished(no more characters
				// are received in inter-char timeout)
				_cell.read();
			}
		}

		// finally check the inter-character timeout
		/*
		 #ifdef DEBUG_GSMRX

		 DebugPrint("\r\nDEBUG: intercharacter", 0);
		 <			Serial.print((unsigned long)(millis() - prev_time));
		 DebugPrint("\r\nDEBUG: interchar_tmout\r\n", 0);
		 Serial.print(interchar_tmout);


		 #endif
		 */
		if ((unsigned long) (millis() - prev_time) >= interchar_tmout) {
			// timeout between received character was reached
			// reception is finished
			// ---------------------------------------------

			/*
			 #ifdef DEBUG_GSMRX

			 DebugPrint("\r\nDEBUG: OVER INTER TIMEOUT", 0);
			 #endif
			 */
			comm_buf[comm_buf_len] = 0x00;  // for sure finish string again
											// but it is not necessary
			ret_val = RX_FINISHED;
		}
	}

	return (ret_val);
}

/**********************************************************
 Method checks received bytes

 compare_string - pointer to the string which should be find

 return: 0 - string was NOT received
 1 - string was received
 **********************************************************/
byte BGsmShield::IsStringReceived(char const *compare_string) {
	char *ch;
	byte ret_val = 0;

	if (comm_buf_len) {
		/*
		 #ifdef DEBUG_GSMRX
		 DebugPrint("DEBUG: Compare the string: \r\n", 0);
		 for (int i=0; i<comm_buf_len; i++){
		 Serial.print(byte(comm_buf[i]));
		 }

		 DebugPrint("\r\nDEBUG: with the string: \r\n", 0);
		 Serial.print(compare_string);
		 DebugPrint("\r\n", 0);
		 #endif
		 */
#ifdef DEBUG_ON
		Serial.print("ATT: ");
		Serial.println(compare_string);
		Serial.print("RIC: ");
		Serial.println((char*) comm_buf);
		Serial.print("LEN: ");
		Serial.println(comm_buf_len);
#endif
		ch = strstr((char*) comm_buf, compare_string);
		if (ch != NULL) {
			ret_val = 1;
			/*#ifdef DEBUG_PRINT
			 DebugPrint("\r\nDEBUG: expected string was received\r\n", 0);
			 #endif
			 */
		} else {
			//check for pending SMS
			ch = strstr((char*) comm_buf, "+CMTI:");
			if (ch != NULL) {
				//+CMTI: "SM",1

				char *begin;

				//search for comma
				if ((begin = memchr(ch + 5, ',', 10)) != NULL) {
					int no = atoi((char*) (begin + 1));

					Serial.print("PENDING SMS: ");
					Serial.println(no);
					//add sms to queue
					_smsQueue << no;
				}
			} else {
				Serial.print("WTF : ");
				Serial.print((char*) comm_buf);
			}
			/*#ifdef DEBUG_PRINT
			 DebugPrint("\r\nDEBUG: expected string was NOT received\r\n", 0);
			 #endif
			 */
		}
	} else {
#ifdef DEBUG_ON
		Serial.print("ATT: ");
		Serial.println(compare_string);
		Serial.print("RIC: NO STRING RCVD");
#endif
	}

	return (ret_val);
}

void BGsmShield::RxInit(uint16_t start_comm_tmout,
		uint16_t max_interchar_tmout) {
	rx_state = RX_NOT_STARTED;
	start_reception_tmout = start_comm_tmout;
	interchar_tmout = max_interchar_tmout;
	prev_time = millis();
	comm_buf[0] = 0x00; // end of string
	p_comm_buf = &comm_buf[0];
	comm_buf_len = 0;
	_cell.flush(); // erase rx circular buffer
}

void BGsmShield::Loop() {
	byte status = WaitResp(1, 10);
	if (status != RX_TMOUT_ERR) {
//		Serial.print("S->");
//		Serial.print((char*)comm_buf);
//		Serial.print("<-S");
		ProcessIncomingCommand();
	}
	/* PROCESS PENDING SMS, ON PER LOOP */
	int no = 0; //container for sms number
	if(_smsQueue >> no){		//if pop from queue success, process it
		Serial.print("PROC PENDING SMS: ");
		Serial.println(no);
		if(!ProcessSMS(no))
			_smsQueue << no;	//if SMS no read put it on the end of queue
	}
}

void BGsmShield::ProcessIncomingCommand() {
	byte *p_wbuf = 0;
	byte wbuf_len = 0;
	char ret;
	TrimWhiteSpacesFront(&p_wbuf, &wbuf_len);

	/*RING*/
	if (strstr((char*) p_wbuf, "RING") == p_wbuf) {
		//check who calls
		ret = SendATCmdWaitResp("AT+CLCC", 500, 20, "OK", 1);
		if (AT_RESP_OK == ret) {
			//copy response
			TrimWhiteSpacesFront(&p_wbuf, &wbuf_len);
			char *response = malloc(wbuf_len + 1);
			memcpy(response, p_wbuf, wbuf_len + 1);

			//reject call
			ret = SendATCmdWaitResp("ATH", 500, 20, "OK", 5);
			if (AT_RESP_OK == ret) {
				//print who was calling
				if (strncmp("+CLCC: 1,1,4,0,0,", response, 17) == 0) {
					char *begin;
					char *end;
					//search for begin of phone number
					if ((begin = memchr(response + 17, '"', 15)) != NULL) {
						//search for end of phone number
						if ((end = memchr(begin + 1, '"', 15)) != NULL) {
							size_t len = end - begin - 1;
							*end = NULL;

							//callback
							if (this->incomeVoiceCallback != NULL)
								incomeVoiceCallback(begin + 1, len);
						}
					}
				}
			}
			free(response);
		}
		/*+CMTI:*/
	} else if (strstr((char*) p_wbuf, "+CMTI:") == p_wbuf) {
		//+CMTI: "SM",1

		char *begin;

		//search for comma
		if ((begin = memchr(p_wbuf + 5, ',', 10)) != NULL) {
			int no = atoi((char*) (begin + 1));

			Serial.print("Sms numer: ");
			Serial.println(no);

			ProcessSMS(no);
		}
	}
}

byte BGsmShield::TrimWhiteSpacesFront(byte **ret_buf, byte *ret_len) {
	*ret_buf = comm_buf;
	*ret_len = comm_buf_len;

	//skip \r and \n and ' '
	while (**ret_buf == '\r' || **ret_buf == '\n' || **ret_buf == 0x20) {
		(*ret_buf)++;
		(*ret_len)--;
	}
	return *ret_len;

}

int BGsmShield::ReadSms(int no, char **text, size_t *text_len, char **tnumber,
		size_t *tnumber_len) {
	char *p_work, *p_begin;

	//command to get sms
	_cell.print("AT+CMGR=");
	_cell.println(no);

	//wait for response
	enum at_resp_enum resp = WaitResp(1000, 20, "OK");
	if (RX_FINISHED_STR_RECV != resp) {
		return -1;
	}

	// extract phone number string
	// ---------------------------
	p_work = strchr((char*) (comm_buf), ',');
	p_begin = p_work + 2; // we are on the first phone number character
	p_work = strchr((char*) (p_begin), '"');
	if (p_work != NULL) {
		*p_work = NULL; // end of string
		*tnumber = p_begin;
		*tnumber_len = strlen(*tnumber);
	} else
		return -2;

	// extract message
	// ------------------------------------------------------
	p_work = strchr(p_work + 1, 0x0a);  // find <LF>
	if (p_work != NULL) {
		// next character after <LF> is the first SMS character
		p_begin = ++p_work;  // now we are on the first SMS character

		// find <CR> as the end of SMS string
		p_work = strchr((char*) (p_work), 0x0d);
		if (p_work != NULL) {
			// finish the SMS text string
			// because string must be finished for right behaviour
			// of next strcpy() function
			*p_work = NULL;
			// find out length of the SMS (excluding 0x00 termination character)
			*text_len = strlen(p_begin);
			*text = p_begin;

			//job done
			return 1;
		} else
			return -3;
	} else
		return -4;
}

void BGsmShield::SetIncomeVoiceCallback(
		void (*callback)(char *num, size_t num_len)) {
	this->incomeVoiceCallback = callback;
}

void BGsmShield::SetIncomeSmsCallback(
		bool (*callback)(char *num, size_t num_len, char *msg,
				size_t msg_len)) {
	this->incomeSmsCallback = callback;
}

int BGsmShield::DeleteSms(int no) {
	_cell.print("AT+CMGD=");
	_cell.println(no);

	enum at_resp_enum resp = WaitResp(2000, 500, "OK");
	if (RX_FINISHED_STR_RECV != resp)
		return -1;
	else
		return 1;
}

int BGsmShield::SendSMS(char *number_str, char *message_str) {
	byte i;
	char end[2];
	end[0] = 0x1a;
	end[1] = '\0';

	// try to send SMS 3 times in case there is some problem
	for (i = 0; i < 3; i++) {

		// send  AT+CMGS="number_str"
		_cell.print(F("AT+CMGS=\""));
		_cell.print(number_str);
		_cell.println("\"");

		if (RX_FINISHED_STR_RECV == WaitResp(2000, 10, ">")) {

			// send SMS text
			_cell.print(message_str);
			_cell.println(end);
			if (RX_FINISHED_STR_RECV == WaitResp(7000, 10, "+CMGS"))
				// SMS was send correctly
				return 1;
		}
	}
	return -1;
}

int BGsmShield::SendSMSFromStorage(char *number_str, int smsNum) {
	byte i;
	char end[2];
	end[0] = 0x1a;
	end[1] = '\0';

	// try to send SMS 3 times in case there is some problem
	for (i = 0; i < 3; i++) {

		// send  AT+CMGS="number_str"
		_cell.print(F("AT+CMSS="));
		_cell.print(smsNum);
		_cell.print(",\"");
		_cell.print(number_str);
		_cell.println("\"");

		if (RX_FINISHED_STR_RECV == WaitResp(2000, 10, "OK")) {
			return 1;
		}
	}
	return -1;
}

int BGsmShield::StoreSMS(char *message_str) {
	byte i;
	char end[2];
	end[0] = 0x1a;
	end[1] = '\0';

	// try to store SMS 3 times in case there is some problem
	for (i = 0; i < 3; i++) {

		// store command  AT+CMGW
		_cell.println(F("AT+CMGW"));

		if (RX_FINISHED_STR_RECV == WaitResp(2000, 10, ">")) {

			// store SMS text
			_cell.print(message_str);
			_cell.println(end);
			if (RX_FINISHED_STR_RECV == WaitResp(7000, 10, "OK")) {
				char *p_wbuf = comm_buf;

				while (*(++p_wbuf) > 0x39 || *p_wbuf < 0x30) {
				};
				int no = atoi(p_wbuf - 1);
				return no;
			}
		} else {
			Serial.print("NOK");
			Serial.print((char*) comm_buf);
		}
	}
	return -1;
}

int BGsmShield::SendSMSBegin(char *number_str) {
	// send  AT+CMGS="number_str"
	_cell.print(F("AT+CMGS=\""));
	_cell.print(number_str);
	_cell.println("\"");

	if (RX_FINISHED_STR_RECV == WaitResp(2000, 10, ">"))
		return 1;
	else
		return 0;
}

void BGsmShield::SendSMSAttachText(char *message_str) {
	_cell.print(message_str);
}

void BGsmShield::SendSMSAttachInt(int val) {
	_cell.print(val);
}

int BGsmShield::SendSMSEnd() {
	char end[2];
	end[0] = 0x1a;
	end[1] = '\0';

	_cell.println(end);
	if (RX_FINISHED_STR_RECV == WaitResp(7000, 10, "+CMGS"))
		// SMS was send correctly
		return 1;
	else
		return 0;
}

/*
 * return:
 * 0 - error, SMS not read and not deleted
 * 1 if SMS read and deleted
 */
int BGsmShield::ProcessSMS(int no){
	//pointers to message and phone number
	char *msg, *tel;
	size_t msg_len, tel_len;

	if (ReadSms(no, &msg, &msg_len, &tel, &tel_len) == 1) {
		if (incomeSmsCallback != NULL)
			if(!incomeSmsCallback(tel, tel_len, msg, msg_len)){
				Serial.print("TRY TO FORWARD :");
				Serial.println(_debugNumber);
				//Forward SMS if it isn't processed
				if(_debugNumber != NULL){
					SendSMSFromStorage(_debugNumber, no);
				}
			} else {
				Serial.println("PROCESSING SMS OK");
			}
	} else {
		return 0;
	}

	//delete SMS
	if (!DeleteSms(no)){
		Serial.println("DELTE SMS ERROR");
		return 0;
	}
	return 1;

}
