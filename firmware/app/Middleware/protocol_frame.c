#include "protocol_frame.h"

#include <stddef.h>
#include <string.h>

/*
 * IMPORTANT:
 *
 * This module uses the existing generic StateMachine framework.
 * It does not implement another independent state machine framework.
 *
 * CRC rule:
 *   CRC16 covers:
 *     VER + TYPE + FLAGS + SEQ + CMD + LEN + PAYLOAD
 *
 *   CRC16 does not cover:
 *     SOF1 + SOF2 + CRC16 itself
 *
 * Current CRC implementation below is CRC-16/CCITT-FALSE:
 *   poly    = 0x1021
 *   init    = 0xFFFF
 *   xorout  = 0x0000
 *   refin   = false
 *   refout  = false
 *
 * If your project already exposes a CRC16 middleware function,
 * you may replace ProtocolFrame_CalcCrc16() with that function call.
 */

typedef struct
{
    uint8_t byte;
    uint32_t now_ms;
} ProtocolFrameByteEvent_t;

static uint16_t ProtocolFrame_CalcCrc16(const uint8_t *data, uint16_t len);

static int ProtocolFrame_OnWaitSof1(void *ctx, EventId_t event, const void *event_data);
static int ProtocolFrame_OnWaitSof2(void *ctx, EventId_t event, const void *event_data);
static int ProtocolFrame_OnReadHeader(void *ctx, EventId_t event, const void *event_data);
static int ProtocolFrame_OnReadPayload(void *ctx, EventId_t event, const void *event_data);
static int ProtocolFrame_OnReadCrc(void *ctx, EventId_t event, const void *event_data);
static int ProtocolFrame_OnVerifyCrc(void *ctx, EventId_t event, const void *event_data);
static int ProtocolFrame_OnFrameReady(void *ctx, EventId_t event, const void *event_data);
static int ProtocolFrame_OnErrorRecovery(void *ctx, EventId_t event, const void *event_data);

static void ProtocolFrameParser_ClearWorkBuffer(ProtocolFrameParser_t *parser);
static int ProtocolFrameParser_ParseHeader(ProtocolFrameParser_t *parser);
static int ProtocolFrameParser_VerifyAndFinish(ProtocolFrameParser_t *parser, uint32_t now_ms);
static uint8_t ProtocolFrame_IsValidType(uint8_t type);
static void ProtocolFrameParser_SaveReadyFrame(ProtocolFrameParser_t *parser);
static void ProtocolFrameParser_ResetToWaitSof1(ProtocolFrameParser_t *parser, uint32_t now_ms);

static const StateDef_t g_protocol_frame_state_table[] =
{
    {
        PROTO_FRAME_STATE_WAIT_SOF1,
        NULL,
        NULL,
        ProtocolFrame_OnWaitSof1
    },
    {
        PROTO_FRAME_STATE_WAIT_SOF2,
        NULL,
        NULL,
        ProtocolFrame_OnWaitSof2
    },
    {
        PROTO_FRAME_STATE_READ_HEADER,
        NULL,
        NULL,
        ProtocolFrame_OnReadHeader
    },
    {
        PROTO_FRAME_STATE_READ_PAYLOAD,
        NULL,
        NULL,
        ProtocolFrame_OnReadPayload
    },
    {
        PROTO_FRAME_STATE_READ_CRC,
        NULL,
        NULL,
        ProtocolFrame_OnReadCrc
    },
    {
        PROTO_FRAME_STATE_VERIFY_CRC,
        NULL,
        NULL,
        ProtocolFrame_OnVerifyCrc
    },
    {
        PROTO_FRAME_STATE_FRAME_READY,
        NULL,
        NULL,
        ProtocolFrame_OnFrameReady
    },
    {
        PROTO_FRAME_STATE_ERROR_RECOVERY,
        NULL,
        NULL,
        ProtocolFrame_OnErrorRecovery
    }
};

