#include "block_transfer_session.h"

#include "storage_manager.h"
#include "board_log.h"

#include <string.h>

#define BLOCK_TRANSFER_DOWNLOAD_CHUNK_SIZE  112U

static BlockTransferSessionStats_t g_session;

static uint32_t BlockTransfer_Crc32Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    crc = crc ^ 0xFFFFFFFFUL;

    for (i = 0UL; i < len; i++)
    {
        uint32_t j;
        crc ^= (uint32_t)data[i];
        for (j = 0UL; j < 8UL; j++)
        {
            uint32_t mask = 0UL - (crc & 1UL);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

static void BlockTransfer_SetError(int error)
{
    g_session.last_error = error;
    if (error != BLOCK_TRANSFER_OK)
    {
        g_session.error_count++;
        g_session.state = BLOCK_TRANSFER_STATE_ERROR;
    }
}

void BlockTransferSession_Init(void)
{
    memset(&g_session, 0, sizeof(g_session));
    g_session.init_count++;
    g_session.state = BLOCK_TRANSFER_STATE_IDLE;
    g_session.direction = BLOCK_TRANSFER_DIR_NONE;
    g_session.partition_id = STORAGE_PARTITION_COUNT;
    BlockManager_SetRxPacketHandler(BlockTransferSession_OnBlockPacket, 0);
    BoardLog_Info("BlockTransferSession init OK\r\n");
}

int BlockTransferSession_StartUpload(StoragePartitionId_t partition_id,
                                     uint32_t base_offset,
                                     uint32_t total_size,
                                     uint32_t expected_crc32,
                                     uint8_t handle)
{
    uint8_t erased = 0U;
    int ret;

    if ((total_size == 0UL) || (handle == 0U))
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_INVALID_PARAM);
        return BLOCK_TRANSFER_INVALID_PARAM;
    }

    if ((g_session.state == BLOCK_TRANSFER_STATE_RECEIVING) ||
        (g_session.state == BLOCK_TRANSFER_STATE_SENDING))
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_BUSY);
        return BLOCK_TRANSFER_BUSY;
    }

    if (StorageManager_IsHostWritable(partition_id) == 0)
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_STORAGE_ERROR);
        return BLOCK_TRANSFER_STORAGE_ERROR;
    }

    ret = StorageManager_IsErased(partition_id, base_offset, total_size, &erased);
    if (ret != STORAGE_MANAGER_OK)
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_STORAGE_ERROR);
        return BLOCK_TRANSFER_STORAGE_ERROR;
    }

    if (erased == 0U)
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_STORAGE_ERROR);
        return BLOCK_TRANSFER_STORAGE_ERROR;
    }

    g_session.state = BLOCK_TRANSFER_STATE_RECEIVING;
    g_session.direction = BLOCK_TRANSFER_DIR_PC_TO_MCU;
    g_session.partition_id = partition_id;
    g_session.handle = handle;
    g_session.base_offset = base_offset;
    g_session.total_size = total_size;
    g_session.current_offset = 0UL;
    g_session.expected_crc32 = expected_crc32;
    g_session.running_crc32 = 0UL;
    g_session.upload_begin_count++;
    g_session.last_error = BLOCK_TRANSFER_OK;

    BoardLog_Info("[BTS] upload begin partition=%d offset=0x%06lX size=%lu handle=0x%02X crc=0x%08lX\r\n",
                  (int)partition_id,
                  (unsigned long)base_offset,
                  (unsigned long)total_size,
                  handle,
                  (unsigned long)expected_crc32);

    return BLOCK_TRANSFER_OK;
}

