/*
 * ring_buffer.h
 *
 *  Created on: Jul 27, 2023
 *      Author: AngelRobotics HW
 */

#ifndef RING_BUFFER_INC_RING_BUFFER_H_
#define RING_BUFFER_INC_RING_BUFFER_H_

#include "stdint.h"
#include "stdbool.h"
#include "string.h"

//#define RingBuf_MaxLength 256


typedef struct
{
	uint32_t head;
	uint32_t tail;
	uint32_t length;

	uint8_t* Buffer;
}RingBufferStruct;


bool RingBufferCreate(RingBufferStruct* RingBuf, uint8_t* buff, uint32_t length);
bool RingBufferPush(RingBufferStruct* RingBuf, uint8_t *data, uint32_t length);
bool RingBufferPop(RingBufferStruct* RingBuf, uint8_t *data, uint32_t length);
bool RingBufferFlush(RingBufferStruct* RingBuf);
bool RingBufferIsEmpty(RingBufferStruct* RingBuf);
bool RingBufferIsFull(RingBufferStruct* RingBuf);
bool RingBufferIsAvailable(RingBufferStruct* RingBuf);

#endif /* RING_BUFFER_INC_RING_BUFFER_H_ */