int ProtocolFrameParser_Init(ProtocolFrameParser_t *parser, uint32_t now_ms)
{
    int ret;

    if (parser == NULL)
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    memset(parser, 0, sizeof(*parser));

    ret = StateMachine_Init(&parser->sm,
                            g_protocol_frame_state_table,
                            (uint16_t)(sizeof(g_protocol_frame_state_table) / sizeof(g_protocol_frame_state_table[0])),
                            PROTO_FRAME_STATE_WAIT_SOF1,
                            parser,
                            now_ms);

    if (ret != STATE_MACHINE_OK)
    {
        return PROTO_FRAME_RESULT_ERROR;
    }

    return PROTO_FRAME_RESULT_OK;
}

int ProtocolFrameParser_Reset(ProtocolFrameParser_t *parser, uint32_t now_ms)
{
    if (parser == NULL)
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    parser->stats.reset_count++;

    parser->header_index = 0U;
    parser->payload_len = 0U;
    parser->payload_index = 0U;
    parser->crc_index = 0U;
    parser->frame_ready = 0U;

    memset(parser->header, 0, sizeof(parser->header));
    memset(parser->payload, 0, sizeof(parser->payload));
    memset(parser->crc_buf, 0, sizeof(parser->crc_buf));
    memset(&parser->ready_frame, 0, sizeof(parser->ready_frame));

    (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_WAIT_SOF1, now_ms);

    return PROTO_FRAME_RESULT_OK;
}

int ProtocolFrameParser_InputByte(ProtocolFrameParser_t *parser,
                                  uint8_t byte,
                                  uint32_t now_ms)
{
    ProtocolFrameByteEvent_t byte_event;

    if (parser == NULL)
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    parser->stats.input_bytes++;

    if (parser->frame_ready != 0U)
    {
        /*
         * The upper layer should call ProtocolFrameParser_GetFrame()
         * before feeding the next byte.
         */
        parser->stats.busy_drop_count++;
        return PROTO_FRAME_RESULT_ERROR;
    }

    byte_event.byte = byte;
    byte_event.now_ms = now_ms;

    if (StateMachine_Dispatch(&parser->sm, PROTO_FRAME_EVT_BYTE, &byte_event) != STATE_MACHINE_OK)
    {
        return PROTO_FRAME_RESULT_ERROR;
    }

    return PROTO_FRAME_RESULT_OK;
}

uint8_t ProtocolFrameParser_HasFrame(const ProtocolFrameParser_t *parser)
{
    if (parser == NULL)
    {
        return 0U;
    }

    return parser->frame_ready;
}

int ProtocolFrameParser_GetFrame(ProtocolFrameParser_t *parser,
                                 ProtocolFrame_t *frame,
                                 uint32_t now_ms)
{
    if ((parser == NULL) || (frame == NULL))
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    if (parser->frame_ready == 0U)
    {
        return PROTO_FRAME_RESULT_NO_FRAME;
    }

    memcpy(frame, &parser->ready_frame, sizeof(ProtocolFrame_t));

    parser->frame_ready = 0U;
    ProtocolFrameParser_ClearWorkBuffer(parser);

    (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_WAIT_SOF1, now_ms);

    return PROTO_FRAME_RESULT_OK;
}

const ProtocolFrameParserStats_t *ProtocolFrameParser_GetStats(const ProtocolFrameParser_t *parser)
{
    if (parser == NULL)
    {
        return NULL;
    }

    return &parser->stats;
}