int BlockTransferSession_StartDownload(StoragePartitionId_t partition_id,
                                       uint32_t base_offset,
                                       uint32_t total_size,
                                       uint32_t expected_crc32,
                                       uint8_t handle)
{
    if ((total_size == 0UL) || (handle == 0U))
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_INVALID_PARAM);
        return BLOCK_TRANSFER_INVALID_PARAM;
    }

    if ((g_session.state == BLOCK_TRANSFER_STATE_RECEIVING) ||
        (g_session.state == BLOCK_TRANSFER_STATE_SENDING))
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_BUSY);
        return BLOCK_TRANSFER_BUSY;
    }

    if (StorageManager_IsHostReadable(partition_id) == 0)
    {
        BlockTransfer_SetError(BLOCK_TRANSFER_STORAGE_ERROR);
        return BLOCK_TRANSFER_STORAGE_ERROR;
    }

    g_session.state = BLOCK_TRANSFER_STATE_SENDING;
    g_session.direction = BLOCK_TRANSFER_DIR_MCU_TO_PC;
    g_session.partition_id = partition_id;
    g_session.handle = handle;
    g_session.base_offset = base_offset;
    g_session.total_size = total_size;
    g_session.current_offset = 0UL;
    g_session.expected_crc32 = expected_crc32;
    g_session.running_crc32 = 0UL;
    g_session.download_begin_count++;
    g_session.last_error = BLOCK_TRANSFER_OK;

    (void)BlockManager_SendBegin(handle, total_size, expected_crc32, PROTOCOL_TX_PRIORITY_NORMAL);

    BoardLog_Info("[BTS] download begin partition=%d offset=0x%06lX size=%lu handle=0x%02X\r\n",
                  (int)partition_id,
                  (unsigned long)base_offset,
                  (unsigned long)total_size,
                  handle);

    return BLOCK_TRANSFER_OK;
}

uint8_t BlockTransferSession_OnBlockPacket(const BlockManagerPacket_t *packet, void *user)
{
    int ret;
    (void)user;

    if (packet == 0)
    {
        return 0U;
    }

    if (g_session.state != BLOCK_TRANSFER_STATE_RECEIVING)
    {
        return 0U;
    }

    if ((g_session.direction != BLOCK_TRANSFER_DIR_PC_TO_MCU) ||
        (packet->handle != g_session.handle))
    {
        return 0U;
    }

    if (packet->op == BLOCK_MANAGER_OP_BEGIN)
    {
        (void)BlockManager_SendAck(packet->handle, 0UL, packet->data_len);
        return 1U;
    }

    if (packet->op == BLOCK_MANAGER_OP_ABORT)
    {
        g_session.abort_count++;
        g_session.state = BLOCK_TRANSFER_STATE_ABORTED;
        g_session.last_error = BLOCK_TRANSFER_OK;
        (void)BlockManager_SendAck(packet->handle, packet->offset, packet->data_len);
        return 1U;
    }

    if (packet->op == BLOCK_MANAGER_OP_CHUNK)
    {
        if (packet->offset != g_session.current_offset)
        {
            (void)BlockManager_SendNack(packet->handle, packet->offset, 0x02U);
            BlockTransfer_SetError(BLOCK_TRANSFER_BAD_OFFSET);
            return 1U;
        }

        if (((uint32_t)packet->offset + (uint32_t)packet->data_len) > g_session.total_size)
        {
            (void)BlockManager_SendNack(packet->handle, packet->offset, 0x03U);
            BlockTransfer_SetError(BLOCK_TRANSFER_INVALID_PARAM);
            return 1U;
        }

        ret = StorageManager_Write(g_session.partition_id,
                                   g_session.base_offset + packet->offset,
                                   packet->data,
                                   packet->data_len);
        if (ret != STORAGE_MANAGER_OK)
        {
            (void)BlockManager_SendNack(packet->handle, packet->offset, 0x04U);
            BlockTransfer_SetError(BLOCK_TRANSFER_STORAGE_ERROR);
            return 1U;
        }

        g_session.running_crc32 = BlockTransfer_Crc32Update(g_session.running_crc32, packet->data, packet->data_len);
        g_session.current_offset += packet->data_len;
        g_session.bytes_written += packet->data_len;
        g_session.upload_chunk_count++;

        (void)BlockManager_SendAck(packet->handle, packet->offset, packet->data_len);
        return 1U;
    }

    if (packet->op == BLOCK_MANAGER_OP_END)
    {
        uint32_t host_crc = 0UL;

        if (packet->data_len >= 4U)
        {
            host_crc = ((uint32_t)packet->data[0]) |
                       ((uint32_t)packet->data[1] << 8) |
                       ((uint32_t)packet->data[2] << 16) |
                       ((uint32_t)packet->data[3] << 24);
        }

        g_session.upload_end_count++;
        g_session.verify_count++;

        if ((g_session.current_offset != g_session.total_size) ||
            ((g_session.expected_crc32 != 0UL) && (g_session.running_crc32 != g_session.expected_crc32)) ||
            ((host_crc != 0UL) && (host_crc != g_session.running_crc32)))
        {
            (void)BlockManager_SendNack(packet->handle, packet->offset, 0x05U);
            BlockTransfer_SetError(BLOCK_TRANSFER_VERIFY_ERROR);
            return 1U;
        }

        g_session.state = BLOCK_TRANSFER_STATE_COMPLETED;
        g_session.direction = BLOCK_TRANSFER_DIR_NONE;
        g_session.last_error = BLOCK_TRANSFER_OK;
        (void)BlockManager_SendAck(packet->handle, packet->offset, packet->data_len);
        return 1U;
    }

    return 0U;
}

