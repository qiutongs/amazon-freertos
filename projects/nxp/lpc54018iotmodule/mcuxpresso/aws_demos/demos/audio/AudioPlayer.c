#include "AudioPlayer.h"

#include "fsl_debug_console.h"
#include <string.h>

#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"



#define RING_BUFFER_SIZE (1024*16)

static size_t g_ringBufferSize=RING_BUFFER_SIZE;
static uint8_t g_buffer[RING_BUFFER_SIZE];
static uint8_t *g_bufferWrite;
static uint8_t *g_bufferRead;

static SemaphoreHandle_t g_ringBufferMutex = NULL;
static StaticSemaphore_t g_ringBufferMutexBuffer;

static void bufferData(const uint8_t* pcmInputBuffer, size_t pcmInputBufferSize)
{
	static int total = 0;
	total += pcmInputBufferSize;
    const uint8_t *bufferEnd = g_buffer + g_ringBufferSize;

//	configPRINTF(("BD waiting for semaphore size: %lu bytes\r\n", pcmInputBufferSize));
//	configPRINTF(("  (total %d)\r\n", total));
	if (xSemaphoreTake(g_ringBufferMutex, portMAX_DELAY))
	{
//		configPRINTF(("BD buffering (all offsets) g_bufferWrite = %x\r\n", g_bufferWrite-g_buffer));
//		configPRINTF(("                        g_bufferRead = %x\r\n",g_bufferRead-g_buffer));

		if (g_bufferWrite + pcmInputBufferSize >= bufferEnd)
		{
			configPRINTF(("wrapping\r\n"));

			size_t firstSegmentSize = bufferEnd - g_bufferWrite;
			memcpy(g_bufferWrite, pcmInputBuffer, firstSegmentSize);
			//configPRINTF(("first seg: %d at %d\r\n", firstSegmentSize, g_bufferHead-g_buffer));

			size_t secondSegmentSize = pcmInputBufferSize - firstSegmentSize;
			memcpy(g_buffer, pcmInputBuffer + firstSegmentSize, secondSegmentSize);
			//configPRINTF(("second seg: %d at %d\r\n", secondSegmentSize, g_buffer - g_buffer));
			g_bufferWrite = g_buffer + secondSegmentSize;
		}
		else {
			memcpy(g_bufferWrite, pcmInputBuffer, pcmInputBufferSize);
			//configPRINTF(("whole: %d at %d\r\n", pcmInputBufferSize, g_bufferHead - g_buffer));
			g_bufferWrite += pcmInputBufferSize;
		}

		xSemaphoreGive(g_ringBufferMutex);
	}
}

static void stop(AudioPlayer_t *audioPlayer)
{
	configPRINTF(("stopping (current play index: %d, current stop index %d)\r\n", audioPlayer->playingIndex, audioPlayer->stopIndex));

	audioPlayer->state = AUDIO_PLAYER_STATE_IDLE;
	if (audioPlayer->stoppedCallback)
	{
		audioPlayer->stoppedCallback();
	}
}

void handleSpeaker(AudioPlayer_t *audioPlayer, uint64_t playIndex, const uint8_t *data, size_t length)
{
//	configPRINTF(("handleSpeaker: bytes %lu\r\n",length));
//	configPRINTF(("             : playIndex %llu\r\n",playIndex));
	size_t truncatedLength;
	if (audioPlayer->state == AUDIO_PLAYER_STATE_PLAYING)
	{
		audioPlayer->playingIndex = playIndex;

		if (audioPlayer->playingIndex < audioPlayer->stopIndex)
		{
			truncatedLength = audioPlayer->playingIndex + length >= audioPlayer->stopIndex ? audioPlayer->stopIndex - audioPlayer->playingIndex : length;

			bufferData(data, truncatedLength);
			audioPlayer->playingIndex += truncatedLength;
		}

		if (audioPlayer->playingIndex >= audioPlayer->stopIndex)
		{
			stop(audioPlayer);
		}
	}
}

extern void playData(size_t dataSize, uint8_t *data);

static void internalPlayData(size_t dataSize, uint8_t *data)
{
	if(dataSize != 0)
	{
		dataSize += (dataSize % 4);

		if(data != g_buffer && (((uint32_t)data) % 4) != 0)
		{
			uint32_t diff = ((uint32_t)data) % 4;
			g_bufferRead += diff;
			data = g_bufferRead;
		}

		playData(dataSize, data);
	}
}