int ProtocolFrame_Build(uint8_t type,
                        uint8_t flags,
                        uint8_t seq,
                        uint8_t cmd,
                        const uint8_t *payload,
                        uint16_t payload_len,
                        uint8_t *out_buf,
                        uint16_t out_buf_size,
                        uint16_t *out_len)
{
    uint16_t required_len;
    uint16_t crc_input_len;
    uint16_t crc;
    uint16_t index;

    if ((out_buf == NULL) || (out_len == NULL))
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    if ((payload_len > 0U) && (payload == NULL))
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    if (payload_len > PROTO_FRAME_MAX_PAYLOAD_SIZE)
    {
        return PROTO_FRAME_RESULT_PAYLOAD_TOO_LARGE;
    }

    required_len = (uint16_t)(2U + PROTO_FRAME_HEADER_SIZE + payload_len + 2U);

    if (out_buf_size < required_len)
    {
        return PROTO_FRAME_RESULT_BUFFER_TOO_SMALL;
    }

    index = 0U;

    out_buf[index++] = PROTO_FRAME_SOF1;
    out_buf[index++] = PROTO_FRAME_SOF2;

    out_buf[index++] = PROTO_FRAME_VERSION;
    out_buf[index++] = type;
    out_buf[index++] = flags;
    out_buf[index++] = seq;
    out_buf[index++] = cmd;
    out_buf[index++] = (uint8_t)(payload_len & 0xFFU);
    out_buf[index++] = (uint8_t)((payload_len >> 8U) & 0xFFU);

    if (payload_len > 0U)
    {
        memcpy(&out_buf[index], payload, payload_len);
        index = (uint16_t)(index + payload_len);
    }

    /*
     * CRC input starts at VER.
     * out_buf[0] = SOF1
     * out_buf[1] = SOF2
     * out_buf[2] = VER
     */
    crc_input_len = (uint16_t)(PROTO_FRAME_HEADER_SIZE + payload_len);
    crc = ProtocolFrame_CalcCrc16(&out_buf[2], crc_input_len);

    out_buf[index++] = (uint8_t)(crc & 0xFFU);
    out_buf[index++] = (uint8_t)((crc >> 8U) & 0xFFU);

    *out_len = index;

    return PROTO_FRAME_RESULT_OK;
}

static int ProtocolFrame_OnWaitSof1(void *ctx, EventId_t event, const void *event_data)
{
    ProtocolFrameParser_t *parser;
    const ProtocolFrameByteEvent_t *byte_event;
    uint8_t byte;

    if ((ctx == NULL) || (event_data == NULL))
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event != PROTO_FRAME_EVT_BYTE)
    {
        return STATE_MACHINE_OK;
    }

    parser = (ProtocolFrameParser_t *)ctx;
    byte_event = (const ProtocolFrameByteEvent_t *)event_data;
    byte = byte_event->byte;

    if (byte == PROTO_FRAME_SOF1)
    {
        ProtocolFrameParser_ClearWorkBuffer(parser);
        (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_WAIT_SOF2, byte_event->now_ms);
    }
    else
    {
        parser->stats.sof1_error_count++;
    }

    return STATE_MACHINE_OK;
}

static int ProtocolFrame_OnWaitSof2(void *ctx, EventId_t event, const void *event_data)
{
    ProtocolFrameParser_t *parser;
    const ProtocolFrameByteEvent_t *byte_event;
    uint8_t byte;

    if ((ctx == NULL) || (event_data == NULL))
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event != PROTO_FRAME_EVT_BYTE)
    {
        return STATE_MACHINE_OK;
    }

    parser = (ProtocolFrameParser_t *)ctx;
    byte_event = (const ProtocolFrameByteEvent_t *)event_data;
    byte = byte_event->byte;

    if (byte == PROTO_FRAME_SOF2)
    {
        parser->header_index = 0U;
        (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_READ_HEADER, byte_event->now_ms);
    }
    else if (byte == PROTO_FRAME_SOF1)
    {
        /*
         * Stay in WAIT_SOF2.
         * This handles stream like:
         *   A5 A5 5A ...
         */
        parser->stats.sof2_error_count++;
    }
    else
    {
        parser->stats.sof2_error_count++;
        (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_WAIT_SOF1, byte_event->now_ms);
    }

    return STATE_MACHINE_OK;
}

static int ProtocolFrame_OnReadHeader(void *ctx, EventId_t event, const void *event_data)
{
    ProtocolFrameParser_t *parser;
    const ProtocolFrameByteEvent_t *byte_event;
    int ret;

    if ((ctx == NULL) || (event_data == NULL))
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event != PROTO_FRAME_EVT_BYTE)
    {
        return STATE_MACHINE_OK;
    }

    parser = (ProtocolFrameParser_t *)ctx;
    byte_event = (const ProtocolFrameByteEvent_t *)event_data;

    if (parser->header_index >= PROTO_FRAME_HEADER_SIZE)
    {
        parser->stats.len_error_count++;
        ProtocolFrameParser_ResetToWaitSof1(parser, byte_event->now_ms);
        return STATE_MACHINE_ERROR;
    }

    parser->header[parser->header_index++] = byte_event->byte;

    if (parser->header_index >= PROTO_FRAME_HEADER_SIZE)
    {
        ret = ProtocolFrameParser_ParseHeader(parser);

        if (ret != PROTO_FRAME_RESULT_OK)
        {
            ProtocolFrameParser_ResetToWaitSof1(parser, byte_event->now_ms);
            return STATE_MACHINE_OK;
        }

        if (parser->payload_len > 0U)
        {
            parser->payload_index = 0U;
            (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_READ_PAYLOAD, byte_event->now_ms);
        }
        else
        {
            parser->crc_index = 0U;
            (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_READ_CRC, byte_event->now_ms);
        }
    }

    return STATE_MACHINE_OK;
}

