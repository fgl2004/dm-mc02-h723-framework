#ifndef DATA_ROUTER_H
#define DATA_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "protocol_frame.h"

/*
 * DataRouter
 *
 * RX-side semantic router for TYPE=DATA / ACK / WINDOW_ACK frames.
 * ProtocolManager parses frames; DataRouter routes them by frame->cmd.
 */

typedef enum
{
    DATA_ROUTER_OK = 0,
    DATA_ROUTER_ERROR = -1,
    DATA_ROUTER_INVALID_PARAM = -2,
    DATA_ROUTER_UNKNOWN_CMD = -3,
    DATA_ROUTER_NOT_INITIALIZED = -4
} DataRouterResult_t;

typedef struct
{
    uint32_t init_count;

    uint32_t data_frame_count;
    uint32_t ack_frame_count;
    uint32_t window_ack_frame_count;

    uint32_t stream_data_count;
    uint32_t block_data_count;
    uint32_t block_ack_count;
    uint32_t block_window_ack_count;

    uint32_t unknown_data_cmd_count;
    uint32_t unknown_ack_cmd_count;
    uint32_t unknown_window_ack_cmd_count;

    uint32_t invalid_param_count;
    uint32_t not_initialized_count;
    uint32_t route_error_count;

    uint8_t last_type;
    uint8_t last_cmd;
    int last_error;
} DataRouterStats_t;

void DataRouter_Init(void);

int DataRouter_HandleData(const ProtocolFrame_t *frame);
int DataRouter_HandleAck(const ProtocolFrame_t *frame);
int DataRouter_HandleWindowAck(const ProtocolFrame_t *frame);

const DataRouterStats_t *DataRouter_GetStats(void);
void DataRouter_ResetStats(void);
void DataRouter_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_ROUTER_H */