void BlockTransferSession_Process(void)
{
    uint8_t buf[BLOCK_TRANSFER_DOWNLOAD_CHUNK_SIZE];
    uint32_t remaining;
    uint16_t chunk_len;
    uint8_t flags;
    int ret;

    if ((g_session.state != BLOCK_TRANSFER_STATE_SENDING) ||
        (g_session.direction != BLOCK_TRANSFER_DIR_MCU_TO_PC))
    {
        return;
    }

    remaining = g_session.total_size - g_session.current_offset;
    if (remaining == 0UL)
    {
        (void)BlockManager_SendEnd(g_session.handle, g_session.running_crc32, PROTOCOL_TX_PRIORITY_NORMAL);
        g_session.download_end_count++;
        g_session.verify_count++;
        g_session.state = BLOCK_TRANSFER_STATE_COMPLETED;
        g_session.direction = BLOCK_TRANSFER_DIR_NONE;
        g_session.last_error = BLOCK_TRANSFER_OK;
        return;
    }

    chunk_len = (remaining > BLOCK_TRANSFER_DOWNLOAD_CHUNK_SIZE) ?
                (uint16_t)BLOCK_TRANSFER_DOWNLOAD_CHUNK_SIZE :
                (uint16_t)remaining;

    ret = StorageManager_Read(g_session.partition_id,
                              g_session.base_offset + g_session.current_offset,
                              buf,
                              chunk_len);
    if (ret != STORAGE_MANAGER_OK)
    {
        (void)BlockManager_SendAbort(g_session.handle, 0x04U, PROTOCOL_TX_PRIORITY_HIGH);
        BlockTransfer_SetError(BLOCK_TRANSFER_STORAGE_ERROR);
        return;
    }

    flags = BLOCK_MANAGER_FLAG_NEED_ACK;
    if ((g_session.current_offset + chunk_len) < g_session.total_size)
    {
        flags |= BLOCK_MANAGER_FLAG_MORE;
    }

    ret = BlockManager_SendChunk(g_session.handle,
                                 g_session.current_offset,
                                 buf,
                                 chunk_len,
                                 flags,
                                 PROTOCOL_TX_PRIORITY_NORMAL);
    if (ret != BLOCK_MANAGER_OK)
    {
        return; /* TX queue may be full; retry next Process() call. */
    }

    g_session.running_crc32 = BlockTransfer_Crc32Update(g_session.running_crc32, buf, chunk_len);
    g_session.current_offset += chunk_len;
    g_session.bytes_read += chunk_len;
    g_session.download_chunk_count++;
}

void BlockTransferSession_Abort(uint8_t reason)
{
    if ((g_session.state == BLOCK_TRANSFER_STATE_RECEIVING) ||
        (g_session.state == BLOCK_TRANSFER_STATE_SENDING))
    {
        (void)BlockManager_SendAbort(g_session.handle, reason, PROTOCOL_TX_PRIORITY_HIGH);
    }

    g_session.abort_count++;
    g_session.state = BLOCK_TRANSFER_STATE_ABORTED;
    g_session.direction = BLOCK_TRANSFER_DIR_NONE;
    g_session.last_error = BLOCK_TRANSFER_OK;
}

const BlockTransferSessionStats_t *BlockTransferSession_GetStats(void)
{
    return &g_session;
}

void BlockTransferSession_ResetStats(void)
{
    uint32_t init_count = g_session.init_count;
    memset(&g_session, 0, sizeof(g_session));
    g_session.init_count = init_count;
    g_session.state = BLOCK_TRANSFER_STATE_IDLE;
    g_session.direction = BLOCK_TRANSFER_DIR_NONE;
    g_session.partition_id = STORAGE_PARTITION_COUNT;
}