static int ProtocolFrame_OnReadPayload(void *ctx, EventId_t event, const void *event_data)
{
    ProtocolFrameParser_t *parser;
    const ProtocolFrameByteEvent_t *byte_event;

    if ((ctx == NULL) || (event_data == NULL))
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event != PROTO_FRAME_EVT_BYTE)
    {
        return STATE_MACHINE_OK;
    }

    parser = (ProtocolFrameParser_t *)ctx;
    byte_event = (const ProtocolFrameByteEvent_t *)event_data;

    if (parser->payload_index >= PROTO_FRAME_MAX_PAYLOAD_SIZE)
    {
        parser->stats.len_error_count++;
        ProtocolFrameParser_ResetToWaitSof1(parser, byte_event->now_ms);
        return STATE_MACHINE_ERROR;
    }

    parser->payload[parser->payload_index++] = byte_event->byte;

    if (parser->payload_index >= parser->payload_len)
    {
        parser->crc_index = 0U;
        (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_READ_CRC, byte_event->now_ms);
    }

    return STATE_MACHINE_OK;
}

static int ProtocolFrame_OnReadCrc(void *ctx, EventId_t event, const void *event_data)
{
    ProtocolFrameParser_t *parser;
    const ProtocolFrameByteEvent_t *byte_event;

    if ((ctx == NULL) || (event_data == NULL))
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event != PROTO_FRAME_EVT_BYTE)
    {
        return STATE_MACHINE_OK;
    }

    parser = (ProtocolFrameParser_t *)ctx;
    byte_event = (const ProtocolFrameByteEvent_t *)event_data;

    if (parser->crc_index >= PROTO_FRAME_CRC_SIZE)
    {
        parser->stats.crc_error_count++;
        ProtocolFrameParser_ResetToWaitSof1(parser, byte_event->now_ms);
        return STATE_MACHINE_ERROR;
    }

    parser->crc_buf[parser->crc_index++] = byte_event->byte;

    if (parser->crc_index >= PROTO_FRAME_CRC_SIZE)
    {
        (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_VERIFY_CRC, byte_event->now_ms);

        (void)ProtocolFrameParser_VerifyAndFinish(parser, byte_event->now_ms);
    }

    return STATE_MACHINE_OK;
}

static int ProtocolFrame_OnVerifyCrc(void *ctx, EventId_t event, const void *event_data)
{
    (void)ctx;
    (void)event;
    (void)event_data;

    return STATE_MACHINE_OK;
}

static int ProtocolFrame_OnFrameReady(void *ctx, EventId_t event, const void *event_data)
{
    (void)ctx;
    (void)event;
    (void)event_data;

    /*
     * The upper layer must call ProtocolFrameParser_GetFrame()
     * before feeding more bytes.
     */
    return STATE_MACHINE_OK;
}

static int ProtocolFrame_OnErrorRecovery(void *ctx, EventId_t event, const void *event_data)
{
    ProtocolFrameParser_t *parser;
    const ProtocolFrameByteEvent_t *byte_event;

    if ((ctx == NULL) || (event_data == NULL))
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    parser = (ProtocolFrameParser_t *)ctx;
    byte_event = (const ProtocolFrameByteEvent_t *)event_data;

    (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_WAIT_SOF1, byte_event->now_ms);

    return STATE_MACHINE_OK;
}

