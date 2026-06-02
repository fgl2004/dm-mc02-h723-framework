#include "ring_buffer.h"

#include <stddef.h>
#include <string.h>

static void RingBuffer_UpdateHighWatermark(RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return;
    }

    if (rb->used > rb->stats.high_watermark)
    {
        rb->stats.high_watermark = rb->used;
    }
}

void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buffer, uint16_t size)
{
    if ((rb == NULL) || (buffer == NULL) || (size == 0U))
    {
        return;
    }

    rb->buffer = buffer;
    rb->size = size;
    rb->read_index = 0U;
    rb->write_index = 0U;
    rb->used = 0U;

    memset(&rb->stats, 0, sizeof(rb->stats));
}

void RingBuffer_Clear(RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return;
    }

    rb->read_index = 0U;
    rb->write_index = 0U;
    rb->used = 0U;
}

uint8_t RingBuffer_PutByte(RingBuffer_t *rb, uint8_t byte)
{
    if ((rb == NULL) || (rb->buffer == NULL) || (rb->size == 0U))
    {
        return 0U;
    }

    if (rb->used >= rb->size)
    {
        rb->stats.overflow_count++;
        return 0U;
    }

    rb->buffer[rb->write_index] = byte;

    rb->write_index++;
    if (rb->write_index >= rb->size)
    {
        rb->write_index = 0U;
    }

    rb->used++;
    rb->stats.write_bytes++;

    RingBuffer_UpdateHighWatermark(rb);

    return 1U;
}

uint8_t RingBuffer_GetByte(RingBuffer_t *rb, uint8_t *byte)
{
    if ((rb == NULL) || (rb->buffer == NULL) || (byte == NULL) || (rb->size == 0U))
    {
        return 0U;
    }

    if (rb->used == 0U)
    {
        return 0U;
    }

    *byte = rb->buffer[rb->read_index];

    rb->read_index++;
    if (rb->read_index >= rb->size)
    {
        rb->read_index = 0U;
    }

    rb->used--;
    rb->stats.read_bytes++;

    return 1U;
}

uint16_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint16_t len)
{
    uint16_t written = 0U;

    if ((rb == NULL) || (data == NULL) || (len == 0U))
    {
        return 0U;
    }

    while (written < len)
    {
        if (RingBuffer_PutByte(rb, data[written]) == 0U)
        {
            break;
        }

        written++;
    }

    return written;
}

uint16_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t read_len = 0U;

    if ((rb == NULL) || (data == NULL) || (len == 0U))
    {
        return 0U;
    }

    while (read_len < len)
    {
        if (RingBuffer_GetByte(rb, &data[read_len]) == 0U)
        {
            break;
        }

        read_len++;
    }

    return read_len;
}

uint16_t RingBuffer_Available(const RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return 0U;
    }

    return rb->used;
}

uint16_t RingBuffer_Free(const RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return 0U;
    }

    return (uint16_t)(rb->size - rb->used);
}

uint8_t RingBuffer_IsEmpty(const RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return 1U;
    }

    return (rb->used == 0U) ? 1U : 0U;
}

uint8_t RingBuffer_IsFull(const RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return 0U;
    }

    return (rb->used >= rb->size) ? 1U : 0U;
}

const RingBufferStats_t *RingBuffer_GetStats(const RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return NULL;
    }

    return &rb->stats;
}

void RingBuffer_ResetStats(RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return;
    }

    memset(&rb->stats, 0, sizeof(rb->stats));

    RingBuffer_UpdateHighWatermark(rb);
}