static void playTask(void *userData)
{
	AudioPlayer_t *audioPlayer = userData;
	while (audioPlayer->state == AUDIO_PLAYER_STATE_PLAYING)
	{
		//configPRINTF(("waiting for semaphore\r\n"));
		if (xSemaphoreTake(g_ringBufferMutex, 1))
		{
			//configPRINTF(("waiting for data (all offsets) g_bufferWrite = %x\r\n", g_bufferWrite-g_buffer));
			//configPRINTF(("                               g_bufferRead = %x\r\n",g_bufferRead-g_buffer));
			if (g_bufferRead != g_bufferWrite)
			{
//				configPRINTF(("PLAYING (all offsets) g_bufferWrite = %x\r\n", g_bufferWrite-g_buffer));
//				configPRINTF(("                      g_bufferRead = %x\r\n",g_bufferRead-g_buffer));

				const uint8_t *bufferEnd = g_buffer + g_ringBufferSize;

				if (g_bufferWrite >= g_bufferRead)
				{
					internalPlayData(g_bufferWrite - g_bufferRead, g_bufferRead);
					//configPRINTF(("single  size: %d from %d\r\n", g_bufferHead - g_bufferEnd, g_bufferEnd - g_buffer));
				}
				else
				{
					internalPlayData(bufferEnd - g_bufferRead, g_bufferRead);
					//configPRINTF(("double  size: %d from %d\r\n", bufferEnd - g_bufferEnd, g_bufferEnd - g_buffer));
					internalPlayData(g_bufferWrite - g_buffer,g_buffer);
					//configPRINTF(("double  size: %d from %d\r\n", g_bufferHead - g_buffer, g_buffer - g_buffer));
				}

				g_bufferRead = g_bufferWrite;
			}
			xSemaphoreGive(g_ringBufferMutex);
		}
	}

	configPRINTF(("done playing\r\n"));
	vTaskDelete(NULL); /* Delete this task. */
}

void handlePlay(AudioPlayer_t *audioPlayer)
{
	configPRINTF(("handlePlay (current play index: %lld, current stop index %lld)\r\n", audioPlayer->playingIndex, audioPlayer->stopIndex));
	audioPlayer->state = AUDIO_PLAYER_STATE_PLAYING;
	audioPlayer->stopIndex = (uint64_t)-1LL;
	if (audioPlayer->playCallback)
	{
		audioPlayer->playCallback(playTask, audioPlayer);
	}
    BOARD_DMA_EDMA_Start();
}

void handleStop(AudioPlayer_t *audioPlayer, uint64_t stopIndex)
{
	configPRINTF(("handleStop (current play index: %lld, current stop index %lld  new stop index %lld)\r\n", audioPlayer->playingIndex, audioPlayer->stopIndex, stopIndex));
	audioPlayer->stopIndex = stopIndex;
	if (audioPlayer->stopIndex <= audioPlayer->playingIndex)
	{
		stop(audioPlayer);
	}
}

void handleSetVolume(AudioPlayer_t *audioPlayer, uint64_t volume)
{
	configPRINTF(("handleSetVolume (current play index: %lld, current stop index %lld)\r\n", audioPlayer->playingIndex, audioPlayer->stopIndex));
	(void)audioPlayer;
	(void)volume;
}

void initAudioPlayer(AudioPlayer_t *audioPlayer, void(*stoppedCallback)(), void(*playCallback)(void(*playTask)(void *), void *))
{
	configASSERT((((uint32_t)g_buffer) % 4 == 0));
	g_bufferWrite = g_buffer;
	g_bufferRead = g_buffer;

	g_ringBufferMutex = xSemaphoreCreateMutexStatic(&g_ringBufferMutexBuffer);

	audioPlayer->state = AUDIO_PLAYER_STATE_IDLE;
	audioPlayer->stopIndex = (uint64_t)-1LL;
	audioPlayer->playingIndex = 0;
	audioPlayer->stoppedCallback = stoppedCallback;
	audioPlayer->playCallback = playCallback;
}

void deinitAudioPlayer(AudioPlayer_t *audioPlayer)
{
	(void)audioPlayer;
}