static void ProtocolFrameParser_ClearWorkBuffer(ProtocolFrameParser_t *parser)
{
    if (parser == NULL)
    {
        return;
    }

    parser->header_index = 0U;
    parser->payload_len = 0U;
    parser->payload_index = 0U;
    parser->crc_index = 0U;

    parser->version = 0U;
    parser->type = 0U;
    parser->flags = 0U;
    parser->seq = 0U;
    parser->cmd = 0U;

    memset(parser->header, 0, sizeof(parser->header));
    memset(parser->payload, 0, sizeof(parser->payload));
    memset(parser->crc_buf, 0, sizeof(parser->crc_buf));
}

static int ProtocolFrameParser_ParseHeader(ProtocolFrameParser_t *parser)
{
    if (parser == NULL)
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    parser->version = parser->header[0];
    parser->type = parser->header[1];
    parser->flags = parser->header[2];
    parser->seq = parser->header[3];
    parser->cmd = parser->header[4];
    parser->payload_len = (uint16_t)(((uint16_t)parser->header[6] << 8U) | parser->header[5]);

    if (parser->version != PROTO_FRAME_VERSION)
    {
        parser->stats.version_error_count++;
        return PROTO_FRAME_RESULT_ERROR;
    }

    if (ProtocolFrame_IsValidType(parser->type) == 0U)
    {
        parser->stats.type_error_count++;
        return PROTO_FRAME_RESULT_ERROR;
    }

    if (parser->payload_len > PROTO_FRAME_MAX_PAYLOAD_SIZE)
    {
        parser->stats.len_error_count++;
        return PROTO_FRAME_RESULT_PAYLOAD_TOO_LARGE;
    }

    return PROTO_FRAME_RESULT_OK;
}

static int ProtocolFrameParser_VerifyAndFinish(ProtocolFrameParser_t *parser, uint32_t now_ms)
{
    uint8_t crc_input[PROTO_FRAME_HEADER_SIZE + PROTO_FRAME_MAX_PAYLOAD_SIZE];
    uint16_t crc_input_len;
    uint16_t calc_crc;
    uint16_t recv_crc;

    if (parser == NULL)
    {
        return PROTO_FRAME_RESULT_INVALID_PARAM;
    }

    memcpy(crc_input, parser->header, PROTO_FRAME_HEADER_SIZE);

    if (parser->payload_len > 0U)
    {
        memcpy(&crc_input[PROTO_FRAME_HEADER_SIZE], parser->payload, parser->payload_len);
    }

    crc_input_len = (uint16_t)(PROTO_FRAME_HEADER_SIZE + parser->payload_len);
    calc_crc = ProtocolFrame_CalcCrc16(crc_input, crc_input_len);

    recv_crc = (uint16_t)(((uint16_t)parser->crc_buf[1] << 8U) | parser->crc_buf[0]);

    if (calc_crc != recv_crc)
    {
        parser->stats.crc_error_count++;
        ProtocolFrameParser_ResetToWaitSof1(parser, now_ms);
        return PROTO_FRAME_RESULT_ERROR;
    }

    ProtocolFrameParser_SaveReadyFrame(parser);

    parser->frame_ready = 1U;
    parser->stats.frame_ok_count++;
    parser->stats.frame_ready_count++;

    (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_FRAME_READY, now_ms);

    return PROTO_FRAME_RESULT_OK;
}

static uint8_t ProtocolFrame_IsValidType(uint8_t type)
{
    switch (type)
    {
        case PROTO_FRAME_TYPE_REQ:
        case PROTO_FRAME_TYPE_RESP:
        case PROTO_FRAME_TYPE_ACK:
        case PROTO_FRAME_TYPE_NACK:
        case PROTO_FRAME_TYPE_EVENT:
        case PROTO_FRAME_TYPE_DATA:
        case PROTO_FRAME_TYPE_WINDOW_ACK:
            return 1U;

        default:
            return 0U;
    }
}

static void ProtocolFrameParser_SaveReadyFrame(ProtocolFrameParser_t *parser)
{
    if (parser == NULL)
    {
        return;
    }

    memset(&parser->ready_frame, 0, sizeof(parser->ready_frame));

    parser->ready_frame.type = parser->type;
    parser->ready_frame.flags = parser->flags;
    parser->ready_frame.seq = parser->seq;
    parser->ready_frame.cmd = parser->cmd;
    parser->ready_frame.payload_len = parser->payload_len;

    if (parser->payload_len > 0U)
    {
        memcpy(parser->ready_frame.payload, parser->payload, parser->payload_len);
    }
}

