/*
 * micBuffer.c
 *
 *  Created on: Apr 17, 2018
 *      Author: bandken
 */

#include "micBuffer.h"

#include <stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"

static micBuff_t *g_currentMicBufferHead = NULL;
static micBuff_t g_micBuffers[2];
static StaticSemaphore_t g_micBufferMutexBuffer;
static SemaphoreHandle_t g_micBufferMutex;

void initMicBuffers() {
	g_micBufferMutex = xSemaphoreCreateMutexStatic(&g_micBufferMutexBuffer);

	if (xSemaphoreTake(g_micBufferMutex, portMAX_DELAY)) {
		for (int i = 0; i < sizeof(g_micBuffers)/sizeof(g_micBuffers[0]); ++i) {
			memset(g_micBuffers[i].message.data, 0xbb,
					sizeof(g_micBuffers[i].message.data));
			g_micBuffers[i].length = 0;
			g_micBuffers[i].next = g_currentMicBufferHead;
			g_currentMicBufferHead = &(g_micBuffers[i]);
		}
		xSemaphoreGive(g_micBufferMutex);
	}
}

micBuff_t *allocMicBuffer() {
	micBuff_t *mic_buffer = NULL;

	if (xSemaphoreTake(g_micBufferMutex, portMAX_DELAY)) {
		if (!g_currentMicBufferHead) {
			configPRINTF(("allocMicBuffer - all buffers are in use\r\n"));
			while(1)
				;
			return NULL;
		}

		mic_buffer = g_currentMicBufferHead;
		g_currentMicBufferHead = mic_buffer->next;
		mic_buffer->next = NULL;
		xSemaphoreGive(g_micBufferMutex);
	}
	return mic_buffer;
}

void freeMicBuffer(micBuff_t *micBuff) {
	if (xSemaphoreTake(g_micBufferMutex, portMAX_DELAY)) {
		configASSERT(micBuff);

		micBuff_t *workingMicBuff = micBuff;
		while(workingMicBuff->next)
		{
			workingMicBuff = workingMicBuff->next;
		}

		workingMicBuff->next = g_currentMicBufferHead;
		g_currentMicBufferHead = micBuff;
		xSemaphoreGive(g_micBufferMutex);
	}
}
