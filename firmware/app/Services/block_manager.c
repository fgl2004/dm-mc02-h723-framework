#include "block_manager.h"
#include "board_log.h"
#include <string.h>

typedef struct
{
    uint8_t initialized;
    uint8_t seq;
    BlockManagerStats_t stats;
    BlockManagerRxPacketHandler_t rx_handler;
    void *rx_handler_user;
} BlockManagerContext_t;

static BlockManagerContext_t g_block_manager;

static int BlockManager_SendPacket(uint8_t op,
                                   uint8_t handle,
                                   uint32_t offset,
                                   const uint8_t *data,
                                   uint16_t data_len,
                                   uint8_t flags,
                                   ProtocolTxPriority_t priority);

static int BlockManager_ParseRxPacket(const ProtocolFrame_t *frame,
                                      uint8_t *op,
                                      uint8_t *handle,
                                      uint32_t *offset,
                                      uint16_t *data_len,
                                      const uint8_t **data);

static void BlockManager_WriteLe16(uint8_t *buf, uint16_t value);
static void BlockManager_WriteLe32(uint8_t *buf, uint32_t value);
static uint16_t BlockManager_ReadLe16(const uint8_t *buf);
static uint32_t BlockManager_ReadLe32(const uint8_t *buf);
static void BlockManager_UpdateLast(uint8_t op, uint8_t handle, uint8_t flags, uint32_t offset, uint16_t len, int error);
static void BlockManager_UpdateLastRx(uint8_t op, uint8_t handle, uint8_t flags, uint32_t offset, uint16_t len, int error);

void BlockManager_Init(void)
{
    memset(&g_block_manager, 0, sizeof(g_block_manager));
    g_block_manager.initialized = 1U;
    g_block_manager.stats.init_count++;
    BoardLog_Info("BlockManager init OK\r\n");
}

int BlockManager_SendBegin(uint8_t handle, uint32_t total_size, uint32_t crc32, ProtocolTxPriority_t priority)
{
    uint8_t metadata[8];
    BlockManager_WriteLe32(&metadata[0], total_size);
    BlockManager_WriteLe32(&metadata[4], crc32);
    return BlockManager_SendPacket(BLOCK_MANAGER_OP_BEGIN, handle, 0U, metadata, (uint16_t)sizeof(metadata), BLOCK_MANAGER_FLAG_NEED_ACK, priority);
}

int BlockManager_SendChunk(uint8_t handle, uint32_t offset, const uint8_t *chunk, uint16_t chunk_len, uint8_t flags, ProtocolTxPriority_t priority)
{
    return BlockManager_SendPacket(BLOCK_MANAGER_OP_CHUNK, handle, offset, chunk, chunk_len, flags, priority);
}

int BlockManager_SendEnd(uint8_t handle, uint32_t crc32, ProtocolTxPriority_t priority)
{
    uint8_t metadata[4];
    BlockManager_WriteLe32(&metadata[0], crc32);
    return BlockManager_SendPacket(BLOCK_MANAGER_OP_END, handle, 0U, metadata, (uint16_t)sizeof(metadata), BLOCK_MANAGER_FLAG_NEED_ACK, priority);
}

int BlockManager_SendAbort(uint8_t handle, uint8_t reason, ProtocolTxPriority_t priority)
{
    return BlockManager_SendPacket(BLOCK_MANAGER_OP_ABORT, handle, 0U, &reason, 1U, BLOCK_MANAGER_FLAG_NEED_ACK, priority);
}

int BlockManager_SendAck(uint8_t handle, uint32_t offset, uint16_t accepted_len)
{
    uint8_t data[2];
    BlockManager_WriteLe16(data, accepted_len);
    return BlockManager_SendPacket(BLOCK_MANAGER_OP_ACK, handle, offset, data, (uint16_t)sizeof(data), 0U, PROTOCOL_TX_PRIORITY_HIGH);
}

