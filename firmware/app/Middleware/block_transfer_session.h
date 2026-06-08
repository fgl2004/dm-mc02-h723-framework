#ifndef BLOCK_TRANSFER_SESSION_H
#define BLOCK_TRANSFER_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "storage_partition.h"
#include "block_manager.h"

typedef enum
{
    BLOCK_TRANSFER_OK = 0,
    BLOCK_TRANSFER_ERROR = -1,
    BLOCK_TRANSFER_INVALID_PARAM = -2,
    BLOCK_TRANSFER_BUSY = -3,
    BLOCK_TRANSFER_BAD_STATE = -4,
    BLOCK_TRANSFER_BAD_HANDLE = -5,
    BLOCK_TRANSFER_BAD_OFFSET = -6,
    BLOCK_TRANSFER_STORAGE_ERROR = -7,
    BLOCK_TRANSFER_VERIFY_ERROR = -8,
    BLOCK_TRANSFER_NOT_ACTIVE = -9
} BlockTransferResult_t;

typedef enum
{
    BLOCK_TRANSFER_STATE_IDLE = 0,
    BLOCK_TRANSFER_STATE_RECEIVING,
    BLOCK_TRANSFER_STATE_SENDING,
    BLOCK_TRANSFER_STATE_VERIFYING,
    BLOCK_TRANSFER_STATE_COMPLETED,
    BLOCK_TRANSFER_STATE_ABORTED,
    BLOCK_TRANSFER_STATE_ERROR
} BlockTransferState_t;

typedef enum
{
    BLOCK_TRANSFER_DIR_NONE = 0,
    BLOCK_TRANSFER_DIR_PC_TO_MCU,
    BLOCK_TRANSFER_DIR_MCU_TO_PC
} BlockTransferDirection_t;

typedef struct
{
    uint32_t init_count;
    uint32_t upload_begin_count;
    uint32_t upload_chunk_count;
    uint32_t upload_end_count;
    uint32_t download_begin_count;
    uint32_t download_chunk_count;
    uint32_t download_end_count;
    uint32_t abort_count;
    uint32_t verify_count;
    uint32_t error_count;

    uint32_t bytes_written;
    uint32_t bytes_read;

    BlockTransferState_t state;
    BlockTransferDirection_t direction;
    StoragePartitionId_t partition_id;
    uint8_t handle;
    uint32_t base_offset;
    uint32_t total_size;
    uint32_t current_offset;
    uint32_t expected_crc32;
    uint32_t running_crc32;
    int last_error;
} BlockTransferSessionStats_t;

void BlockTransferSession_Init(void);

int BlockTransferSession_StartUpload(StoragePartitionId_t partition_id,
                                     uint32_t base_offset,
                                     uint32_t total_size,
                                     uint32_t expected_crc32,
                                     uint8_t handle);

int BlockTransferSession_StartDownload(StoragePartitionId_t partition_id,
                                       uint32_t base_offset,
                                       uint32_t total_size,
                                       uint32_t expected_crc32,
                                       uint8_t handle);

uint8_t BlockTransferSession_OnBlockPacket(const BlockManagerPacket_t *packet, void *user);

void BlockTransferSession_Process(void);
void BlockTransferSession_Abort(uint8_t reason);

const BlockTransferSessionStats_t *BlockTransferSession_GetStats(void);
void BlockTransferSession_ResetStats(void);

#ifdef __cplusplus
}
#endif

#endif /* BLOCK_TRANSFER_SESSION_H */
