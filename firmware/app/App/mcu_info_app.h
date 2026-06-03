#ifndef MCU_INFO_APP_H
#define MCU_INFO_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "protocol_frame.h"
#include "platform_reset.h"
#include "command_service.h"

#define MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE      64U
#define MCU_INFO_APP_EVENT_QUEUE_SIZE            8U
#define MCU_INFO_APP_MAX_SNAPSHOT_PAYLOAD_SIZE   64U
#define MCU_INFO_APP_SNAPSHOT_SLOT_COUNT         8U

typedef enum
{
    MCU_INFO_APP_OK = 0,
    MCU_INFO_APP_ERROR = -1,
    MCU_INFO_APP_INVALID_PARAM = -2,
    MCU_INFO_APP_UNKNOWN_CMD = -3,
    MCU_INFO_APP_QUEUE_FULL = -4,
    MCU_INFO_APP_NOT_FOUND = -5
} McuInfoAppResult_t;

/*
 * Current Stage 2 basic commands.
 * These values are kept compatible with the current PC python tools.
 */
typedef enum
{
    MCU_INFO_CMD_PING           = SYS_CMD_PING,
    MCU_INFO_CMD_GET_VERSION    = SYS_CMD_GET_VERSION,
    MCU_INFO_CMD_GET_STATUS     = SYS_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_RESET_INFO = SYS_CMD_GET_RESET_INFO,
    MCU_INFO_CMD_GET_TIME_INFO  = SYS_CMD_GET_TIME_INFO,
    MCU_INFO_CMD_GET_FAULT_INFO = SYS_CMD_GET_FAULT_INFO,
    MCU_INFO_CMD_GET_UART_STATS = SYS_CMD_GET_UART_STATS,
    MCU_INFO_CMD_GET_APP_STATS  = SYS_CMD_GET_APP_STATS,
    MCU_INFO_CMD_GET_EVENT_STATS = SYS_CMD_GET_EVENT_STATS,
	  MCU_INFO_CMD_GET_COMMAND_STATS    =   CMD_GET_COMMAND_STATS
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
    MCU_INFO_APP_ID_USER   = 3U,
    MCU_INFO_APP_ID_RESET  = 4U
} McuInfoAppId_t;

typedef enum
{
    MCU_INFO_SNAPSHOT_SYSTEM_STATUS = 0x01U,
    MCU_INFO_SNAPSHOT_RESET_INFO    = 0x02U,
    MCU_INFO_SNAPSHOT_FAULT_INFO    = 0x03U,
    MCU_INFO_SNAPSHOT_UART_STATS    = 0x04U,
    MCU_INFO_SNAPSHOT_APP_STATS     = 0x05U,
    MCU_INFO_SNAPSHOT_CLOCK_INFO    = 0x06U,
    MCU_INFO_SNAPSHOT_BUILD_INFO    = 0x07U
} McuInfoSnapshotId_t;

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

    uint32_t register_command_count;
    uint32_t register_command_fail_count;

    uint32_t ping_count;
    uint32_t get_version_count;
    uint32_t get_status_count;
    uint32_t get_reset_info_count;
    uint32_t get_time_info_count;
    uint32_t get_fault_info_count;
    uint32_t get_uart_stats_count;
    uint32_t get_app_stats_count;
    uint32_t get_event_stats_count;

    uint32_t post_event_count;
    uint32_t event_forward_count;
    uint32_t event_drop_count;

    uint32_t update_snapshot_count;
    uint32_t get_snapshot_count;
    uint32_t update_reset_snapshot_count;
    uint32_t get_reset_snapshot_count;
    uint32_t update_status_count;

    uint32_t invalid_param_count;
    uint32_t not_found_count;
    uint32_t error_count;

    uint8_t last_cmd;
    uint8_t last_snapshot_id;
    uint8_t last_event_id;
    uint8_t last_error;
} McuInfoAppStats_t;

void McuInfoApp_Init(void);
void McuInfoApp_Run(void);

int McuInfoApp_RegisterCommand(uint8_t cmd,
                               CommandServiceHandler_t handler,
                               void *ctx,
                               const char *name,
                               uint32_t flags);

int McuInfoApp_PostEvent(uint8_t app_id,
                         uint8_t event_id,
                         const uint8_t *payload,
                         uint16_t payload_len);

int McuInfoApp_UpdateSnapshot(uint8_t snapshot_id,
                              const uint8_t *snapshot_data,
                              uint16_t snapshot_len);

int McuInfoApp_GetSnapshot(uint8_t snapshot_id,
                           uint8_t *out_buf,
                           uint16_t out_buf_size,
                           uint16_t *out_len);

int McuInfoApp_UpdateResetSnapshot(const PlatformResetInfo_t *reset_info);
int McuInfoApp_GetResetSnapshot(PlatformResetInfo_t *reset_info);

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