int BlockManager_SendNack(uint8_t handle, uint32_t offset, uint8_t error_code)
{
    return BlockManager_SendPacket(BLOCK_MANAGER_OP_NACK, handle, offset, &error_code, 1U, 0U, PROTOCOL_TX_PRIORITY_HIGH);
}

int BlockManager_HandleRxData(const ProtocolFrame_t *frame)
{
    uint8_t op;
    uint8_t handle;
    uint32_t offset;
    uint16_t data_len;
    const uint8_t *data;
    int ret;

    ret = BlockManager_ParseRxPacket(frame, &op, &handle, &offset, &data_len, &data);
    if (ret != BLOCK_MANAGER_OK)
    {
        return ret;
    }

    g_block_manager.stats.rx_data_count++;
    g_block_manager.stats.rx_bytes += data_len;

    if (g_block_manager.rx_handler != 0)
    {
        BlockManagerPacket_t packet;

        packet.op = op;
        packet.handle = handle;
        packet.flags = frame->flags;
        packet.seq = frame->seq;
        packet.frame_type = frame->type;
        packet.offset = offset;
        packet.data_len = data_len;
        packet.data = data;

        if (g_block_manager.rx_handler(&packet, g_block_manager.rx_handler_user) != 0U)
        {
            switch (op)
            {
                case BLOCK_MANAGER_OP_BEGIN: g_block_manager.stats.rx_begin_count++; break;
                case BLOCK_MANAGER_OP_CHUNK: g_block_manager.stats.rx_chunk_count++; break;
                case BLOCK_MANAGER_OP_END: g_block_manager.stats.rx_end_count++; break;
                case BLOCK_MANAGER_OP_ABORT: g_block_manager.stats.rx_abort_count++; break;
                case BLOCK_MANAGER_OP_ACK: g_block_manager.stats.rx_ack_count++; break;
                case BLOCK_MANAGER_OP_NACK: g_block_manager.stats.rx_nack_count++; break;
                default: g_block_manager.stats.rx_unknown_op_count++; break;
            }

            BlockManager_UpdateLastRx(op, handle, frame->flags, offset, data_len, BLOCK_MANAGER_OK);
            return BLOCK_MANAGER_OK;
        }
    }

    switch (op)
    {
        case BLOCK_MANAGER_OP_BEGIN:
            g_block_manager.stats.rx_begin_count++;
            break;

        case BLOCK_MANAGER_OP_CHUNK:
            g_block_manager.stats.rx_chunk_count++;
            (void)BlockManager_SendAck(handle, offset, data_len);
            break;

        case BLOCK_MANAGER_OP_END:
            g_block_manager.stats.rx_end_count++;
            break;

        case BLOCK_MANAGER_OP_ABORT:
            g_block_manager.stats.rx_abort_count++;
            break;

        case BLOCK_MANAGER_OP_ACK:
            g_block_manager.stats.rx_ack_count++;
            break;

        case BLOCK_MANAGER_OP_NACK:
            g_block_manager.stats.rx_nack_count++;
            break;

        default:
            g_block_manager.stats.rx_unknown_op_count++;
            BlockManager_UpdateLastRx(op, handle, frame->flags, offset, data_len, BLOCK_MANAGER_UNKNOWN_OP);
            return BLOCK_MANAGER_UNKNOWN_OP;
    }

    (void)data;
    BlockManager_UpdateLastRx(op, handle, frame->flags, offset, data_len, BLOCK_MANAGER_OK);
    return BLOCK_MANAGER_OK;
}