static void ProtocolFrameParser_ResetToWaitSof1(ProtocolFrameParser_t *parser, uint32_t now_ms)
{
    if (parser == NULL)
    {
        return;
    }

    parser->frame_ready = 0U;
    ProtocolFrameParser_ClearWorkBuffer(parser);

    (void)StateMachine_Transition(&parser->sm, PROTO_FRAME_STATE_WAIT_SOF1, now_ms);
}

static uint16_t ProtocolFrame_CalcCrc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;

    if (data == NULL)
    {
        return crc;
    }

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8U);

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1U);
            }
        }
    }

    return crc;
}

/*
 * Self test.
 *
 * This function does not depend on UART.
 * It verifies:
 * 1. Build PING frame
 * 2. Parser can ignore garbage bytes
 * 3. Parser can parse valid frame
 * 4. Parser rejects CRC error frame
 * 5. Parser can parse zero-length payload
 */
int ProtocolFrame_RunSelfTest(void)
{
    ProtocolFrameParser_t parser;
    ProtocolFrame_t frame;
    uint8_t tx_buf[PROTO_FRAME_MAX_SIZE];
    uint16_t tx_len = 0U;
    uint16_t i;
    int ret;
    uint32_t now_ms = 0U;

    ret = ProtocolFrameParser_Init(&parser, now_ms);
    if (ret != PROTO_FRAME_RESULT_OK)
    {
        return -1;
    }

    ret = ProtocolFrame_Build(PROTO_FRAME_TYPE_REQ,
                              0U,
                              1U,
                              PROTO_CMD_PING,
                              NULL,
                              0U,
                              tx_buf,
                              sizeof(tx_buf),
                              &tx_len);

    if (ret != PROTO_FRAME_RESULT_OK)
    {
        return -2;
    }

    /*
     * Feed garbage before valid frame.
     */
    (void)ProtocolFrameParser_InputByte(&parser, 0x00U, now_ms++);
    (void)ProtocolFrameParser_InputByte(&parser, 0x11U, now_ms++);
    (void)ProtocolFrameParser_InputByte(&parser, 0x22U, now_ms++);

    for (i = 0U; i < tx_len; i++)
    {
        (void)ProtocolFrameParser_InputByte(&parser, tx_buf[i], now_ms++);

        if (ProtocolFrameParser_HasFrame(&parser) != 0U)
        {
            break;
        }
    }

    if (ProtocolFrameParser_HasFrame(&parser) == 0U)
    {
        return -3;
    }

    ret = ProtocolFrameParser_GetFrame(&parser, &frame, now_ms++);
    if (ret != PROTO_FRAME_RESULT_OK)
    {
        return -4;
    }

    if (frame.type != PROTO_FRAME_TYPE_REQ)
    {
        return -5;
    }

    if (frame.seq != 1U)
    {
        return -6;
    }

    if (frame.cmd != PROTO_CMD_PING)
    {
        return -7;
    }

    if (frame.payload_len != 0U)
    {
        return -8;
    }

    /*
     * CRC error test.
     */
    ret = ProtocolFrame_Build(PROTO_FRAME_TYPE_REQ,
                              0U,
                              2U,
                              PROTO_CMD_PING,
                              NULL,
                              0U,
                              tx_buf,
                              sizeof(tx_buf),
                              &tx_len);

    if (ret != PROTO_FRAME_RESULT_OK)
    {
        return -9;
    }

    if (tx_len < PROTO_FRAME_MIN_SIZE)
    {
        return -10;
    }

    tx_buf[tx_len - 1U] ^= 0x55U;

    for (i = 0U; i < tx_len; i++)
    {
        (void)ProtocolFrameParser_InputByte(&parser, tx_buf[i], now_ms++);
    }

    if (ProtocolFrameParser_HasFrame(&parser) != 0U)
    {
        return -11;
    }

    if (parser.stats.crc_error_count == 0U)
    {
        return -12;
    }

    return 0;
}