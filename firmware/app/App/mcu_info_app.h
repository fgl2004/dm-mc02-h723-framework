#ifndef MCU_INFO_APP_H
#define MCU_INFO_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "protocol_frame.h"

#define MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE      64U
#define MCU_INFO_APP_EVENT_QUEUE_SIZE            8U
#define MCU_INFO_APP_MAX_SNAPSHOT_PAYLOAD_SIZE   64U

typedef enum
{
    MCU_INFO_APP_OK = 0,
    MCU_INFO_APP_ERROR = -1,
    MCU_INFO_APP_INVALID_PARAM = -2,
    MCU_INFO_APP_UNKNOWN_CMD = -3,
    MCU_INFO_APP_QUEUE_FULL = -4
} McuInfoAppResult_t;

typedef enum
{
    MCU_INFO_CMD_PING           = 0x01U,
    MCU_INFO_CMD_GET_VERSION    = 0x02U,
    MCU_INFO_CMD_GET_STATUS     = 0x03U,
    MCU_INFO_CMD_GET_RESET_INFO = 0x04U,
    MCU_INFO_CMD_GET_TIME_INFO  = 0x05U,
    MCU_INFO_CMD_GET_FAULT_INFO = 0x06U,
    MCU_INFO_CMD_GET_UART_STATS = 0x07U,
    MCU_INFO_CMD_GET_APP_STATS  = 0x08U
} McuInfoCommandId_t;

typedef enum
{
    MCU_INFO_EVENT_BOOT             = 0x81U,
    MCU_INFO_EVENT_HEARTBEAT        = 0x82U,
    MCU_INFO_EVENT_RUNTIME_STATUS   = 0x83U,
    MCU_INFO_EVENT_UART_WARNING     = 0x84U,
    MCU_INFO_EVENT_APP_MESSAGE      = 0x85U,
    MCU_INFO_EVENT_FAULT            = 0x86U
} McuInfoEventId_t;

typedef enum
{
    MCU_INFO_APP_ID_SYSTEM = 0U,
    MCU_INFO_APP_ID_UART   = 1U,
    MCU_INFO_APP_ID_FAULT  = 2U,
    MCU_INFO_APP_ID_USER   = 3U
} McuInfoAppId_t;

typedef struct
{
    uint8_t frame_type;
    uint8_t cmd;
    uint8_t error_code;

    uint16_t payload_len;
    uint8_t payload[PROTO_FRAME_MAX_PAYLOAD_SIZE];
} McuInfoAppResponse_t;

typedef struct
{
    uint8_t app_id;
    uint8_t status;
    uint32_t value;
    uint32_t update_count;
} McuInfoRuntimeStatus_t;

typedef struct
{
    uint8_t app_id;
    uint8_t event_id;
    uint16_t payload_len;
    uint32_t tick_ms;
    uint8_t payload[MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE];
} McuInfoEventRecord_t;

typedef struct
{
    uint32_t init_count;
    uint32_t run_count;
    uint32_t dispatch_count;

    uint32_t ping_count;
    uint32_t get_version_count;
    uint32_t get_status_count;
    uint32_t get_reset_info_count;
    uint32_t get_time_info_count;
    uint32_t get_fault_info_count;
    uint32_t get_uart_stats_count;
    uint32_t get_app_stats_count;

    uint32_t post_event_count;
    uint32_t event_forward_count;
    uint32_t event_drop_count;
    uint32_t update_snapshot_count;
    uint32_t update_status_count;

    uint32_t unknown_cmd_count;
    uint32_t invalid_param_count;
    uint32_t error_count;

    uint8_t last_cmd;
    uint8_t last_event_id;
    uint8_t last_error;
} McuInfoAppStats_t;

void McuInfoApp_Init(void);
void McuInfoApp_Run(void);

int McuInfoApp_HandleCommand(const ProtocolFrame_t *req_frame,
                             McuInfoAppResponse_t *resp);

int McuInfoApp_PostEvent(uint8_t app_id,
                         uint8_t event_id,
                         const uint8_t *payload,
                         uint16_t payload_len);

int McuInfoApp_UpdateSnapshot(uint8_t app_id,
                              const uint8_t *snapshot_data,
                              uint16_t snapshot_len);

int McuInfoApp_UpdateRuntimeStatus(uint8_t app_id,
                                   uint8_t status,
                                   uint32_t value);

const McuInfoAppStats_t *McuInfoApp_GetStats(void);
void McuInfoApp_ResetStats(void);
void McuInfoApp_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* MCU_INFO_APP_H */