int BlockManager_HandleRxAck(const ProtocolFrame_t *frame)
{
    uint8_t op;
    uint8_t handle;
    uint32_t offset;
    uint16_t data_len;
    const uint8_t *data;
    int ret;

    ret = BlockManager_ParseRxPacket(frame, &op, &handle, &offset, &data_len, &data);
    if (ret != BLOCK_MANAGER_OK)
    {
        return ret;
    }

    (void)data;

    if (op == BLOCK_MANAGER_OP_ACK)
    {
        g_block_manager.stats.rx_ack_count++;
    }
    else if (op == BLOCK_MANAGER_OP_NACK)
    {
        g_block_manager.stats.rx_nack_count++;
    }
    else
    {
        g_block_manager.stats.rx_unknown_op_count++;
        BlockManager_UpdateLastRx(op, handle, frame->flags, offset, data_len, BLOCK_MANAGER_UNKNOWN_OP);
        return BLOCK_MANAGER_UNKNOWN_OP;
    }

    BlockManager_UpdateLastRx(op, handle, frame->flags, offset, data_len, BLOCK_MANAGER_OK);
    return BLOCK_MANAGER_OK;
}

int BlockManager_HandleRxWindowAck(const ProtocolFrame_t *frame)
{
    if (frame == 0)
    {
        g_block_manager.stats.invalid_param_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_INVALID_PARAM;
        return BLOCK_MANAGER_INVALID_PARAM;
    }

    if (g_block_manager.initialized == 0U)
    {
        g_block_manager.stats.not_initialized_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_NOT_INITIALIZED;
        return BLOCK_MANAGER_NOT_INITIALIZED;
    }

    if (frame->cmd != BLOCK_MANAGER_DATA_CMD)
    {
        g_block_manager.stats.rx_bad_frame_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_BAD_FRAME;
        return BLOCK_MANAGER_BAD_FRAME;
    }

    g_block_manager.stats.rx_window_ack_count++;
    g_block_manager.stats.last_error = BLOCK_MANAGER_OK;
    return BLOCK_MANAGER_OK;
}

void BlockManager_SetRxPacketHandler(BlockManagerRxPacketHandler_t handler, void *user)
{
    g_block_manager.rx_handler = handler;
    g_block_manager.rx_handler_user = user;
}

void BlockManager_ResetStats(void)
{
    memset(&g_block_manager.stats, 0, sizeof(g_block_manager.stats));
}

const BlockManagerStats_t *BlockManager_GetStats(void)
{
    return &g_block_manager.stats;
}

void BlockManager_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("BlockManager Stats:\r\n");
    BoardLog_Info("  initialized              = %u\r\n", g_block_manager.initialized);
    BoardLog_Info("  begin_send_count         = %lu\r\n", g_block_manager.stats.begin_send_count);
    BoardLog_Info("  chunk_send_count         = %lu\r\n", g_block_manager.stats.chunk_send_count);
    BoardLog_Info("  end_send_count           = %lu\r\n", g_block_manager.stats.end_send_count);
    BoardLog_Info("  ack_send_count           = %lu\r\n", g_block_manager.stats.ack_send_count);
    BoardLog_Info("  nack_send_count          = %lu\r\n", g_block_manager.stats.nack_send_count);
    BoardLog_Info("  rx_data_count            = %lu\r\n", g_block_manager.stats.rx_data_count);
    BoardLog_Info("  rx_begin_count           = %lu\r\n", g_block_manager.stats.rx_begin_count);
    BoardLog_Info("  rx_chunk_count           = %lu\r\n", g_block_manager.stats.rx_chunk_count);
    BoardLog_Info("  rx_end_count             = %lu\r\n", g_block_manager.stats.rx_end_count);
    BoardLog_Info("  rx_ack_count             = %lu\r\n", g_block_manager.stats.rx_ack_count);
    BoardLog_Info("  rx_nack_count            = %lu\r\n", g_block_manager.stats.rx_nack_count);
    BoardLog_Info("  rx_window_ack_count      = %lu\r\n", g_block_manager.stats.rx_window_ack_count);
    BoardLog_Info("  rx_bad_frame_count       = %lu\r\n", g_block_manager.stats.rx_bad_frame_count);
    BoardLog_Info("  rx_bad_length_count      = %lu\r\n", g_block_manager.stats.rx_bad_length_count);
    BoardLog_Info("  rx_unknown_op_count      = %lu\r\n", g_block_manager.stats.rx_unknown_op_count);
    BoardLog_Info("  bytes_submitted          = %lu\r\n", g_block_manager.stats.bytes_submitted);
    BoardLog_Info("  rx_bytes                 = %lu\r\n", g_block_manager.stats.rx_bytes);
    BoardLog_Info("  last_rx_op               = 0x%02X\r\n", g_block_manager.stats.last_rx_op);
    BoardLog_Info("  last_rx_handle           = 0x%02X\r\n", g_block_manager.stats.last_rx_handle);
    BoardLog_Info("  last_rx_offset           = %lu\r\n", g_block_manager.stats.last_rx_offset);
    BoardLog_Info("  last_rx_len              = %u\r\n", g_block_manager.stats.last_rx_len);
    BoardLog_Info("  last_error               = %d\r\n", g_block_manager.stats.last_error);
}

