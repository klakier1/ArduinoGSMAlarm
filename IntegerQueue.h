/*
 * IntLinkedList.h
 *
 *  Created on: 3 kwi 2020
 *      Author: lukas
 */
#include <Arduino.h>

#ifndef INTLINKEDLIST_H_
#define INTLINKEDLIST_H_

struct IntegerItem {
	int value = 0;
	IntegerItem *p_next = NULL;

	IntegerItem(int value) : value(value) {};
};

class IntegerQueue {

public:
	IntegerQueue();
	void PushBack(int value);
	bool PopFront(int& value);
	IntegerQueue operator<< (int value);
	bool operator>>(int& value);
	int Size() const;

private:
	IntegerItem *mp_head = NULL;
};

#endif /* INTLINKEDLIST_H_ */
