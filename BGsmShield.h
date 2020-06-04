/*
 * BGsmShield.h
 *
 *  Created on: 24 mar 2020
 *      Author: lukas
 */

#ifndef BGSMSHIELD_H_
#define BGSMSHIELD_H_

//#define DEBUG_ON

#include <Arduino.h>
#include "SoftwareSerial.h"
#include "IntegerQueue.h"
#include "TimeLib.h"

#define GSM_RX_PIN  		2	//Komunikacja poprzez interfejs szeregowy.
#define GSM_TX_PIN  		3	//Komunikacja poprzez interfejs szeregowy.
#define GSM_PWRKEY_PIN  	7	//Kontrola napiêcia zasilania uk³adu GSM (On/Off).
#define GSM_STATUS_PIN  	5	//Wyprowadzenie statusowe.
#define GSM_RESET_PIN  		6	//Reset uk³adu GSM.
#define GSM_SERIAL_SPEED	57600
#define COMM_BUF_LEN        200

enum comm_line_status_enum {
	// CLS like CommunicationLineStatus
	CLS_FREE,   // line is free - not used by the communication and can be used
	CLS_ATCMD,  // line is used by AT commands, includes also time for response
	CLS_DATA,  // for the future - line is used in the CSD or GPRS communication
	CLS_LAST_ITEM
};

enum rx_state_enum {
	RX_NOT_STARTED, 			//
	RX_ALREADY_STARTED, 		//
	RX_NOT_FINISHED,     		// not finished yet
	RX_FINISHED,              	// finished, some character was received
	RX_FINISHED_STR_RECV,     	// finished and expected string received
	RX_FINISHED_STR_NOT_RECV, 	// finished, but expected string not received
	RX_TMOUT_ERR,             	// finished, no character received
								// initial communication tmout occurred

	RX_LAST_ITEM
};

enum at_resp_enum {
	AT_RESP_ERR_NO_RESP = -1,   // nothing received
	AT_RESP_ERR_DIF_RESP = 0,  // response_string is different from the response
	AT_RESP_OK = 1,             // response_string was included in the response

	AT_RESP_LAST_ITEM
};

class BGsmShield {
public:
	BGsmShield();
	virtual ~BGsmShield();
	void SetPowerKeyPin(uint8_t state);
	void SetResetPin(uint8_t state);
	int GetStatusPin();
	uint8_t switchOn();
	uint8_t switchOff();

	char SendATCmdWaitResp(char const *AT_cmd_string, uint16_t start_comm_tmout,
			uint16_t max_interchar_tmout, char const *response_string,
			byte no_of_attempts);
	char SendATCmdWaitResp(const __FlashStringHelper *AT_cmd_string,
			uint16_t start_comm_tmout, uint16_t max_interchar_tmout,
			char const *response_string, byte no_of_attempts);
	byte WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout,
			char const *expected_resp_string);
	byte WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
	byte IsRxFinished(void);
	byte IsStringReceived(char const *compare_string);
	void RxInit(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
	void Loop();
	void ProcessIncomingCommand();
	byte TrimWhiteSpacesFront(byte **ret_buf, byte *ret_len);

	int ReadSms(int no, char **text, size_t *text_len, char **tnumber,
			size_t *tnumber_len);

	int DeleteSms(int no);
	int SendSMS(char *number_str, const char *message_str);
	int SendSMS(char *number_str, const __FlashStringHelper *message_str);
	int SendSMSFromStorage(char *number_str, int smsNum);
	int StoreSMS(const char *message_str);
	int StoreSMS(const __FlashStringHelper *message_str);
	int ProcessSMS(int no);

	//Send SMS in few parts
	int SendSMSBegin(const char *number_str);
	void SendSMSAttachText(char *message_str);
	void SendSMSAttachText(const __FlashStringHelper *message_str);
	void SendSMSAttachInt(int);
	int SendSMSEnd();

	//Make call
	void MakeCall(const char *number_str);
	bool IsCalling();

	//Debug number;
	void SetDebugNumber(const char*);
	void ResetDebugNumber();
	const char* GetDebugNumber();

	//Set Callback
	void SetIncomeSmsCallback(
			bool (*callback)(char *num, size_t num_len, char *msg,
					size_t msg_len));
	void SetIncomeVoiceCallback(void (*callback)(char *num, size_t num_len));

	//Time functions
	static time_t ParseTime(byte *tstr);
	static time_t GetTime(void);
	static BGsmShield *sp_bgsm;

	byte comm_buf[COMM_BUF_LEN + 1]; // communication buffer +1 for 0x00 termination
	byte pending_buf[COMM_BUF_LEN + 1]; // communication buffer +1 for 0x00 termination
	SoftwareSerial _cell;
	IntegerQueue _smsQueue;
private:
	void (*incomeVoiceCallback)(char *num, size_t num_len) = NULL;
	bool (*incomeSmsCallback)(char *num, size_t num_len, char *msg,
			size_t msg_len) = NULL;



	char *_debugNumber;

	byte comm_line_status;

	// global status - bits are used for representation of states
	byte module_status;

	// variables connected with communication buffer

	byte *p_comm_buf;               // pointer to the communication buffer
	volatile byte comm_buf_len;              // num. of characters in the buffer
	byte rx_state;                  // internal state of rx state machine
	uint16_t start_reception_tmout; // max tmout for starting reception
	uint16_t interchar_tmout;       // previous time in msec.
	unsigned long prev_time;        // previous time in msec.

	volatile bool _call_in_prog;
	time_t lastTime = 0;
	bool _waitingCpasResponse = false;
};

#endif /* BGSMSHIELD_H_ */