static int BlockManager_SendPacket(uint8_t op, uint8_t handle, uint32_t offset, const uint8_t *data, uint16_t data_len, uint8_t flags, ProtocolTxPriority_t priority)
{
    uint8_t payload[8U + BLOCK_MANAGER_MAX_CHUNK_SIZE];
    int ret;

    if (g_block_manager.initialized == 0U)
    {
        g_block_manager.stats.not_initialized_count++;
        BlockManager_UpdateLast(op, handle, flags, offset, data_len, BLOCK_MANAGER_NOT_INITIALIZED);
        return BLOCK_MANAGER_NOT_INITIALIZED;
    }

    if ((data_len > 0U) && (data == 0))
    {
        g_block_manager.stats.invalid_param_count++;
        BlockManager_UpdateLast(op, handle, flags, offset, data_len, BLOCK_MANAGER_INVALID_PARAM);
        return BLOCK_MANAGER_INVALID_PARAM;
    }

    if (data_len > BLOCK_MANAGER_MAX_CHUNK_SIZE)
    {
        g_block_manager.stats.chunk_too_large_count++;
        BlockManager_UpdateLast(op, handle, flags, offset, data_len, BLOCK_MANAGER_CHUNK_TOO_LARGE);
        return BLOCK_MANAGER_CHUNK_TOO_LARGE;
    }

    payload[0] = op;
    payload[1] = handle;
    BlockManager_WriteLe32(&payload[2], offset);
    BlockManager_WriteLe16(&payload[6], data_len);

    if ((data != 0) && (data_len > 0U))
    {
        memcpy(&payload[8], data, data_len);
    }

    ret = ProtocolManager_SendData(flags, g_block_manager.seq++, BLOCK_MANAGER_DATA_CMD, payload, (uint16_t)(8U + data_len), priority);

    if (ret == PROTOCOL_MANAGER_OK)
    {
        switch (op)
        {
            case BLOCK_MANAGER_OP_BEGIN: g_block_manager.stats.begin_send_count++; break;
            case BLOCK_MANAGER_OP_CHUNK: g_block_manager.stats.chunk_send_count++; break;
            case BLOCK_MANAGER_OP_END: g_block_manager.stats.end_send_count++; break;
            case BLOCK_MANAGER_OP_ABORT: g_block_manager.stats.abort_send_count++; break;
            case BLOCK_MANAGER_OP_ACK: g_block_manager.stats.ack_send_count++; break;
            case BLOCK_MANAGER_OP_NACK: g_block_manager.stats.nack_send_count++; break;
            default: break;
        }

        g_block_manager.stats.bytes_submitted += data_len;
        BlockManager_UpdateLast(op, handle, flags, offset, data_len, BLOCK_MANAGER_OK);
        return BLOCK_MANAGER_OK;
    }

    g_block_manager.stats.send_error_count++;
    if (ret == PROTOCOL_MANAGER_TX_QUEUE_FULL)
    {
        g_block_manager.stats.queue_full_count++;
        BlockManager_UpdateLast(op, handle, flags, offset, data_len, BLOCK_MANAGER_TX_QUEUE_FULL);
        return BLOCK_MANAGER_TX_QUEUE_FULL;
    }

    BlockManager_UpdateLast(op, handle, flags, offset, data_len, BLOCK_MANAGER_ERROR);
    return BLOCK_MANAGER_ERROR;
}

