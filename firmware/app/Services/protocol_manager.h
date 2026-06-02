#ifndef PROTOCOL_MANAGER_H
#define PROTOCOL_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "protocol_frame.h"

typedef enum
{
    PROTOCOL_MANAGER_OK = 0,
    PROTOCOL_MANAGER_ERROR = -1,
    PROTOCOL_MANAGER_INVALID_PARAM = -2,
    PROTOCOL_MANAGER_NOT_INITIALIZED = -3,
    PROTOCOL_MANAGER_TX_ERROR = -4
} ProtocolManagerResult_t;

typedef struct
{
    uint32_t init_count;
    uint32_t process_count;

    uint32_t rx_bytes_consumed;

    uint32_t frame_received_count;
    uint32_t frame_sent_count;

    uint32_t req_frame_count;
    uint32_t resp_frame_count;
    uint32_t nack_frame_count;
    uint32_t other_frame_count;

    uint32_t ping_count;
    uint32_t get_version_count;
    uint32_t get_status_count;
    uint32_t unknown_cmd_count;

    uint32_t parser_error_count;
    uint32_t tx_error_count;
    uint32_t build_error_count;

    uint8_t last_rx_type;
    uint8_t last_rx_flags;
    uint8_t last_rx_seq;
    uint8_t last_rx_cmd;
    uint16_t last_rx_payload_len;

    uint8_t last_error;
} ProtocolManagerStats_t;

void ProtocolManager_Init(void);
void ProtocolManager_Process(void);

const ProtocolManagerStats_t *ProtocolManager_GetStats(void);
void ProtocolManager_ResetStats(void);
void ProtocolManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_MANAGER_H */