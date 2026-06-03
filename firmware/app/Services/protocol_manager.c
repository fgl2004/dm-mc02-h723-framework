#include "protocol_manager.h"

#include "command_manager.h"

#include "platform_uart.h"
#include "platform_time.h"
#include "board_log.h"

#include <stdio.h>
#include <string.h>

#ifndef PROTOCOL_MANAGER_RX_READ_CHUNK
#define PROTOCOL_MANAGER_RX_READ_CHUNK              64U
#endif

#ifndef PROTOCOL_MANAGER_MAX_BYTES_PER_PROCESS
#define PROTOCOL_MANAGER_MAX_BYTES_PER_PROCESS      256U
#endif

#ifndef PROTOCOL_MANAGER_ENABLE_FRAME_LOG
#define PROTOCOL_MANAGER_ENABLE_FRAME_LOG           0U
#endif

#ifndef PROTOCOL_MANAGER_TX_BUFFER_SIZE
#define PROTOCOL_MANAGER_TX_BUFFER_SIZE             PROTO_FRAME_MAX_SIZE
#endif

typedef struct
{
    uint8_t initialized;

    ProtocolFrameParser_t parser;
    ProtocolManagerStats_t stats;

    uint8_t tx_buf[PROTOCOL_MANAGER_TX_BUFFER_SIZE];

    uint8_t rx_buf[PROTOCOL_MANAGER_RX_READ_CHUNK];
    ProtocolFrame_t rx_frame;
    CommandManagerResponse_t cmd_resp;
    CommandManagerEventRecord_t event_record;

    uint8_t event_seq;
} ProtocolManagerContext_t;

static ProtocolManagerContext_t g_protocol_manager;

static void ProtocolManager_ProcessRx(void);
static void ProtocolManager_ProcessPendingEvents(void);

static void ProtocolManager_HandleFrame(const ProtocolFrame_t *frame);
static void ProtocolManager_HandleReq(const ProtocolFrame_t *frame);

static int ProtocolManager_SendFrame(uint8_t type,
                                     uint8_t flags,
                                     uint8_t seq,
                                     uint8_t cmd,
                                     const uint8_t *payload,
                                     uint16_t payload_len);

static void ProtocolManager_UpdateLastRx(const ProtocolFrame_t *frame);
static void ProtocolManager_LogFrame(const char *prefix,
                                     const ProtocolFrame_t *frame);

void ProtocolManager_Init(void)
{
    memset(&g_protocol_manager, 0, sizeof(g_protocol_manager));

    if (ProtocolFrameParser_Init(&g_protocol_manager.parser, PlatformTime_GetMs()) == PROTO_FRAME_RESULT_OK)
    {
        g_protocol_manager.initialized = 1U;
        g_protocol_manager.stats.init_count++;

        BoardLog_Info("ProtocolManager init OK\r\n");
    }
    else
    {
        g_protocol_manager.initialized = 0U;
        g_protocol_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        BoardLog_Error("ProtocolManager init failed\r\n");
    }
}

void ProtocolManager_Process(void)
{
    uint32_t start_cycle;
    uint32_t elapsed_us;

    if (g_protocol_manager.initialized == 0U)
    {
        return;
    }

    start_cycle = PlatformTime_ProfileStart();

    g_protocol_manager.stats.process_count++;

    ProtocolManager_ProcessRx();
    ProtocolManager_ProcessPendingEvents();

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_protocol_manager.stats.last_process_us = elapsed_us;

    if (elapsed_us > g_protocol_manager.stats.max_process_us)
    {
        g_protocol_manager.stats.max_process_us = elapsed_us;
    }
}

const ProtocolManagerStats_t *ProtocolManager_GetStats(void)
{
    return &g_protocol_manager.stats;
}

const ProtocolFrameParserStats_t *ProtocolManager_GetParserStats(void)
{
    return ProtocolFrameParser_GetStats(&g_protocol_manager.parser);
}

void ProtocolManager_ResetStats(void)
{
    memset(&g_protocol_manager.stats, 0, sizeof(g_protocol_manager.stats));
}

void ProtocolManager_ResetParserStats(void)
{
    memset(&g_protocol_manager.parser.stats, 0, sizeof(g_protocol_manager.parser.stats));
}

