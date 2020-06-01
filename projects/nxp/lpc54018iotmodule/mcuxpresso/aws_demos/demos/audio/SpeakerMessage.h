/*
 * SpeakerMessage.h
 *
 *  Created on: Apr 18, 2018
 *      Author: bandken
 */

#ifndef SPEAKERMESSAGE_H_
#define SPEAKERMESSAGE_H_

#include <stdint.h>
#include "adsHeader.h"

#pragma pack (push, 1)
typedef struct {
	ADS_BINARY_HDR_t adsHdr;
	uint64_t index;
	uint8_t data[];
} SpeakerMessage_t;

#endif /* SPEAKERMESSAGE_H_ */
