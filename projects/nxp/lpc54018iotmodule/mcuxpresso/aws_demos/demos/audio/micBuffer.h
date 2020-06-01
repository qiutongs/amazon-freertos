/*
 * micBuffer.h
 *
 *  Created on: Apr 17, 2018
 *      Author: bandken
 */

#ifndef MICBUFFER_H_
#define MICBUFFER_H_

#include <stddef.h>

#include "MicMessage.h"

typedef struct micBuff
{
	struct micBuff *next;
	size_t length;
	MicMessage_t message;
} micBuff_t;

void initMicBuffers();
micBuff_t *allocMicBuffer();
void freeMicBuffer(micBuff_t *micBuff);

#endif /* MICBUFFER_H_ */
