#include "stream_manager.h"
#include "platform_time.h"
#include "board_log.h"
#include <string.h>

typedef struct
{
    uint8_t initialized;
    uint16_t seq;
    StreamManagerStats_t stats;
} StreamManagerContext_t;

static StreamManagerContext_t g_stream_manager;

static void StreamManager_WriteLe16(uint8_t *buf, uint16_t value);
static void StreamManager_WriteLe32(uint8_t *buf, uint32_t value);
static uint16_t StreamManager_ReadLe16(const uint8_t *buf);
static uint32_t StreamManager_ReadLe32(const uint8_t *buf);

void StreamManager_Init(void)
{
    memset(&g_stream_manager, 0, sizeof(g_stream_manager));
    g_stream_manager.initialized = 1U;
    g_stream_manager.stats.init_count++;
    BoardLog_Info("StreamManager init OK\r\n");
}

int StreamManager_SendSample(uint8_t channel_id,
                             const uint8_t *sample,
                             uint16_t sample_len,
                             ProtocolTxPriority_t priority)
{
    return StreamManager_SendSampleEx(channel_id,
                                      STREAM_MANAGER_FLAG_DROP_ALLOWED,
                                      sample,
                                      sample_len,
                                      priority);
}

int StreamManager_SendSampleEx(uint8_t channel_id,
                               uint8_t flags,
                               const uint8_t *sample,
                               uint16_t sample_len,
                               ProtocolTxPriority_t priority)
{
    uint8_t payload[8U + STREAM_MANAGER_MAX_SAMPLE_SIZE];
    uint16_t seq;
    uint32_t timestamp_ms;
    int ret;

    if (g_stream_manager.initialized == 0U)
    {
        g_stream_manager.stats.not_initialized_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_NOT_INITIALIZED;
        return STREAM_MANAGER_NOT_INITIALIZED;
    }

    if ((sample_len > 0U) && (sample == 0))
    {
        g_stream_manager.stats.invalid_param_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_INVALID_PARAM;
        return STREAM_MANAGER_INVALID_PARAM;
    }

    if (sample_len > STREAM_MANAGER_MAX_SAMPLE_SIZE)
    {
        g_stream_manager.stats.payload_too_large_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_PAYLOAD_TOO_LARGE;
        return STREAM_MANAGER_PAYLOAD_TOO_LARGE;
    }

    seq = g_stream_manager.seq++;
    timestamp_ms = PlatformTime_GetMs();

    payload[0] = channel_id;
    StreamManager_WriteLe16(&payload[1], seq);
    StreamManager_WriteLe32(&payload[3], timestamp_ms);
    payload[7] = (uint8_t)sample_len;

    if ((sample != 0) && (sample_len > 0U))
    {
        memcpy(&payload[8], sample, sample_len);
    }

    ret = ProtocolManager_SendData(flags,
                                   (uint8_t)(seq & 0xFFU),
                                   STREAM_MANAGER_DATA_CMD,
                                   payload,
                                   (uint16_t)(8U + sample_len),
                                   priority);

    if (ret != PROTOCOL_MANAGER_OK)
    {
        g_stream_manager.stats.send_error_count++;
        g_stream_manager.stats.last_error = ret;
        if (ret == PROTOCOL_MANAGER_TX_QUEUE_FULL)
        {
            g_stream_manager.stats.queue_full_count++;
            return STREAM_MANAGER_TX_QUEUE_FULL;
        }
        return STREAM_MANAGER_ERROR;
    }

    g_stream_manager.stats.send_count++;
    g_stream_manager.stats.bytes_submitted += sample_len;
    g_stream_manager.stats.last_channel_id = channel_id;
    g_stream_manager.stats.last_flags = flags;
    g_stream_manager.stats.last_stream_seq = seq;
    g_stream_manager.stats.last_sample_len = sample_len;
    g_stream_manager.stats.last_timestamp_ms = timestamp_ms;
    g_stream_manager.stats.last_error = STREAM_MANAGER_OK;

    return STREAM_MANAGER_OK;
}

