#ifndef BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "protocol_manager.h"

#ifndef BLOCK_MANAGER_DATA_CMD
#define BLOCK_MANAGER_DATA_CMD                  0x71U
#endif

#ifndef BLOCK_MANAGER_MAX_CHUNK_SIZE
#define BLOCK_MANAGER_MAX_CHUNK_SIZE            112U
#endif

#define BLOCK_MANAGER_FLAG_NEED_ACK             0x01U
#define BLOCK_MANAGER_FLAG_MORE                 0x02U
#define BLOCK_MANAGER_FLAG_RETRY                0x04U

typedef enum
{
    BLOCK_MANAGER_OK = 0,
    BLOCK_MANAGER_ERROR = -1,
    BLOCK_MANAGER_INVALID_PARAM = -2,
    BLOCK_MANAGER_NOT_INITIALIZED = -3,
    BLOCK_MANAGER_CHUNK_TOO_LARGE = -4,
    BLOCK_MANAGER_TX_QUEUE_FULL = -5,
    BLOCK_MANAGER_BAD_FRAME = -6,
    BLOCK_MANAGER_BAD_LENGTH = -7,
    BLOCK_MANAGER_UNKNOWN_OP = -8
} BlockManagerResult_t;

typedef enum
{
    BLOCK_MANAGER_OP_BEGIN = 1,
    BLOCK_MANAGER_OP_CHUNK = 2,
    BLOCK_MANAGER_OP_END = 3,
    BLOCK_MANAGER_OP_ABORT = 4,
    BLOCK_MANAGER_OP_ACK = 5,
    BLOCK_MANAGER_OP_NACK = 6
} BlockManagerOp_t;

typedef struct
{
    uint8_t op;
    uint8_t handle;
    uint8_t flags;
    uint8_t seq;
    uint8_t frame_type;
    uint32_t offset;
    uint16_t data_len;
    const uint8_t *data;
} BlockManagerPacket_t;

typedef uint8_t (*BlockManagerRxPacketHandler_t)(const BlockManagerPacket_t *packet, void *user);

typedef struct
{
    uint32_t init_count;
    uint32_t begin_send_count;
    uint32_t chunk_send_count;
    uint32_t end_send_count;
    uint32_t abort_send_count;
    uint32_t ack_send_count;
    uint32_t nack_send_count;
    uint32_t send_error_count;
    uint32_t queue_full_count;
    uint32_t invalid_param_count;
    uint32_t chunk_too_large_count;
    uint32_t not_initialized_count;

    uint32_t rx_data_count;
    uint32_t rx_begin_count;
    uint32_t rx_chunk_count;
    uint32_t rx_end_count;
    uint32_t rx_abort_count;
    uint32_t rx_ack_count;
    uint32_t rx_nack_count;
    uint32_t rx_window_ack_count;
    uint32_t rx_bad_frame_count;
    uint32_t rx_bad_length_count;
    uint32_t rx_unknown_op_count;

    uint32_t bytes_submitted;
    uint32_t rx_bytes;

    uint8_t last_op;
    uint8_t last_handle;
    uint8_t last_flags;
    uint32_t last_offset;
    uint16_t last_len;

    uint8_t last_rx_op;
    uint8_t last_rx_handle;
    uint8_t last_rx_flags;
    uint32_t last_rx_offset;
    uint16_t last_rx_len;

    int last_error;
} BlockManagerStats_t;

void BlockManager_Init(void);

int BlockManager_SendBegin(uint8_t handle,
                           uint32_t total_size,
                           uint32_t crc32,
                           ProtocolTxPriority_t priority);

int BlockManager_SendChunk(uint8_t handle,
                           uint32_t offset,
                           const uint8_t *chunk,
                           uint16_t chunk_len,
                           uint8_t flags,
                           ProtocolTxPriority_t priority);

int BlockManager_SendEnd(uint8_t handle,
                         uint32_t crc32,
                         ProtocolTxPriority_t priority);

int BlockManager_SendAbort(uint8_t handle,
                           uint8_t reason,
                           ProtocolTxPriority_t priority);

int BlockManager_SendAck(uint8_t handle,
                         uint32_t offset,
                         uint16_t accepted_len);

int BlockManager_SendNack(uint8_t handle,
                          uint32_t offset,
                          uint8_t error_code);

int BlockManager_HandleRxData(const ProtocolFrame_t *frame);
int BlockManager_HandleRxAck(const ProtocolFrame_t *frame);
int BlockManager_HandleRxWindowAck(const ProtocolFrame_t *frame);

void BlockManager_SetRxPacketHandler(BlockManagerRxPacketHandler_t handler, void *user);

void BlockManager_ResetStats(void);
const BlockManagerStats_t *BlockManager_GetStats(void);
void BlockManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* BLOCK_MANAGER_H */
