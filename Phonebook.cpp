/*
 * Phonebook.cpp
 *
 *  Created on: 29 mar 2020
 *      Author: lukas
 */

#include "Phonebook.h"
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>

Phonebook::Phonebook() {
	phoneNumbers[0] = (char*) "668195261";
	for (int i = PHONEBOOK_FIXED_NUMBERS_COUNT; i < PHONEBOOK_CAPACITY; i++) {
		phoneNumbers[i] = NULL;
	}
}

Phonebook::~Phonebook() {

}

/*
 * Add phone number
 * return:
 * position in phonebook or 0
 */
int Phonebook::Add(const char *number) {

	size_t len = strlen(number);
	if (len != 12 && len != 9)
		return false;
	if (len == 12)
		number += 3;

	for (int i = PHONEBOOK_FIXED_NUMBERS_COUNT; i < PHONEBOOK_CAPACITY; i++) {
		if (phoneNumbers[i] == NULL) {
			phoneNumbers[i] = (char*) malloc(PHONEBOOK_NUMBER_LEN + 1);
			phoneNumbers[i][PHONEBOOK_NUMBER_LEN] = NULL; // last char NULL for string
			memcpy(phoneNumbers[i], number, PHONEBOOK_NUMBER_LEN);
			return i;
		}
	}
	return 0;
}

bool Phonebook::Delete(int pos) {
	if (pos >= PHONEBOOK_FIXED_NUMBERS_COUNT && pos < PHONEBOOK_CAPACITY) {
		if (phoneNumbers[pos] != NULL) {
			free(phoneNumbers[pos]);
			phoneNumbers[pos] = NULL;
		}
		return true;
	}
	return false;
}

bool Phonebook::Delete(const char *number) {

	int pos = GetPosition(number);

	return Delete(pos);
}

bool Phonebook::HasFreeSpace() {
	for (int i = PHONEBOOK_FIXED_NUMBERS_COUNT; i < PHONEBOOK_CAPACITY; i++) {
		if (phoneNumbers[i] == NULL)
			return true;
	}
	return false;
}

/*
 * return:
 *
 */
number_status Phonebook::Contains(const char *number) {

	size_t len = strlen(number);

	if (len != 12 && len != 9)
		return number_status::ERROR;
	if (len == 12)
		number += 3;

	for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
		if (phoneNumbers[i] != NULL)
			if (strcmp(number, phoneNumbers[i]) == 0) {
				if (i < PHONEBOOK_FIXED_NUMBERS_COUNT)
					return number_status::FIXED_MEMBER;
				else
					return number_status::MEMBER;

			}
	}
	return number_status::STRANGER;
}

void Phonebook::Print(void) {
	Serial.println(" *** BAZA ***");
	for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
		if (phoneNumbers[i] != NULL)
			Serial.println(phoneNumbers[i]);
	}
	Serial.println(" ************");
}

int Phonebook::GetPosition(const char *number) {
	size_t len = strlen(number);

	if (len != 12 && len != 9)
		return number_status::ERROR;
	if (len == 12)
		number += 3;

	for (int i = 0; i < PHONEBOOK_CAPACITY; i++) {
		if (phoneNumbers[i] != NULL)
			if (strcmp(number, phoneNumbers[i]) == 0) {
				return i;
			}
	}
	return -1;
}