int StreamManager_HandleRxData(const ProtocolFrame_t *frame)
{
    const uint8_t *payload;
    uint8_t channel_id;
    uint16_t seq;
    uint32_t timestamp_ms;
    uint8_t sample_len;

    if (frame == 0)
    {
        g_stream_manager.stats.invalid_param_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_INVALID_PARAM;
        return STREAM_MANAGER_INVALID_PARAM;
    }

    if (g_stream_manager.initialized == 0U)
    {
        g_stream_manager.stats.not_initialized_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_NOT_INITIALIZED;
        return STREAM_MANAGER_NOT_INITIALIZED;
    }

    if ((frame->type != PROTO_FRAME_TYPE_DATA) ||
        (frame->cmd != STREAM_MANAGER_DATA_CMD))
    {
        g_stream_manager.stats.rx_bad_frame_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_BAD_FRAME;
        return STREAM_MANAGER_BAD_FRAME;
    }

    if (frame->payload_len < 8U)
    {
        g_stream_manager.stats.rx_bad_length_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_BAD_LENGTH;
        return STREAM_MANAGER_BAD_LENGTH;
    }

    payload = frame->payload;
    channel_id = payload[0];
    seq = StreamManager_ReadLe16(&payload[1]);
    timestamp_ms = StreamManager_ReadLe32(&payload[3]);
    sample_len = payload[7];

    if (frame->payload_len != (uint16_t)(8U + sample_len))
    {
        g_stream_manager.stats.rx_bad_length_count++;
        g_stream_manager.stats.last_error = STREAM_MANAGER_BAD_LENGTH;
        return STREAM_MANAGER_BAD_LENGTH;
    }

    g_stream_manager.stats.rx_data_count++;
    g_stream_manager.stats.rx_bytes += sample_len;
    g_stream_manager.stats.last_rx_channel_id = channel_id;
    g_stream_manager.stats.last_rx_stream_seq = seq;
    g_stream_manager.stats.last_rx_timestamp_ms = timestamp_ms;
    g_stream_manager.stats.last_rx_sample_len = sample_len;
    g_stream_manager.stats.last_error = STREAM_MANAGER_OK;

    return STREAM_MANAGER_OK;
}

void StreamManager_ResetStats(void)
{
    memset(&g_stream_manager.stats, 0, sizeof(g_stream_manager.stats));
}

const StreamManagerStats_t *StreamManager_GetStats(void)
{
    return &g_stream_manager.stats;
}

void StreamManager_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("StreamManager Stats:\r\n");
    BoardLog_Info("  initialized              = %u\r\n", g_stream_manager.initialized);
    BoardLog_Info("  send_count               = %lu\r\n", g_stream_manager.stats.send_count);
    BoardLog_Info("  send_error_count         = %lu\r\n", g_stream_manager.stats.send_error_count);
    BoardLog_Info("  queue_full_count         = %lu\r\n", g_stream_manager.stats.queue_full_count);
    BoardLog_Info("  rx_data_count            = %lu\r\n", g_stream_manager.stats.rx_data_count);
    BoardLog_Info("  rx_bad_frame_count       = %lu\r\n", g_stream_manager.stats.rx_bad_frame_count);
    BoardLog_Info("  rx_bad_length_count      = %lu\r\n", g_stream_manager.stats.rx_bad_length_count);
    BoardLog_Info("  bytes_submitted          = %lu\r\n", g_stream_manager.stats.bytes_submitted);
    BoardLog_Info("  rx_bytes                 = %lu\r\n", g_stream_manager.stats.rx_bytes);
    BoardLog_Info("  last_channel_id          = 0x%02X\r\n", g_stream_manager.stats.last_channel_id);
    BoardLog_Info("  last_stream_seq          = %u\r\n", g_stream_manager.stats.last_stream_seq);
    BoardLog_Info("  last_sample_len          = %u\r\n", g_stream_manager.stats.last_sample_len);
    BoardLog_Info("  last_rx_channel_id       = 0x%02X\r\n", g_stream_manager.stats.last_rx_channel_id);
    BoardLog_Info("  last_rx_stream_seq       = %u\r\n", g_stream_manager.stats.last_rx_stream_seq);
    BoardLog_Info("  last_rx_sample_len       = %u\r\n", g_stream_manager.stats.last_rx_sample_len);
    BoardLog_Info("  last_error               = %d\r\n", g_stream_manager.stats.last_error);
}

static void StreamManager_WriteLe16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void StreamManager_WriteLe32(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);
    buf[2] = (uint8_t)((value >> 16) & 0xFFU);
    buf[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint16_t StreamManager_ReadLe16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0]) | ((uint16_t)buf[1] << 8));
}

static uint32_t StreamManager_ReadLe32(const uint8_t *buf)
{
    return ((uint32_t)buf[0]) |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}
