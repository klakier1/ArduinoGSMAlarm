/*
 * IntLinkedList.cpp
 *
 *  Created on: 3 kwi 2020
 *      Author: lukas
 */

#include "IntegerQueue.h"

IntegerQueue::IntegerQueue() {
}

void IntegerQueue::PushBack(int value) {

	IntegerItem *p_item = new IntegerItem(value);

	if (mp_head == NULL) {
		mp_head = p_item;
	}
	else {
		IntegerItem *p_temp = mp_head;
		while (p_temp->p_next != NULL) {
			p_temp = p_temp->p_next;
		}
		p_temp->p_next = p_item;
	}
}

bool IntegerQueue::PopFront(int& value) {

	if (mp_head == NULL) {
		return false;
	}
	else {
		value = mp_head->value;
		IntegerItem *temp = mp_head->p_next;
		delete mp_head;
		mp_head = temp;
		return true;
	}
}

IntegerQueue IntegerQueue::operator<< (int value) {
	PushBack(value);
	return *this;
}

bool IntegerQueue::operator>>(int& value) {
	return PopFront(value);
}

int IntegerQueue::Size() const{
	if (mp_head == NULL) {
		return 0;
	}
	else {
		int out = 1;
		//temporaty pointer
		IntegerItem *p_temp = mp_head;
		while (p_temp->p_next != NULL) {
			p_temp = p_temp->p_next;
			out++;
		}
		return out;
	}
}

