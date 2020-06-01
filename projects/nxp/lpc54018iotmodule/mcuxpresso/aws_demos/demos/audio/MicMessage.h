#pragma once

#include <stdint.h>
#include "adsHeader.h"

#pragma pack (push, 1)
typedef struct {
	ADS_BINARY_HDR_t adsHdr;
	uint64_t index;
	uint8_t data[0x400];
} MicMessage_t;
#pragma pack(pop)

