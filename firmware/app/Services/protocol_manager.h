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
    PROTOCOL_MANAGER_TX_ERROR = -4,
    PROTOCOL_MANAGER_TX_QUEUE_FULL = -5,
    PROTOCOL_MANAGER_TX_BUSY = -6
} ProtocolManagerResult_t;

typedef enum
{
    PROTOCOL_TX_PRIORITY_HIGH = 0,
    PROTOCOL_TX_PRIORITY_NORMAL = 1,
    PROTOCOL_TX_PRIORITY_LOW = 2,
    PROTOCOL_TX_PRIORITY_COUNT = 3
} ProtocolTxPriority_t;

typedef struct
{
    uint32_t init_count;
    uint32_t process_count;

    uint32_t rx_bytes_consumed;

    uint32_t frame_received_count;
    uint32_t frame_sent_count;
    uint32_t event_sent_count;
    uint32_t resp_sent_count;
    uint32_t nack_sent_count;
    uint32_t ack_sent_count;
    uint32_t data_sent_count;
    uint32_t window_ack_sent_count;

    uint32_t req_frame_count;
    uint32_t resp_frame_count;
    uint32_t nack_frame_count;
    uint32_t event_frame_count;
    uint32_t data_frame_count;
    uint32_t ack_frame_count;
    uint32_t window_ack_frame_count;
    uint32_t other_frame_count;

    uint32_t parser_error_count;
    uint32_t tx_error_count;
    uint32_t build_error_count;

    uint32_t tx_enqueue_count;
    uint32_t tx_dequeue_count;
    uint32_t tx_queue_full_count;
    uint32_t tx_dma_start_count;
    uint32_t tx_dma_busy_skip_count;
    uint32_t tx_dma_start_error_count;

    uint32_t tx_high_enqueue_count;
    uint32_t tx_normal_enqueue_count;
    uint32_t tx_low_enqueue_count;

    uint32_t tx_high_drop_count;
    uint32_t tx_normal_drop_count;
    uint32_t tx_low_drop_count;

    uint16_t tx_high_depth;
    uint16_t tx_normal_depth;
    uint16_t tx_low_depth;
    uint16_t tx_high_high_watermark;
    uint16_t tx_normal_high_watermark;
    uint16_t tx_low_high_watermark;

    uint32_t pending_event_pop_count;
    uint32_t pending_event_no_event_count;

    uint32_t last_process_us;
    uint32_t max_process_us;

    uint8_t last_rx_type;
    uint8_t last_rx_flags;
    uint8_t last_rx_seq;
    uint8_t last_rx_cmd;
    uint16_t last_rx_payload_len;

    uint8_t last_tx_type;
    uint8_t last_tx_flags;
    uint8_t last_tx_seq;
    uint8_t last_tx_cmd;
    uint16_t last_tx_len;

    uint8_t last_error;
} ProtocolManagerStats_t;

void ProtocolManager_Init(void);
void ProtocolManager_Process(void);

int ProtocolManager_SendResp(uint8_t seq,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len);

int ProtocolManager_SendNack(uint8_t seq,
                             uint8_t cmd,
                             uint8_t error_code);

int ProtocolManager_SendEvent(uint8_t event_id,
                              const uint8_t *payload,
                              uint16_t payload_len,
                              ProtocolTxPriority_t priority);

int ProtocolManager_SendData(uint8_t flags,
                             uint8_t seq,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len,
                             ProtocolTxPriority_t priority);

int ProtocolManager_SendAck(uint8_t seq,
                            uint8_t cmd,
                            const uint8_t *payload,
                            uint16_t payload_len);

int ProtocolManager_SendWindowAck(uint8_t seq,
                                  uint8_t cmd,
                                  const uint8_t *payload,
                                  uint16_t payload_len);

const ProtocolManagerStats_t *ProtocolManager_GetStats(void);
const ProtocolFrameParserStats_t *ProtocolManager_GetParserStats(void);
void ProtocolManager_ResetStats(void);
void ProtocolManager_ResetParserStats(void);
void ProtocolManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_MANAGER_H */