void ProtocolManager_PrintStats(void)
{
    const ProtocolFrameParserStats_t *parser_stats;

    parser_stats = ProtocolFrameParser_GetStats(&g_protocol_manager.parser);

    BoardLog_PrintSeparator();

    BoardLog_Info("ProtocolManager Stats:\r\n");
    BoardLog_Info("  initialized        = %u\r\n", g_protocol_manager.initialized);
    BoardLog_Info("  init_count         = %lu\r\n", g_protocol_manager.stats.init_count);
    BoardLog_Info("  process_count      = %lu\r\n", g_protocol_manager.stats.process_count);
    BoardLog_Info("  last_process_us    = %lu\r\n", g_protocol_manager.stats.last_process_us);
    BoardLog_Info("  max_process_us     = %lu\r\n", g_protocol_manager.stats.max_process_us);
    BoardLog_Info("  rx_bytes_consumed  = %lu\r\n", g_protocol_manager.stats.rx_bytes_consumed);
    BoardLog_Info("  frame_received     = %lu\r\n", g_protocol_manager.stats.frame_received_count);
    BoardLog_Info("  frame_sent         = %lu\r\n", g_protocol_manager.stats.frame_sent_count);
    BoardLog_Info("  resp_sent          = %lu\r\n", g_protocol_manager.stats.resp_sent_count);
    BoardLog_Info("  nack_sent          = %lu\r\n", g_protocol_manager.stats.nack_sent_count);
    BoardLog_Info("  event_sent         = %lu\r\n", g_protocol_manager.stats.event_sent_count);
    BoardLog_Info("  req_frame_count    = %lu\r\n", g_protocol_manager.stats.req_frame_count);
    BoardLog_Info("  resp_frame_count   = %lu\r\n", g_protocol_manager.stats.resp_frame_count);
    BoardLog_Info("  nack_frame_count   = %lu\r\n", g_protocol_manager.stats.nack_frame_count);
    BoardLog_Info("  event_frame_count  = %lu\r\n", g_protocol_manager.stats.event_frame_count);
    BoardLog_Info("  other_frame_count  = %lu\r\n", g_protocol_manager.stats.other_frame_count);
    BoardLog_Info("  parser_error_count = %lu\r\n", g_protocol_manager.stats.parser_error_count);
    BoardLog_Info("  tx_error_count     = %lu\r\n", g_protocol_manager.stats.tx_error_count);
    BoardLog_Info("  build_error_count  = %lu\r\n", g_protocol_manager.stats.build_error_count);
    BoardLog_Info("  last_rx_type       = 0x%02X\r\n", g_protocol_manager.stats.last_rx_type);
    BoardLog_Info("  last_rx_flags      = 0x%02X\r\n", g_protocol_manager.stats.last_rx_flags);
    BoardLog_Info("  last_rx_seq        = 0x%02X\r\n", g_protocol_manager.stats.last_rx_seq);
    BoardLog_Info("  last_rx_cmd        = 0x%02X\r\n", g_protocol_manager.stats.last_rx_cmd);
    BoardLog_Info("  last_rx_len        = %u\r\n", g_protocol_manager.stats.last_rx_payload_len);
    BoardLog_Info("  last_tx_type       = 0x%02X\r\n", g_protocol_manager.stats.last_tx_type);
    BoardLog_Info("  last_tx_cmd        = 0x%02X\r\n", g_protocol_manager.stats.last_tx_cmd);

    if (parser_stats != NULL)
    {
        BoardLog_Info("ProtocolFrame Parser Stats:\r\n");
        BoardLog_Info("  input_bytes        = %lu\r\n", parser_stats->input_bytes);
        BoardLog_Info("  frame_ok_count     = %lu\r\n", parser_stats->frame_ok_count);
        BoardLog_Info("  frame_ready_count  = %lu\r\n", parser_stats->frame_ready_count);
        BoardLog_Info("  sof1_error_count   = %lu\r\n", parser_stats->sof1_error_count);
        BoardLog_Info("  sof2_error_count   = %lu\r\n", parser_stats->sof2_error_count);
        BoardLog_Info("  version_error      = %lu\r\n", parser_stats->version_error_count);
        BoardLog_Info("  type_error         = %lu\r\n", parser_stats->type_error_count);
        BoardLog_Info("  len_error          = %lu\r\n", parser_stats->len_error_count);
        BoardLog_Info("  crc_error          = %lu\r\n", parser_stats->crc_error_count);
        BoardLog_Info("  busy_drop          = %lu\r\n", parser_stats->busy_drop_count);
        BoardLog_Info("  reset_count        = %lu\r\n", parser_stats->reset_count);
    }
}

