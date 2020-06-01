#pragma once

#include <stddef.h>
#include <stdint.h>


enum
{
	AUDIO_PLAYER_STATE_IDLE,
	AUDIO_PLAYER_STATE_SHOULD_PLAY,
	AUDIO_PLAYER_STATE_PLAYING
};

typedef struct AudioPlayer
{
	int state;
	uint64_t playingIndex;
	uint64_t stopIndex;
	void(*stoppedCallback)();
	void(*playCallback)(void(* playTask)(void *), void *);
} AudioPlayer_t;

void initAudioPlayer(AudioPlayer_t *audioPlayer, void(*stoppedCallback)(), void(*playCallback)(void(*playTask)(void *), void *));
void deinitAudioPlayer(AudioPlayer_t *audioPlayer);
void handleSpeaker(AudioPlayer_t *audioPlayer, uint64_t playIndex, const uint8_t *data, size_t length);
void handlePlay(AudioPlayer_t *audioPlayer);
void handleStop(AudioPlayer_t *audioPlayer, uint64_t stopIndex);
void handleSetVolume(AudioPlayer_t *audioPlayer, uint64_t volume);
void getCurrentAudio(AudioPlayer_t *audioPlayer, volatile size_t *dataSize, volatile uint8_t **data);
