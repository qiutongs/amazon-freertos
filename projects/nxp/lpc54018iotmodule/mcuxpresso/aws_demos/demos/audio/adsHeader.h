/*
 * adsHeader.h
 *
 *  Created on: Apr 18, 2018
 *      Author: bandken
 */

#ifndef ADSHEADER_H_
#define ADSHEADER_H_

#include <stdint.h>

#pragma pack (push, 1)
typedef struct {
	uint32_t sequence;
	uint8_t type;
	uint8_t reserved;
	uint8_t reserved2;
	uint8_t reserved3;
} ADS_BINARY_HDR_t;
#pragma pack(pop)

#endif /* ADSHEADER_H_ */