static void ProtocolManager_ProcessRx(void)
{
    uint16_t read_len;
    uint16_t i;
    uint32_t consumed_this_round = 0U;
    int ret;

    while (consumed_this_round < PROTOCOL_MANAGER_MAX_BYTES_PER_PROCESS)
    {
        read_len = PlatformUart_ReadRx(g_protocol_manager.rx_buf,
                                       sizeof(g_protocol_manager.rx_buf));

        if (read_len == 0U)
        {
            break;
        }

        for (i = 0U; i < read_len; i++)
        {
            ret = ProtocolFrameParser_InputByte(&g_protocol_manager.parser,
                                                g_protocol_manager.rx_buf[i],
                                                PlatformTime_GetMs());

            g_protocol_manager.stats.rx_bytes_consumed++;
            consumed_this_round++;

            if (ret != PROTO_FRAME_RESULT_OK)
            {
                g_protocol_manager.stats.parser_error_count++;
            }

            if (ProtocolFrameParser_HasFrame(&g_protocol_manager.parser) != 0U)
            {
                ret = ProtocolFrameParser_GetFrame(&g_protocol_manager.parser,
                                                   &g_protocol_manager.rx_frame,
                                                   PlatformTime_GetMs());

                if (ret == PROTO_FRAME_RESULT_OK)
                {
                    g_protocol_manager.stats.frame_received_count++;
                    ProtocolManager_UpdateLastRx(&g_protocol_manager.rx_frame);
                    ProtocolManager_HandleFrame(&g_protocol_manager.rx_frame);
                }
                else
                {
                    g_protocol_manager.stats.parser_error_count++;
                }
            }

            if (consumed_this_round >= PROTOCOL_MANAGER_MAX_BYTES_PER_PROCESS)
            {
                break;
            }
        }
    }
}

static void ProtocolManager_ProcessPendingEvents(void)
{
    int ret;
    uint8_t seq;

    ret = CommandManager_TryGetPendingEvent(&g_protocol_manager.event_record);

    if (ret != COMMAND_MANAGER_OK)
    {
        return;
    }

    seq = g_protocol_manager.event_seq;
    g_protocol_manager.event_seq++;

    ret = ProtocolManager_SendFrame(PROTO_FRAME_TYPE_EVENT,
                                    0U,
                                    seq,
                                    g_protocol_manager.event_record.event_id,
                                    g_protocol_manager.event_record.payload,
                                    g_protocol_manager.event_record.payload_len);

    if (ret == PROTOCOL_MANAGER_OK)
    {
        g_protocol_manager.stats.event_sent_count++;
        g_protocol_manager.stats.event_frame_count++;
    }
}

static void ProtocolManager_HandleFrame(const ProtocolFrame_t *frame)
{
    if (frame == NULL)
    {
        return;
    }

#if PROTOCOL_MANAGER_ENABLE_FRAME_LOG
    ProtocolManager_LogFrame("RX", frame);
#endif

    switch (frame->type)
    {
        case PROTO_FRAME_TYPE_REQ:
            g_protocol_manager.stats.req_frame_count++;
            ProtocolManager_HandleReq(frame);
            break;

        case PROTO_FRAME_TYPE_RESP:
            g_protocol_manager.stats.resp_frame_count++;
            break;

        case PROTO_FRAME_TYPE_NACK:
            g_protocol_manager.stats.nack_frame_count++;
            break;

        case PROTO_FRAME_TYPE_EVENT:
            g_protocol_manager.stats.event_frame_count++;
            break;

        default:
            g_protocol_manager.stats.other_frame_count++;
            break;
    }
}

static void ProtocolManager_HandleReq(const ProtocolFrame_t *frame)
{
    int ret;

    if (frame == NULL)
    {
        return;
    }

    memset(&g_protocol_manager.cmd_resp, 0, sizeof(g_protocol_manager.cmd_resp));

    ret = CommandManager_Dispatch(frame, &g_protocol_manager.cmd_resp);

    if ((ret != COMMAND_MANAGER_OK) &&
        (g_protocol_manager.cmd_resp.frame_type == 0U))
    {
        g_protocol_manager.cmd_resp.frame_type = PROTO_FRAME_TYPE_NACK;
        g_protocol_manager.cmd_resp.cmd = frame->cmd;
        g_protocol_manager.cmd_resp.error_code = PROTO_ERROR_INTERNAL_ERROR;
        g_protocol_manager.cmd_resp.payload[0] = PROTO_ERROR_INTERNAL_ERROR;
        g_protocol_manager.cmd_resp.payload[1] = frame->cmd;
        g_protocol_manager.cmd_resp.payload_len = 2U;
    }

    (void)ProtocolManager_SendFrame(g_protocol_manager.cmd_resp.frame_type,
                                    0U,
                                    frame->seq,
                                    g_protocol_manager.cmd_resp.cmd,
                                    g_protocol_manager.cmd_resp.payload,
                                    g_protocol_manager.cmd_resp.payload_len);
}

static int ProtocolManager_SendFrame(uint8_t type,
                                     uint8_t flags,
                                     uint8_t seq,
                                     uint8_t cmd,
                                     const uint8_t *payload,
                                     uint16_t payload_len)
{
    uint16_t tx_len = 0U;
    int ret;

    ret = ProtocolFrame_Build(type,
                              flags,
                              seq,
                              cmd,
                              payload,
                              payload_len,
                              g_protocol_manager.tx_buf,
                              sizeof(g_protocol_manager.tx_buf),
                              &tx_len);

    if (ret != PROTO_FRAME_RESULT_OK)
    {
        g_protocol_manager.stats.build_error_count++;
        g_protocol_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;
        return PROTOCOL_MANAGER_ERROR;
    }

#if PROTOCOL_MANAGER_ENABLE_FRAME_LOG
    {
        ProtocolFrame_t log_frame;

        memset(&log_frame, 0, sizeof(log_frame));

        log_frame.type = type;
        log_frame.flags = flags;
        log_frame.seq = seq;
        log_frame.cmd = cmd;
        log_frame.payload_len = payload_len;

        if ((payload != NULL) && (payload_len > 0U))
        {
            uint16_t copy_len = payload_len;

            if (copy_len > PROTO_FRAME_MAX_PAYLOAD_SIZE)
            {
                copy_len = PROTO_FRAME_MAX_PAYLOAD_SIZE;
            }

            memcpy(log_frame.payload, payload, copy_len);
        }

        ProtocolManager_LogFrame("TX", &log_frame);
    }
#endif

    ret = PlatformUart_SendBuffer(g_protocol_manager.tx_buf, tx_len);

    if (ret != PLATFORM_UART_OK)
    {
        g_protocol_manager.stats.tx_error_count++;
        g_protocol_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;
        return PROTOCOL_MANAGER_TX_ERROR;
    }

    g_protocol_manager.stats.frame_sent_count++;

    if (type == PROTO_FRAME_TYPE_RESP)
    {
        g_protocol_manager.stats.resp_sent_count++;
    }
    else if (type == PROTO_FRAME_TYPE_NACK)
    {
        g_protocol_manager.stats.nack_sent_count++;
    }

    g_protocol_manager.stats.last_tx_type = type;
    g_protocol_manager.stats.last_tx_cmd = cmd;

    return PROTOCOL_MANAGER_OK;
}

static void ProtocolManager_UpdateLastRx(const ProtocolFrame_t *frame)
{
    if (frame == NULL)
    {
        return;
    }

    g_protocol_manager.stats.last_rx_type = frame->type;
    g_protocol_manager.stats.last_rx_flags = frame->flags;
    g_protocol_manager.stats.last_rx_seq = frame->seq;
    g_protocol_manager.stats.last_rx_cmd = frame->cmd;
    g_protocol_manager.stats.last_rx_payload_len = frame->payload_len;
}

static void ProtocolManager_LogFrame(const char *prefix,
                                     const ProtocolFrame_t *frame)
{
    uint16_t i;

    if ((prefix == NULL) || (frame == NULL))
    {
        return;
    }

    BoardLog_Info("[PROTO_%s] type=0x%02X flags=0x%02X seq=%u cmd=0x%02X len=%u\r\n",
                  prefix,
                  frame->type,
                  frame->flags,
                  frame->seq,
                  frame->cmd,
                  frame->payload_len);

    if (frame->payload_len > 0U)
    {
        BoardLog_Info("[PROTO_%s] payload=", prefix);

        for (i = 0U; i < frame->payload_len; i++)
        {
            printf("%02X ", frame->payload[i]);
        }

        printf("\r\n");
    }
}