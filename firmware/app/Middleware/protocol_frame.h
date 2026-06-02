#ifndef PROTOCOL_FRAME_H
#define PROTOCOL_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "state_machine.h"

#define PROTO_FRAME_SOF1                       0xA5U
#define PROTO_FRAME_SOF2                       0x5AU
#define PROTO_FRAME_VERSION                    0x01U

#define PROTO_FRAME_HEADER_SIZE                7U
#define PROTO_FRAME_MIN_SIZE                   11U
#define PROTO_FRAME_MAX_PAYLOAD_SIZE           128U
#define PROTO_FRAME_MAX_SIZE                   (2U + PROTO_FRAME_HEADER_SIZE + PROTO_FRAME_MAX_PAYLOAD_SIZE + 2U)

#define PROTO_FRAME_CRC_SIZE                   2U

typedef enum
{
    PROTO_FRAME_TYPE_REQ        = 0x01U,
    PROTO_FRAME_TYPE_RESP       = 0x02U,
    PROTO_FRAME_TYPE_ACK        = 0x03U,
    PROTO_FRAME_TYPE_NACK       = 0x04U,
    PROTO_FRAME_TYPE_EVENT      = 0x05U,
    PROTO_FRAME_TYPE_DATA       = 0x06U,
    PROTO_FRAME_TYPE_WINDOW_ACK = 0x07U
} ProtocolFrameType_t;

typedef enum
{
    PROTO_FRAME_FLAG_ACK_REQ       = (1U << 0),
    PROTO_FRAME_FLAG_IS_RETRY      = (1U << 1),
    PROTO_FRAME_FLAG_MORE_FRAG     = (1U << 2),
    PROTO_FRAME_FLAG_ENCRYPTED     = (1U << 3),
    PROTO_FRAME_FLAG_AUTH_REQUIRED = (1U << 4)
} ProtocolFrameFlag_t;

typedef enum
{
    PROTO_CMD_PING           = 0x01U,
    PROTO_CMD_GET_VERSION    = 0x02U,
    PROTO_CMD_GET_STATUS     = 0x03U,
    PROTO_CMD_GET_RESET_INFO = 0x04U,
    PROTO_CMD_GET_TIME_INFO  = 0x05U,

    PROTO_CMD_GET_FAULT_INFO = 0x10U,

    PROTO_CMD_PARAM_GET      = 0x20U,
    PROTO_CMD_PARAM_SET      = 0x21U,

    PROTO_CMD_ENTER_BOOTLOADER = 0x30U,
    PROTO_CMD_FW_TRANSFER      = 0x31U,

    PROTO_CMD_SECURITY_CHALLENGE = 0x40U,
    PROTO_CMD_SECURITY_AUTH      = 0x41U
} ProtocolCommandId_t;

typedef enum
{
    PROTO_ERROR_OK                  = 0x00U,
    PROTO_ERROR_UNKNOWN_CMD         = 0x01U,
    PROTO_ERROR_INVALID_LEN         = 0x02U,
    PROTO_ERROR_CRC_ERROR           = 0x03U,
    PROTO_ERROR_INVALID_STATE       = 0x04U,
    PROTO_ERROR_BUSY                = 0x05U,
    PROTO_ERROR_INTERNAL_ERROR      = 0x06U,
    PROTO_ERROR_AUTH_REQUIRED       = 0x07U,
    PROTO_ERROR_TIMEOUT             = 0x08U,
    PROTO_ERROR_INVALID_PARAM       = 0x09U,
    PROTO_ERROR_WINDOW_FULL         = 0x0AU,
    PROTO_ERROR_DUPLICATE_FRAME     = 0x0BU,
    PROTO_ERROR_UNSUPPORTED_VERSION = 0x0CU
} ProtocolErrorCode_t;

typedef enum
{
    PROTO_FRAME_RESULT_OK = 0,
    PROTO_FRAME_RESULT_ERROR = -1,
    PROTO_FRAME_RESULT_INVALID_PARAM = -2,
    PROTO_FRAME_RESULT_BUFFER_TOO_SMALL = -3,
    PROTO_FRAME_RESULT_PAYLOAD_TOO_LARGE = -4,
    PROTO_FRAME_RESULT_NO_FRAME = -5
} ProtocolFrameResult_t;

typedef enum
{
    PROTO_FRAME_STATE_WAIT_SOF1 = 1U,
    PROTO_FRAME_STATE_WAIT_SOF2,
    PROTO_FRAME_STATE_READ_HEADER,
    PROTO_FRAME_STATE_READ_PAYLOAD,
    PROTO_FRAME_STATE_READ_CRC,
    PROTO_FRAME_STATE_VERIFY_CRC,
    PROTO_FRAME_STATE_FRAME_READY,
    PROTO_FRAME_STATE_ERROR_RECOVERY
} ProtocolFrameState_t;

typedef enum
{
    PROTO_FRAME_EVT_BYTE = 1U,
    PROTO_FRAME_EVT_RESET
} ProtocolFrameEvent_t;

typedef struct
{
    uint8_t type;
    uint8_t flags;
    uint8_t seq;
    uint8_t cmd;
    uint16_t payload_len;
    uint8_t payload[PROTO_FRAME_MAX_PAYLOAD_SIZE];
} ProtocolFrame_t;

typedef struct
{
    uint32_t input_bytes;

    uint32_t frame_ok_count;
    uint32_t frame_ready_count;

    uint32_t sof1_error_count;
    uint32_t sof2_error_count;
    uint32_t version_error_count;
    uint32_t type_error_count;
    uint32_t len_error_count;
    uint32_t crc_error_count;

    uint32_t busy_drop_count;
    uint32_t reset_count;
} ProtocolFrameParserStats_t;

typedef struct
{
    StateMachine_t sm;

    uint8_t header[PROTO_FRAME_HEADER_SIZE];
    uint8_t header_index;

    uint8_t payload[PROTO_FRAME_MAX_PAYLOAD_SIZE];
    uint16_t payload_len;
    uint16_t payload_index;

    uint8_t crc_buf[PROTO_FRAME_CRC_SIZE];
    uint8_t crc_index;

    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t seq;
    uint8_t cmd;

    uint8_t frame_ready;
    ProtocolFrame_t ready_frame;

    ProtocolFrameParserStats_t stats;
} ProtocolFrameParser_t;

int ProtocolFrameParser_Init(ProtocolFrameParser_t *parser, uint32_t now_ms);
int ProtocolFrameParser_Reset(ProtocolFrameParser_t *parser, uint32_t now_ms);

int ProtocolFrameParser_InputByte(ProtocolFrameParser_t *parser,
                                  uint8_t byte,
                                  uint32_t now_ms);

uint8_t ProtocolFrameParser_HasFrame(const ProtocolFrameParser_t *parser);

int ProtocolFrameParser_GetFrame(ProtocolFrameParser_t *parser,
                                 ProtocolFrame_t *frame,
                                 uint32_t now_ms);

const ProtocolFrameParserStats_t *ProtocolFrameParser_GetStats(const ProtocolFrameParser_t *parser);

int ProtocolFrame_Build(uint8_t type,
                        uint8_t flags,
                        uint8_t seq,
                        uint8_t cmd,
                        const uint8_t *payload,
                        uint16_t payload_len,
                        uint8_t *out_buf,
                        uint16_t out_buf_size,
                        uint16_t *out_len);

int ProtocolFrame_RunSelfTest(void);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_FRAME_H */