static int BlockManager_ParseRxPacket(const ProtocolFrame_t *frame, uint8_t *op, uint8_t *handle, uint32_t *offset, uint16_t *data_len, const uint8_t **data)
{
    const uint8_t *payload;

    if ((frame == 0) || (op == 0) || (handle == 0) || (offset == 0) || (data_len == 0) || (data == 0))
    {
        g_block_manager.stats.invalid_param_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_INVALID_PARAM;
        return BLOCK_MANAGER_INVALID_PARAM;
    }

    if (g_block_manager.initialized == 0U)
    {
        g_block_manager.stats.not_initialized_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_NOT_INITIALIZED;
        return BLOCK_MANAGER_NOT_INITIALIZED;
    }

    if ((frame->type != PROTO_FRAME_TYPE_DATA) && (frame->type != PROTO_FRAME_TYPE_ACK) && (frame->type != PROTO_FRAME_TYPE_WINDOW_ACK))
    {
        g_block_manager.stats.rx_bad_frame_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_BAD_FRAME;
        return BLOCK_MANAGER_BAD_FRAME;
    }

    if (frame->cmd != BLOCK_MANAGER_DATA_CMD)
    {
        g_block_manager.stats.rx_bad_frame_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_BAD_FRAME;
        return BLOCK_MANAGER_BAD_FRAME;
    }

    if (frame->payload_len < 8U)
    {
        g_block_manager.stats.rx_bad_length_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_BAD_LENGTH;
        return BLOCK_MANAGER_BAD_LENGTH;
    }

    payload = frame->payload;
    *op = payload[0];
    *handle = payload[1];
    *offset = BlockManager_ReadLe32(&payload[2]);
    *data_len = BlockManager_ReadLe16(&payload[6]);

    if (frame->payload_len != (uint16_t)(8U + *data_len))
    {
        g_block_manager.stats.rx_bad_length_count++;
        g_block_manager.stats.last_error = BLOCK_MANAGER_BAD_LENGTH;
        return BLOCK_MANAGER_BAD_LENGTH;
    }

    *data = &payload[8];
    return BLOCK_MANAGER_OK;
}

static void BlockManager_WriteLe16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void BlockManager_WriteLe32(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);
    buf[2] = (uint8_t)((value >> 16) & 0xFFU);
    buf[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint16_t BlockManager_ReadLe16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0]) | ((uint16_t)buf[1] << 8));
}

static uint32_t BlockManager_ReadLe32(const uint8_t *buf)
{
    return ((uint32_t)buf[0]) | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static void BlockManager_UpdateLast(uint8_t op, uint8_t handle, uint8_t flags, uint32_t offset, uint16_t len, int error)
{
    g_block_manager.stats.last_op = op;
    g_block_manager.stats.last_handle = handle;
    g_block_manager.stats.last_flags = flags;
    g_block_manager.stats.last_offset = offset;
    g_block_manager.stats.last_len = len;
    g_block_manager.stats.last_error = error;
}

static void BlockManager_UpdateLastRx(uint8_t op, uint8_t handle, uint8_t flags, uint32_t offset, uint16_t len, int error)
{
    g_block_manager.stats.last_rx_op = op;
    g_block_manager.stats.last_rx_handle = handle;
    g_block_manager.stats.last_rx_flags = flags;
    g_block_manager.stats.last_rx_offset = offset;
    g_block_manager.stats.last_rx_len = len;
    g_block_manager.stats.last_error = error;
}
