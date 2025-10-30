/*
 * ring_buffer.c
 *
 *  Created on: 2023. 7. 12.
 *      Author: AngelRobotics HW
 */


#include "ring_buffer.h"

bool RingBufferCreate(RingBufferStruct* RingBuf, uint8_t* buff, uint32_t length)
{
    RingBuf->head   = 0;
    RingBuf->tail   = 0;
    RingBuf->Buffer = buff; 
    RingBuf->length = length;

    return true;
}

bool RingBufferPush(RingBufferStruct* RingBuf, uint8_t *data, uint32_t length)
{
    bool ret= true;
    uint32_t index=0;
    uint32_t next_index;

    for (; index<length; index++)
    {
        next_index = (RingBuf->head + 1) % RingBuf->length;

        if (next_index != RingBuf->tail)
        {
            if (RingBuf->Buffer != NULL)
            {
                RingBuf->Buffer[RingBuf->head] = data[index];
            }
            RingBuf->head = next_index;
        }
        else
        {
            ret = false;
            break;
        }
    }

    return ret;
}

bool RingBufferPop(RingBufferStruct* RingBuf, uint8_t *data, uint32_t length)
{
    bool ret = true;
    uint32_t index = 0;

    if (RingBuf->Buffer != NULL && length != 0)
    {
        for (; index < length; index++)
            data[index] = RingBuf->Buffer[(RingBuf->tail + index) % RingBuf->length];
    }
    else
    {
        ret = false;
    }

    if (RingBuf->tail != RingBuf->head)
    {
        RingBuf->tail = (RingBuf->tail + length) % RingBuf->length;
    }
    else
    {
        ret = false;
    }
    return ret;
}

bool RingBufferFlush(RingBufferStruct* RingBuf)
{
    
    RingBuf->head=0;
    RingBuf->tail=0;

    return true;
}

bool RingBufferIsFull(RingBufferStruct* RingBuf)
{
    if(!(RingBuf->head % RingBuf->length) || !(RingBuf->tail % RingBuf->length))
        return true;
    
    return false;
}


bool RingBufferIsEmpty(RingBufferStruct* RingBuf)
{
    
    if(RingBuf->head == RingBuf->tail)
        return true;

    return false;
}

bool RingBufferIsAvailable(RingBufferStruct* RingBuf)
{
    if((RingBuf->head != RingBuf->tail) && ((RingBuf->head - RingBuf->tail) % RingBuf->length != 0))
        return true;

    return false;
}
