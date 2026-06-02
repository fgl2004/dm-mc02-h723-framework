#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
    uint32_t write_bytes;
    uint32_t read_bytes;
    uint32_t overflow_count;
    uint16_t high_watermark;
} RingBufferStats_t;

typedef struct
{
    uint8_t *buffer;
    uint16_t size;

    uint16_t read_index;
    uint16_t write_index;
    uint16_t used;

    RingBufferStats_t stats;
} RingBuffer_t;

void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buffer, uint16_t size);
void RingBuffer_Clear(RingBuffer_t *rb);

uint16_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint16_t len);
uint16_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data, uint16_t len);

uint8_t RingBuffer_PutByte(RingBuffer_t *rb, uint8_t byte);
uint8_t RingBuffer_GetByte(RingBuffer_t *rb, uint8_t *byte);

uint16_t RingBuffer_Available(const RingBuffer_t *rb);
uint16_t RingBuffer_Free(const RingBuffer_t *rb);
uint8_t RingBuffer_IsEmpty(const RingBuffer_t *rb);
uint8_t RingBuffer_IsFull(const RingBuffer_t *rb);

const RingBufferStats_t *RingBuffer_GetStats(const RingBuffer_t *rb);
void RingBuffer_ResetStats(RingBuffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */