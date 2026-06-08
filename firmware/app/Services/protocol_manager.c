#include "protocol_manager.h"

#include "command_manager.h"
#include "event_manager.h"

#include "platform_uart.h"
#include "platform_time.h"
#include "board_log.h"

#include "data_router.h"

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

#ifndef PROTOCOL_MANAGER_TX_QUEUE_DEPTH_HIGH
#define PROTOCOL_MANAGER_TX_QUEUE_DEPTH_HIGH        8U
#endif

#ifndef PROTOCOL_MANAGER_TX_QUEUE_DEPTH_NORMAL
#define PROTOCOL_MANAGER_TX_QUEUE_DEPTH_NORMAL      8U
#endif

#ifndef PROTOCOL_MANAGER_TX_QUEUE_DEPTH_LOW
#define PROTOCOL_MANAGER_TX_QUEUE_DEPTH_LOW         8U
#endif

typedef struct
{
    uint16_t len;
    uint8_t data[PROTO_FRAME_MAX_SIZE];

    uint8_t type;
    uint8_t flags;
    uint8_t seq;
    uint8_t cmd;
} ProtocolTxPacket_t;

typedef struct
{
    ProtocolTxPacket_t *items;
    uint16_t capacity;
    uint16_t read_index;
    uint16_t write_index;
    uint16_t used;
    uint16_t high_watermark;
} ProtocolTxQueue_t;

typedef struct
{
    uint8_t initialized;

    ProtocolFrameParser_t parser;
    ProtocolManagerStats_t stats;

    uint8_t rx_buf[PROTOCOL_MANAGER_RX_READ_CHUNK];
    ProtocolFrame_t rx_frame;
    CommandManagerResponse_t cmd_resp;
    EventManagerRecord_t event_record;

    uint8_t event_seq;

    ProtocolTxPacket_t tx_high_mem[PROTOCOL_MANAGER_TX_QUEUE_DEPTH_HIGH];
    ProtocolTxPacket_t tx_normal_mem[PROTOCOL_MANAGER_TX_QUEUE_DEPTH_NORMAL];
    ProtocolTxPacket_t tx_low_mem[PROTOCOL_MANAGER_TX_QUEUE_DEPTH_LOW];

    ProtocolTxQueue_t tx_queues[PROTOCOL_TX_PRIORITY_COUNT];

    /*
     * Persistent DMA source buffer.
     * Do not pass stack packet data to HAL_UART_Transmit_DMA().
     */
    ProtocolTxPacket_t tx_dma_packet;
} ProtocolManagerContext_t;

static ProtocolManagerContext_t g_protocol_manager;

static void ProtocolManager_ProcessRx(void);
static void ProtocolManager_ProcessTx(void);
static void ProtocolManager_ProcessPendingEvents(void);

static void ProtocolManager_HandleFrame(const ProtocolFrame_t *frame);
static void ProtocolManager_HandleReq(const ProtocolFrame_t *frame);

static int ProtocolManager_EnqueueFrame(uint8_t type,
                                        uint8_t flags,
                                        uint8_t seq,
                                        uint8_t cmd,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        ProtocolTxPriority_t priority);

static void ProtocolManager_TxQueueInit(ProtocolTxQueue_t *q,
                                        ProtocolTxPacket_t *items,
                                        uint16_t capacity);

static int ProtocolManager_TxQueuePush(ProtocolTxQueue_t *q,
                                       const ProtocolTxPacket_t *packet);

static int ProtocolManager_TxQueuePop(ProtocolTxQueue_t *q,
                                      ProtocolTxPacket_t *packet);

static ProtocolTxQueue_t *ProtocolManager_GetTxQueue(ProtocolTxPriority_t priority);
static int ProtocolManager_PopNextTxPacket(ProtocolTxPacket_t *packet);
static void ProtocolManager_UpdateTxQueueDepthStats(void);
static void ProtocolManager_UpdateLastRx(const ProtocolFrame_t *frame);
static void ProtocolManager_UpdateLastTx(const ProtocolTxPacket_t *packet);
static void ProtocolManager_LogFrame(const char *prefix,
                                     const ProtocolFrame_t *frame);

void ProtocolManager_Init(void)
{
    memset(&g_protocol_manager, 0, sizeof(g_protocol_manager));

    ProtocolManager_TxQueueInit(&g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_HIGH],
                                g_protocol_manager.tx_high_mem,
                                PROTOCOL_MANAGER_TX_QUEUE_DEPTH_HIGH);

    ProtocolManager_TxQueueInit(&g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_NORMAL],
                                g_protocol_manager.tx_normal_mem,
                                PROTOCOL_MANAGER_TX_QUEUE_DEPTH_NORMAL);

    ProtocolManager_TxQueueInit(&g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_LOW],
                                g_protocol_manager.tx_low_mem,
                                PROTOCOL_MANAGER_TX_QUEUE_DEPTH_LOW);

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

    /*
     * RX and TX are decoupled.
     *
     * RX:
     *   UART RX ring -> parser -> dispatch -> enqueue response/event/data.
     *
     * Event:
     *   EventManager pending queue -> ProtocolManager TX priority queue.
     *
     * TX:
     *   TX priority queue -> UART TX DMA.
     */
    ProtocolManager_ProcessRx();
    ProtocolManager_ProcessPendingEvents();
    ProtocolManager_ProcessTx();

    ProtocolManager_UpdateTxQueueDepthStats();

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_protocol_manager.stats.last_process_us = elapsed_us;

    if (elapsed_us > g_protocol_manager.stats.max_process_us)
    {
        g_protocol_manager.stats.max_process_us = elapsed_us;
    }
}

int ProtocolManager_SendResp(uint8_t seq,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len)
{
    return ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_RESP,
                                        0U,
                                        seq,
                                        cmd,
                                        payload,
                                        payload_len,
                                        PROTOCOL_TX_PRIORITY_HIGH);
}

int ProtocolManager_SendNack(uint8_t seq,
                             uint8_t cmd,
                             uint8_t error_code)
{
    uint8_t payload[2];

    payload[0] = error_code;
    payload[1] = cmd;

    return ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_NACK,
                                        0U,
                                        seq,
                                        cmd,
                                        payload,
                                        (uint16_t)sizeof(payload),
                                        PROTOCOL_TX_PRIORITY_HIGH);
}

int ProtocolManager_SendEvent(uint8_t event_id,
                              const uint8_t *payload,
                              uint16_t payload_len,
                              ProtocolTxPriority_t priority)
{
    return ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_EVENT,
                                        0U,
                                        g_protocol_manager.event_seq++,
                                        event_id,
                                        payload,
                                        payload_len,
                                        priority);
}

int ProtocolManager_SendData(uint8_t flags,
                             uint8_t seq,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len,
                             ProtocolTxPriority_t priority)
{
    return ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_DATA,
                                        flags,
                                        seq,
                                        cmd,
                                        payload,
                                        payload_len,
                                        priority);
}

int ProtocolManager_SendAck(uint8_t seq,
                            uint8_t cmd,
                            const uint8_t *payload,
                            uint16_t payload_len)
{
    return ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_ACK,
                                        0U,
                                        seq,
                                        cmd,
                                        payload,
                                        payload_len,
                                        PROTOCOL_TX_PRIORITY_HIGH);
}

int ProtocolManager_SendWindowAck(uint8_t seq,
                                  uint8_t cmd,
                                  const uint8_t *payload,
                                  uint16_t payload_len)
{
    return ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_WINDOW_ACK,
                                        0U,
                                        seq,
                                        cmd,
                                        payload,
                                        payload_len,
                                        PROTOCOL_TX_PRIORITY_HIGH);
}

const ProtocolManagerStats_t *ProtocolManager_GetStats(void)
{
    ProtocolManager_UpdateTxQueueDepthStats();
    return &g_protocol_manager.stats;
}

const ProtocolFrameParserStats_t *ProtocolManager_GetParserStats(void)
{
    return ProtocolFrameParser_GetStats(&g_protocol_manager.parser);
}

void ProtocolManager_ResetStats(void)
{
    memset(&g_protocol_manager.stats, 0, sizeof(g_protocol_manager.stats));
    ProtocolManager_UpdateTxQueueDepthStats();
}

void ProtocolManager_ResetParserStats(void)
{
    memset(&g_protocol_manager.parser.stats, 0, sizeof(g_protocol_manager.parser.stats));
}

void ProtocolManager_PrintStats(void)
{
    const ProtocolFrameParserStats_t *parser_stats;
    const PlatformUartStats_t *uart_stats;

    ProtocolManager_UpdateTxQueueDepthStats();

    parser_stats = ProtocolFrameParser_GetStats(&g_protocol_manager.parser);
    uart_stats = PlatformUart_GetStats();

    BoardLog_PrintSeparator();

    BoardLog_Info("ProtocolManager Stats:\r\n");
    BoardLog_Info("  initialized          = %u\r\n", g_protocol_manager.initialized);
    BoardLog_Info("  init_count           = %lu\r\n", g_protocol_manager.stats.init_count);
    BoardLog_Info("  process_count        = %lu\r\n", g_protocol_manager.stats.process_count);
    BoardLog_Info("  last_process_us      = %lu\r\n", g_protocol_manager.stats.last_process_us);
    BoardLog_Info("  max_process_us       = %lu\r\n", g_protocol_manager.stats.max_process_us);
    BoardLog_Info("  rx_bytes_consumed    = %lu\r\n", g_protocol_manager.stats.rx_bytes_consumed);
    BoardLog_Info("  frame_received       = %lu\r\n", g_protocol_manager.stats.frame_received_count);
    BoardLog_Info("  frame_sent           = %lu\r\n", g_protocol_manager.stats.frame_sent_count);
    BoardLog_Info("  resp_sent            = %lu\r\n", g_protocol_manager.stats.resp_sent_count);
    BoardLog_Info("  nack_sent            = %lu\r\n", g_protocol_manager.stats.nack_sent_count);
    BoardLog_Info("  ack_sent             = %lu\r\n", g_protocol_manager.stats.ack_sent_count);
    BoardLog_Info("  event_sent           = %lu\r\n", g_protocol_manager.stats.event_sent_count);
    BoardLog_Info("  data_sent            = %lu\r\n", g_protocol_manager.stats.data_sent_count);
    BoardLog_Info("  window_ack_sent      = %lu\r\n", g_protocol_manager.stats.window_ack_sent_count);

    BoardLog_Info("  req_frame_count      = %lu\r\n", g_protocol_manager.stats.req_frame_count);
    BoardLog_Info("  resp_frame_count     = %lu\r\n", g_protocol_manager.stats.resp_frame_count);
    BoardLog_Info("  nack_frame_count     = %lu\r\n", g_protocol_manager.stats.nack_frame_count);
    BoardLog_Info("  event_frame_count    = %lu\r\n", g_protocol_manager.stats.event_frame_count);
    BoardLog_Info("  data_frame_count     = %lu\r\n", g_protocol_manager.stats.data_frame_count);
    BoardLog_Info("  ack_frame_count      = %lu\r\n", g_protocol_manager.stats.ack_frame_count);
    BoardLog_Info("  window_ack_count     = %lu\r\n", g_protocol_manager.stats.window_ack_frame_count);
    BoardLog_Info("  other_frame_count    = %lu\r\n", g_protocol_manager.stats.other_frame_count);

    BoardLog_Info("  parser_error_count   = %lu\r\n", g_protocol_manager.stats.parser_error_count);
    BoardLog_Info("  tx_error_count       = %lu\r\n", g_protocol_manager.stats.tx_error_count);
    BoardLog_Info("  build_error_count    = %lu\r\n", g_protocol_manager.stats.build_error_count);

    BoardLog_Info("  tx_enqueue_count     = %lu\r\n", g_protocol_manager.stats.tx_enqueue_count);
    BoardLog_Info("  tx_dequeue_count     = %lu\r\n", g_protocol_manager.stats.tx_dequeue_count);
    BoardLog_Info("  tx_queue_full_count  = %lu\r\n", g_protocol_manager.stats.tx_queue_full_count);
    BoardLog_Info("  tx_dma_start_count   = %lu\r\n", g_protocol_manager.stats.tx_dma_start_count);
    BoardLog_Info("  tx_dma_busy_skip     = %lu\r\n", g_protocol_manager.stats.tx_dma_busy_skip_count);
    BoardLog_Info("  tx_dma_start_error   = %lu\r\n", g_protocol_manager.stats.tx_dma_start_error_count);

    BoardLog_Info("  tx_high_depth        = %u\r\n", g_protocol_manager.stats.tx_high_depth);
    BoardLog_Info("  tx_normal_depth      = %u\r\n", g_protocol_manager.stats.tx_normal_depth);
    BoardLog_Info("  tx_low_depth         = %u\r\n", g_protocol_manager.stats.tx_low_depth);
    BoardLog_Info("  tx_high_highwater    = %u\r\n", g_protocol_manager.stats.tx_high_high_watermark);
    BoardLog_Info("  tx_normal_highwater  = %u\r\n", g_protocol_manager.stats.tx_normal_high_watermark);
    BoardLog_Info("  tx_low_highwater     = %u\r\n", g_protocol_manager.stats.tx_low_high_watermark);

    BoardLog_Info("  pending_event_pop    = %lu\r\n", g_protocol_manager.stats.pending_event_pop_count);
    BoardLog_Info("  pending_event_empty  = %lu\r\n", g_protocol_manager.stats.pending_event_no_event_count);

    BoardLog_Info("  last_rx_type         = 0x%02X\r\n", g_protocol_manager.stats.last_rx_type);
    BoardLog_Info("  last_rx_flags        = 0x%02X\r\n", g_protocol_manager.stats.last_rx_flags);
    BoardLog_Info("  last_rx_seq          = 0x%02X\r\n", g_protocol_manager.stats.last_rx_seq);
    BoardLog_Info("  last_rx_cmd          = 0x%02X\r\n", g_protocol_manager.stats.last_rx_cmd);
    BoardLog_Info("  last_rx_len          = %u\r\n", g_protocol_manager.stats.last_rx_payload_len);
    BoardLog_Info("  last_tx_type         = 0x%02X\r\n", g_protocol_manager.stats.last_tx_type);
    BoardLog_Info("  last_tx_flags        = 0x%02X\r\n", g_protocol_manager.stats.last_tx_flags);
    BoardLog_Info("  last_tx_seq          = 0x%02X\r\n", g_protocol_manager.stats.last_tx_seq);
    BoardLog_Info("  last_tx_cmd          = 0x%02X\r\n", g_protocol_manager.stats.last_tx_cmd);
    BoardLog_Info("  last_tx_len          = %u\r\n", g_protocol_manager.stats.last_tx_len);

    if (uart_stats != 0)
    {
        BoardLog_Info("Platform UART TX Summary:\r\n");
        BoardLog_Info("  uart_tx_dma_start   = %lu\r\n", uart_stats->tx_dma_start_count);
        BoardLog_Info("  uart_tx_dma_done    = %lu\r\n", uart_stats->tx_dma_done_count);
        BoardLog_Info("  uart_tx_dma_error   = %lu\r\n", uart_stats->tx_dma_error_count);
        BoardLog_Info("  uart_tx_busy        = %u\r\n", uart_stats->tx_busy);
    }

    if (parser_stats != 0)
    {
        BoardLog_Info("ProtocolFrame Parser Stats:\r\n");
        BoardLog_Info("  input_bytes          = %lu\r\n", parser_stats->input_bytes);
        BoardLog_Info("  frame_ok_count       = %lu\r\n", parser_stats->frame_ok_count);
        BoardLog_Info("  frame_ready_count    = %lu\r\n", parser_stats->frame_ready_count);
        BoardLog_Info("  sof1_error_count     = %lu\r\n", parser_stats->sof1_error_count);
        BoardLog_Info("  sof2_error_count     = %lu\r\n", parser_stats->sof2_error_count);
        BoardLog_Info("  version_error        = %lu\r\n", parser_stats->version_error_count);
        BoardLog_Info("  type_error           = %lu\r\n", parser_stats->type_error_count);
        BoardLog_Info("  len_error            = %lu\r\n", parser_stats->len_error_count);
        BoardLog_Info("  crc_error            = %lu\r\n", parser_stats->crc_error_count);
        BoardLog_Info("  busy_drop            = %lu\r\n", parser_stats->busy_drop_count);
        BoardLog_Info("  reset_count          = %lu\r\n", parser_stats->reset_count);
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

            /*
             * Important:
             * Get the ready frame immediately.
             * Otherwise the parser rejects following bytes while frame_ready=1.
             */
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

static void ProtocolManager_ProcessTx(void)
{
    int ret;

    if (PlatformUart_IsTxBusy() != 0U)
    {
        g_protocol_manager.stats.tx_dma_busy_skip_count++;
        return;
    }

    ret = ProtocolManager_PopNextTxPacket(&g_protocol_manager.tx_dma_packet);
    if (ret != PROTOCOL_MANAGER_OK)
    {
        return;
    }

    ret = PlatformUart_SendBufferDma(g_protocol_manager.tx_dma_packet.data,
                                     g_protocol_manager.tx_dma_packet.len);
    if (ret != PLATFORM_UART_OK)
    {
        g_protocol_manager.stats.tx_error_count++;
        g_protocol_manager.stats.tx_dma_start_error_count++;
        g_protocol_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;
        return;
    }

    g_protocol_manager.stats.tx_dma_start_count++;
    g_protocol_manager.stats.frame_sent_count++;
    ProtocolManager_UpdateLastTx(&g_protocol_manager.tx_dma_packet);

    switch (g_protocol_manager.tx_dma_packet.type)
    {
        case PROTO_FRAME_TYPE_RESP:
            g_protocol_manager.stats.resp_sent_count++;
            break;

        case PROTO_FRAME_TYPE_NACK:
            g_protocol_manager.stats.nack_sent_count++;
            break;

        case PROTO_FRAME_TYPE_ACK:
            g_protocol_manager.stats.ack_sent_count++;
            break;

        case PROTO_FRAME_TYPE_EVENT:
            g_protocol_manager.stats.event_sent_count++;
            break;

        case PROTO_FRAME_TYPE_DATA:
            g_protocol_manager.stats.data_sent_count++;
            break;

        case PROTO_FRAME_TYPE_WINDOW_ACK:
            g_protocol_manager.stats.window_ack_sent_count++;
            break;

        default:
            break;
    }
}

static void ProtocolManager_ProcessPendingEvents(void)
{
    int ret;
    ProtocolTxPriority_t tx_priority;

    ret = EventManager_TryGetPendingEvent(&g_protocol_manager.event_record);

    if (ret == EVENT_MANAGER_NO_EVENT)
    {
        g_protocol_manager.stats.pending_event_no_event_count++;
        return;
    }

    if (ret != EVENT_MANAGER_OK)
    {
        return;
    }

    g_protocol_manager.stats.pending_event_pop_count++;

    if (g_protocol_manager.event_record.priority == EVENT_MANAGER_PRIORITY_HIGH)
    {
        tx_priority = PROTOCOL_TX_PRIORITY_HIGH;
    }
    else
    {
        tx_priority = PROTOCOL_TX_PRIORITY_NORMAL;
    }

    (void)ProtocolManager_SendEvent(g_protocol_manager.event_record.event_id,
                                    g_protocol_manager.event_record.payload,
                                    g_protocol_manager.event_record.payload_len,
                                    tx_priority);
}

static void ProtocolManager_HandleFrame(const ProtocolFrame_t *frame)
{
    if (frame == 0)
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

        case PROTO_FRAME_TYPE_DATA:
            g_protocol_manager.stats.data_frame_count++;
            (void)DataRouter_HandleData(frame);
            break;

        case PROTO_FRAME_TYPE_ACK:
            g_protocol_manager.stats.ack_frame_count++;
            (void)DataRouter_HandleAck(frame);
            break;

        case PROTO_FRAME_TYPE_WINDOW_ACK:
            g_protocol_manager.stats.window_ack_frame_count++;
            (void)DataRouter_HandleWindowAck(frame);
            break;

        default:
            g_protocol_manager.stats.other_frame_count++;
            break;
    }
}

static void ProtocolManager_HandleReq(const ProtocolFrame_t *frame)
{
    int ret;

    if (frame == 0)
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

    if (g_protocol_manager.cmd_resp.frame_type == PROTO_FRAME_TYPE_RESP)
    {
        (void)ProtocolManager_SendResp(frame->seq,
                                       g_protocol_manager.cmd_resp.cmd,
                                       g_protocol_manager.cmd_resp.payload,
                                       g_protocol_manager.cmd_resp.payload_len);
    }
    else if (g_protocol_manager.cmd_resp.frame_type == PROTO_FRAME_TYPE_NACK)
    {
        if (g_protocol_manager.cmd_resp.payload_len > 0U)
        {
            (void)ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_NACK,
                                               0U,
                                               frame->seq,
                                               g_protocol_manager.cmd_resp.cmd,
                                               g_protocol_manager.cmd_resp.payload,
                                               g_protocol_manager.cmd_resp.payload_len,
                                               PROTOCOL_TX_PRIORITY_HIGH);
        }
        else
        {
            (void)ProtocolManager_SendNack(frame->seq,
                                           g_protocol_manager.cmd_resp.cmd,
                                           g_protocol_manager.cmd_resp.error_code);
        }
    }
}

static int ProtocolManager_EnqueueFrame(uint8_t type,
                                        uint8_t flags,
                                        uint8_t seq,
                                        uint8_t cmd,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        ProtocolTxPriority_t priority)
{
    ProtocolTxPacket_t packet;
    ProtocolTxQueue_t *q;
    uint16_t tx_len = 0U;
    int ret;

    if (g_protocol_manager.initialized == 0U)
    {
        return PROTOCOL_MANAGER_NOT_INITIALIZED;
    }

    q = ProtocolManager_GetTxQueue(priority);
    if (q == 0)
    {
        return PROTOCOL_MANAGER_INVALID_PARAM;
    }

    memset(&packet, 0, sizeof(packet));

    ret = ProtocolFrame_Build(type,
                              flags,
                              seq,
                              cmd,
                              payload,
                              payload_len,
                              packet.data,
                              sizeof(packet.data),
                              &tx_len);

    if (ret != PROTO_FRAME_RESULT_OK)
    {
        g_protocol_manager.stats.build_error_count++;
        g_protocol_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;
        return PROTOCOL_MANAGER_ERROR;
    }

    packet.len = tx_len;
    packet.type = type;
    packet.flags = flags;
    packet.seq = seq;
    packet.cmd = cmd;

#if PROTOCOL_MANAGER_ENABLE_FRAME_LOG
    {
        ProtocolFrame_t log_frame;

        memset(&log_frame, 0, sizeof(log_frame));

        log_frame.type = type;
        log_frame.flags = flags;
        log_frame.seq = seq;
        log_frame.cmd = cmd;
        log_frame.payload_len = payload_len;

        if ((payload != 0) && (payload_len > 0U))
        {
            uint16_t copy_len = payload_len;

            if (copy_len > PROTO_FRAME_MAX_PAYLOAD_SIZE)
            {
                copy_len = PROTO_FRAME_MAX_PAYLOAD_SIZE;
            }

            memcpy(log_frame.payload, payload, copy_len);
        }

        ProtocolManager_LogFrame("TXQ", &log_frame);
    }
#endif

    ret = ProtocolManager_TxQueuePush(q, &packet);
    if (ret != PROTOCOL_MANAGER_OK)
    {
        g_protocol_manager.stats.tx_queue_full_count++;
        g_protocol_manager.stats.last_error = PROTO_ERROR_BUSY;

        if (priority == PROTOCOL_TX_PRIORITY_HIGH)
        {
            g_protocol_manager.stats.tx_high_drop_count++;
        }
        else if (priority == PROTOCOL_TX_PRIORITY_NORMAL)
        {
            g_protocol_manager.stats.tx_normal_drop_count++;
        }
        else
        {
            g_protocol_manager.stats.tx_low_drop_count++;
        }

        return PROTOCOL_MANAGER_TX_QUEUE_FULL;
    }

    g_protocol_manager.stats.tx_enqueue_count++;

    if (priority == PROTOCOL_TX_PRIORITY_HIGH)
    {
        g_protocol_manager.stats.tx_high_enqueue_count++;
    }
    else if (priority == PROTOCOL_TX_PRIORITY_NORMAL)
    {
        g_protocol_manager.stats.tx_normal_enqueue_count++;
    }
    else
    {
        g_protocol_manager.stats.tx_low_enqueue_count++;
    }

    ProtocolManager_UpdateTxQueueDepthStats();

    return PROTOCOL_MANAGER_OK;
}

static void ProtocolManager_TxQueueInit(ProtocolTxQueue_t *q,
                                        ProtocolTxPacket_t *items,
                                        uint16_t capacity)
{
    if ((q == 0) || (items == 0) || (capacity == 0U))
    {
        return;
    }

    q->items = items;
    q->capacity = capacity;
    q->read_index = 0U;
    q->write_index = 0U;
    q->used = 0U;
    q->high_watermark = 0U;
}

static int ProtocolManager_TxQueuePush(ProtocolTxQueue_t *q,
                                       const ProtocolTxPacket_t *packet)
{
    if ((q == 0) || (q->items == 0) || (packet == 0) || (q->capacity == 0U))
    {
        return PROTOCOL_MANAGER_INVALID_PARAM;
    }

    if (q->used >= q->capacity)
    {
        return PROTOCOL_MANAGER_TX_QUEUE_FULL;
    }

    q->items[q->write_index] = *packet;

    q->write_index++;
    if (q->write_index >= q->capacity)
    {
        q->write_index = 0U;
    }

    q->used++;

    if (q->used > q->high_watermark)
    {
        q->high_watermark = q->used;
    }

    return PROTOCOL_MANAGER_OK;
}

static int ProtocolManager_TxQueuePop(ProtocolTxQueue_t *q,
                                      ProtocolTxPacket_t *packet)
{
    if ((q == 0) || (q->items == 0) || (packet == 0) || (q->capacity == 0U))
    {
        return PROTOCOL_MANAGER_INVALID_PARAM;
    }

    if (q->used == 0U)
    {
        return PROTOCOL_MANAGER_ERROR;
    }

    *packet = q->items[q->read_index];

    q->read_index++;
    if (q->read_index >= q->capacity)
    {
        q->read_index = 0U;
    }

    q->used--;

    return PROTOCOL_MANAGER_OK;
}

static ProtocolTxQueue_t *ProtocolManager_GetTxQueue(ProtocolTxPriority_t priority)
{
    if ((uint32_t)priority >= (uint32_t)PROTOCOL_TX_PRIORITY_COUNT)
    {
        return 0;
    }

    return &g_protocol_manager.tx_queues[(uint32_t)priority];
}

static int ProtocolManager_PopNextTxPacket(ProtocolTxPacket_t *packet)
{
    int ret;

    ret = ProtocolManager_TxQueuePop(&g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_HIGH],
                                     packet);
    if (ret == PROTOCOL_MANAGER_OK)
    {
        g_protocol_manager.stats.tx_dequeue_count++;
        return PROTOCOL_MANAGER_OK;
    }

    ret = ProtocolManager_TxQueuePop(&g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_NORMAL],
                                     packet);
    if (ret == PROTOCOL_MANAGER_OK)
    {
        g_protocol_manager.stats.tx_dequeue_count++;
        return PROTOCOL_MANAGER_OK;
    }

    ret = ProtocolManager_TxQueuePop(&g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_LOW],
                                     packet);
    if (ret == PROTOCOL_MANAGER_OK)
    {
        g_protocol_manager.stats.tx_dequeue_count++;
        return PROTOCOL_MANAGER_OK;
    }

    return PROTOCOL_MANAGER_ERROR;
}

static void ProtocolManager_UpdateTxQueueDepthStats(void)
{
    ProtocolTxQueue_t *high_q;
    ProtocolTxQueue_t *normal_q;
    ProtocolTxQueue_t *low_q;

    high_q = &g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_HIGH];
    normal_q = &g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_NORMAL];
    low_q = &g_protocol_manager.tx_queues[PROTOCOL_TX_PRIORITY_LOW];

    g_protocol_manager.stats.tx_high_depth = high_q->used;
    g_protocol_manager.stats.tx_normal_depth = normal_q->used;
    g_protocol_manager.stats.tx_low_depth = low_q->used;

    g_protocol_manager.stats.tx_high_high_watermark = high_q->high_watermark;
    g_protocol_manager.stats.tx_normal_high_watermark = normal_q->high_watermark;
    g_protocol_manager.stats.tx_low_high_watermark = low_q->high_watermark;
}

static void ProtocolManager_UpdateLastRx(const ProtocolFrame_t *frame)
{
    if (frame == 0)
    {
        return;
    }

    g_protocol_manager.stats.last_rx_type = frame->type;
    g_protocol_manager.stats.last_rx_flags = frame->flags;
    g_protocol_manager.stats.last_rx_seq = frame->seq;
    g_protocol_manager.stats.last_rx_cmd = frame->cmd;
    g_protocol_manager.stats.last_rx_payload_len = frame->payload_len;
}

static void ProtocolManager_UpdateLastTx(const ProtocolTxPacket_t *packet)
{
    if (packet == 0)
    {
        return;
    }

    g_protocol_manager.stats.last_tx_type = packet->type;
    g_protocol_manager.stats.last_tx_flags = packet->flags;
    g_protocol_manager.stats.last_tx_seq = packet->seq;
    g_protocol_manager.stats.last_tx_cmd = packet->cmd;
    g_protocol_manager.stats.last_tx_len = packet->len;
}

static void ProtocolManager_LogFrame(const char *prefix,
                                     const ProtocolFrame_t *frame)
{
    uint16_t i;

    if ((prefix == 0) || (frame == 0))
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
