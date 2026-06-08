#ifndef STREAM_MANAGER_H
#define STREAM_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "protocol_manager.h"

#ifndef STREAM_MANAGER_MAX_SAMPLE_SIZE
#define STREAM_MANAGER_MAX_SAMPLE_SIZE       120U
#endif

#ifndef STREAM_MANAGER_DATA_CMD
#define STREAM_MANAGER_DATA_CMD              0x70U
#endif

#define STREAM_MANAGER_FLAG_DROP_ALLOWED     0x01U
#define STREAM_MANAGER_FLAG_IMPORTANT        0x02U

typedef enum
{
    STREAM_MANAGER_OK = 0,
    STREAM_MANAGER_ERROR = -1,
    STREAM_MANAGER_INVALID_PARAM = -2,
    STREAM_MANAGER_NOT_INITIALIZED = -3,
    STREAM_MANAGER_PAYLOAD_TOO_LARGE = -4,
    STREAM_MANAGER_TX_QUEUE_FULL = -5,
    STREAM_MANAGER_BAD_FRAME = -6,
    STREAM_MANAGER_BAD_LENGTH = -7
} StreamManagerResult_t;

typedef enum
{
    STREAM_MANAGER_CHANNEL_IMU = 1,
    STREAM_MANAGER_CHANNEL_TRACE = 2,
    STREAM_MANAGER_CHANNEL_LOG = 3,
    STREAM_MANAGER_CHANNEL_USER = 4
} StreamManagerChannelId_t;

typedef struct
{
    uint32_t init_count;
    uint32_t send_count;
    uint32_t send_error_count;
    uint32_t queue_full_count;
    uint32_t invalid_param_count;
    uint32_t payload_too_large_count;
    uint32_t not_initialized_count;

    uint32_t rx_data_count;
    uint32_t rx_bad_frame_count;
    uint32_t rx_bad_length_count;

    uint32_t bytes_submitted;
    uint32_t rx_bytes;

    uint8_t last_channel_id;
    uint8_t last_flags;
    uint16_t last_stream_seq;
    uint16_t last_sample_len;
    uint32_t last_timestamp_ms;

    uint8_t last_rx_channel_id;
    uint16_t last_rx_stream_seq;
    uint16_t last_rx_sample_len;
    uint32_t last_rx_timestamp_ms;

    int last_error;
} StreamManagerStats_t;

void StreamManager_Init(void);

int StreamManager_SendSample(uint8_t channel_id,
                             const uint8_t *sample,
                             uint16_t sample_len,
                             ProtocolTxPriority_t priority);

int StreamManager_SendSampleEx(uint8_t channel_id,
                               uint8_t flags,
                               const uint8_t *sample,
                               uint16_t sample_len,
                               ProtocolTxPriority_t priority);

int StreamManager_HandleRxData(const ProtocolFrame_t *frame);

void StreamManager_ResetStats(void);
const StreamManagerStats_t *StreamManager_GetStats(void);
void StreamManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_MANAGER_H */
