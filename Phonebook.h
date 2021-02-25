/*
 * Phonebook.h
 *
 *  Created on: 29 mar 2020
 *      Author: lukas
 */

#ifndef PHONEBOOK_H_
#define PHONEBOOK_H_

#define PHONEBOOK_CAPACITY					5
#define PHONEBOOK_NUMBER_LEN 				9
#define PHONEBOOK_FIXED_NUMBERS_COUNT		2

enum number_status {
	ERROR = -1, STRANGER, MEMBER, FIXED_MEMBER
};

class Phonebook {
public:
	Phonebook();
	virtual ~Phonebook();
	int Add(const char *number);
	bool Delete(int pos);
	bool Delete(const char *number);
	bool HasFreeSpace();
	number_status Contains(const char *number);
	void Print(void);
	int GetPosition(const char *number);
	static char* Normalize(char * num);
	char *phoneNumbers[PHONEBOOK_CAPACITY];

};

#endif /* PHONEBOOK_H_ */
