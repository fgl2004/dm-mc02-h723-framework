#include "mcu_info_app.h"

#include "command_manager.h"
#include "ring_buffer.h"

#include "platform_time.h"
#include "platform_uart.h"
#include "platform_reset.h"
#include "board_log.h"

#include <stdio.h>
#include <string.h>

#define MCU_INFO_APP_VERSION_STRING          "DM-MC02-H723,proto=1.0"
#define MCU_INFO_APP_STATUS_STRING           "OK"

#ifndef MCU_INFO_APP_EVENT_FORWARD_INTERVAL_MS
#define MCU_INFO_APP_EVENT_FORWARD_INTERVAL_MS  20U
#endif

#ifndef MCU_INFO_APP_ENABLE_BOOT_EVENT
#define MCU_INFO_APP_ENABLE_BOOT_EVENT          1U
#endif

typedef struct
{
    uint8_t valid;
    uint8_t snapshot_id;
    uint16_t snapshot_len;
    uint8_t snapshot_data[MCU_INFO_APP_MAX_SNAPSHOT_PAYLOAD_SIZE];
} McuInfoSnapshotSlot_t;

typedef struct
{
    uint8_t initialized;

    McuInfoAppStats_t stats;

    McuInfoRuntimeStatus_t runtime_status;

    uint8_t reset_snapshot_valid;
    PlatformResetInfo_t reset_snapshot;

    McuInfoSnapshotSlot_t snapshots[MCU_INFO_APP_SNAPSHOT_SLOT_COUNT];

    RingBuffer_t event_rb;
    uint8_t event_storage[MCU_INFO_APP_EVENT_QUEUE_SIZE * sizeof(McuInfoEventRecord_t)];

    uint32_t last_event_forward_ms;
} McuInfoAppContext_t;

static McuInfoAppContext_t g_mcu_info_app;

static int McuInfoApp_RegisterBuiltinCommands(void);

static int McuInfoApp_HandlePing(const ProtocolFrame_t *req_frame,
                                 CommandManagerResponse_t *resp,
                                 void *ctx);

static int McuInfoApp_HandleGetVersion(const ProtocolFrame_t *req_frame,
                                       CommandManagerResponse_t *resp,
                                       void *ctx);

static int McuInfoApp_HandleGetStatus(const ProtocolFrame_t *req_frame,
                                      CommandManagerResponse_t *resp,
                                      void *ctx);

static int McuInfoApp_HandleGetResetInfo(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx);

static int McuInfoApp_HandleGetTimeInfo(const ProtocolFrame_t *req_frame,
                                        CommandManagerResponse_t *resp,
                                        void *ctx);

static int McuInfoApp_HandleGetFaultInfo(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx);

static int McuInfoApp_HandleGetUartStats(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx);

static int McuInfoApp_HandleGetAppStats(const ProtocolFrame_t *req_frame,
                                        CommandManagerResponse_t *resp,
                                        void *ctx);

static int McuInfoApp_HandleGetEventStats(const ProtocolFrame_t *req_frame,
                                          CommandManagerResponse_t *resp,
                                          void *ctx);

static const char *McuInfoApp_ResetCauseToShortString(PlatformResetCause_t cause);

static void McuInfoApp_BuildResetInfoPayload(char *buf,
                                             uint16_t buf_size,
                                             const PlatformResetInfo_t *info);
static int McuInfoApp_HandleGetCommandStats(const ProtocolFrame_t *req,
                                            CommandManagerResponse_t *resp,
                                            void *ctx);
static int McuInfoApp_PopEvent(McuInfoEventRecord_t *event);
static void McuInfoApp_RunEventForwarder(void);

void McuInfoApp_Init(void)
{
    memset(&g_mcu_info_app, 0, sizeof(g_mcu_info_app));

    RingBuffer_Init(&g_mcu_info_app.event_rb,
                    g_mcu_info_app.event_storage,
                    (uint16_t)sizeof(g_mcu_info_app.event_storage));

    g_mcu_info_app.initialized = 1U;
    g_mcu_info_app.stats.init_count++;
    g_mcu_info_app.last_event_forward_ms = PlatformTime_GetMs();

    (void)McuInfoApp_RegisterBuiltinCommands();

    BoardLog_Info("McuInfoApp init OK\r\n");

#if MCU_INFO_APP_ENABLE_BOOT_EVENT
    {
        static const uint8_t boot_payload[] = { 'B', 'O', 'O', 'T' };

        (void)McuInfoApp_PostEvent(MCU_INFO_APP_ID_SYSTEM,
                                   MCU_INFO_EVENT_BOOT,
                                   boot_payload,
                                   (uint16_t)sizeof(boot_payload));
    }
#endif
}

void McuInfoApp_Run(void)
{
    uint32_t start_cycle;
    uint32_t elapsed_us;

    if (g_mcu_info_app.initialized == 0U)
    {
        return;
    }

    start_cycle = PlatformTime_ProfileStart();

    g_mcu_info_app.stats.run_count++;

    /*
     * McuInfoApp is the central information app.
     *
     * It does not send EVENT frames directly through ProtocolManager.
     * It forwards prepared event records to CommandManager, and
     * ProtocolManager later sends them as EVENT frames.
     */
    McuInfoApp_RunEventForwarder();

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_mcu_info_app.stats.last_run_us = elapsed_us;

    if (elapsed_us > g_mcu_info_app.stats.max_run_us)
    {
        g_mcu_info_app.stats.max_run_us = elapsed_us;
    }
}

int McuInfoApp_RegisterCommand(uint8_t cmd,
                               CommandServiceHandler_t handler,
                               void *ctx,
                               const char *name,
                               uint32_t flags)
{
    int ret;
    uint8_t category;

    if (handler == NULL)
    {
        g_mcu_info_app.stats.invalid_param_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return MCU_INFO_APP_INVALID_PARAM;
    }

    category = CommandService_GetCategoryByCmd(cmd);

    ret = CommandService_Register(cmd,
                                  category,
                                  flags,
                                  handler,
                                  ctx,
                                  name);

    if (ret == COMMAND_SERVICE_OK)
    {
        g_mcu_info_app.stats.register_command_count++;
        g_mcu_info_app.stats.last_cmd = cmd;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;
        return MCU_INFO_APP_OK;
    }

    g_mcu_info_app.stats.register_command_fail_count++;
    g_mcu_info_app.stats.last_cmd = cmd;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

    return MCU_INFO_APP_ERROR;
}

int McuInfoApp_PostEvent(uint8_t app_id,
                         uint8_t event_id,
                         const uint8_t *payload,
                         uint16_t payload_len)
{
    McuInfoEventRecord_t record;
    uint16_t copy_len;
    uint16_t written;

    if ((payload_len > 0U) && (payload == NULL))
    {
        g_mcu_info_app.stats.invalid_param_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return MCU_INFO_APP_INVALID_PARAM;
    }

    if (RingBuffer_Free(&g_mcu_info_app.event_rb) < (uint16_t)sizeof(McuInfoEventRecord_t))
    {
        g_mcu_info_app.stats.event_drop_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_BUSY;
        return MCU_INFO_APP_QUEUE_FULL;
    }

    memset(&record, 0, sizeof(record));

    record.app_id = app_id;
    record.event_id = event_id;
    record.tick_ms = PlatformTime_GetMs();

    copy_len = payload_len;
    if (copy_len > MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE)
    {
        copy_len = MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE;
    }

    if ((payload != NULL) && (copy_len > 0U))
    {
        memcpy(record.payload, payload, copy_len);
        record.payload_len = copy_len;
    }

    written = RingBuffer_Write(&g_mcu_info_app.event_rb,
                               (const uint8_t *)&record,
                               (uint16_t)sizeof(record));

    if (written != (uint16_t)sizeof(record))
    {
        g_mcu_info_app.stats.event_drop_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_BUSY;
        return MCU_INFO_APP_QUEUE_FULL;
    }

    g_mcu_info_app.stats.post_event_count++;
    g_mcu_info_app.stats.last_event_id = event_id;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    return MCU_INFO_APP_OK;
}

int McuInfoApp_UpdateSnapshot(uint8_t snapshot_id,
                              const uint8_t *snapshot_data,
                              uint16_t snapshot_len)
{
    uint8_t i;
    uint16_t copy_len;
    McuInfoSnapshotSlot_t *slot = NULL;

    if ((snapshot_len > 0U) && (snapshot_data == NULL))
    {
        g_mcu_info_app.stats.invalid_param_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return MCU_INFO_APP_INVALID_PARAM;
    }

    for (i = 0U; i < (uint8_t)(sizeof(g_mcu_info_app.snapshots) / sizeof(g_mcu_info_app.snapshots[0])); i++)
    {
        if ((g_mcu_info_app.snapshots[i].valid != 0U) &&
            (g_mcu_info_app.snapshots[i].snapshot_id == snapshot_id))
        {
            slot = &g_mcu_info_app.snapshots[i];
            break;
        }
    }

    if (slot == NULL)
    {
        for (i = 0U; i < (uint8_t)(sizeof(g_mcu_info_app.snapshots) / sizeof(g_mcu_info_app.snapshots[0])); i++)
        {
            if (g_mcu_info_app.snapshots[i].valid == 0U)
            {
                slot = &g_mcu_info_app.snapshots[i];
                break;
            }
        }
    }

    if (slot == NULL)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_BUSY;
        return MCU_INFO_APP_ERROR;
    }

    memset(slot, 0, sizeof(*slot));

    slot->valid = 1U;
    slot->snapshot_id = snapshot_id;

    copy_len = snapshot_len;
    if (copy_len > MCU_INFO_APP_MAX_SNAPSHOT_PAYLOAD_SIZE)
    {
        copy_len = MCU_INFO_APP_MAX_SNAPSHOT_PAYLOAD_SIZE;
    }

    if ((snapshot_data != NULL) && (copy_len > 0U))
    {
        memcpy(slot->snapshot_data, snapshot_data, copy_len);
        slot->snapshot_len = copy_len;
    }

    g_mcu_info_app.stats.update_snapshot_count++;
    g_mcu_info_app.stats.last_snapshot_id = snapshot_id;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    return MCU_INFO_APP_OK;
}

int McuInfoApp_GetSnapshot(uint8_t snapshot_id,
                           uint8_t *out_buf,
                           uint16_t out_buf_size,
                           uint16_t *out_len)
{
    uint8_t i;
    uint16_t copy_len;

    if ((out_buf == NULL) || (out_len == NULL))
    {
        g_mcu_info_app.stats.invalid_param_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return MCU_INFO_APP_INVALID_PARAM;
    }

    *out_len = 0U;

    for (i = 0U; i < (uint8_t)(sizeof(g_mcu_info_app.snapshots) / sizeof(g_mcu_info_app.snapshots[0])); i++)
    {
        if ((g_mcu_info_app.snapshots[i].valid != 0U) &&
            (g_mcu_info_app.snapshots[i].snapshot_id == snapshot_id))
        {
            copy_len = g_mcu_info_app.snapshots[i].snapshot_len;

            if (copy_len > out_buf_size)
            {
                copy_len = out_buf_size;
            }

            if (copy_len > 0U)
            {
                memcpy(out_buf,
                       g_mcu_info_app.snapshots[i].snapshot_data,
                       copy_len);
            }

            *out_len = copy_len;

            g_mcu_info_app.stats.get_snapshot_count++;
            g_mcu_info_app.stats.last_snapshot_id = snapshot_id;
            g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

            return MCU_INFO_APP_OK;
        }
    }

    g_mcu_info_app.stats.not_found_count++;
    g_mcu_info_app.stats.last_snapshot_id = snapshot_id;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;

    return MCU_INFO_APP_NOT_FOUND;
}

int McuInfoApp_UpdateResetSnapshot(const PlatformResetInfo_t *reset_info)
{
    if (reset_info == NULL)
    {
        g_mcu_info_app.stats.invalid_param_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return MCU_INFO_APP_INVALID_PARAM;
    }

    g_mcu_info_app.reset_snapshot = *reset_info;
    g_mcu_info_app.reset_snapshot_valid = 1U;

    g_mcu_info_app.stats.update_reset_snapshot_count++;
    g_mcu_info_app.stats.update_snapshot_count++;
    g_mcu_info_app.stats.last_snapshot_id = MCU_INFO_SNAPSHOT_RESET_INFO;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    return MCU_INFO_APP_OK;
}

int McuInfoApp_GetResetSnapshot(PlatformResetInfo_t *reset_info)
{
    if (reset_info == NULL)
    {
        g_mcu_info_app.stats.invalid_param_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return MCU_INFO_APP_INVALID_PARAM;
    }

    if (g_mcu_info_app.reset_snapshot_valid == 0U)
    {
        g_mcu_info_app.stats.not_found_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_STATE;
        return MCU_INFO_APP_NOT_FOUND;
    }

    *reset_info = g_mcu_info_app.reset_snapshot;

    g_mcu_info_app.stats.get_reset_snapshot_count++;
    g_mcu_info_app.stats.get_snapshot_count++;
    g_mcu_info_app.stats.last_snapshot_id = MCU_INFO_SNAPSHOT_RESET_INFO;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    return MCU_INFO_APP_OK;
}

int McuInfoApp_UpdateRuntimeStatus(uint8_t app_id,
                                   uint8_t status,
                                   uint32_t value)
{
    g_mcu_info_app.runtime_status.app_id = app_id;
    g_mcu_info_app.runtime_status.status = status;
    g_mcu_info_app.runtime_status.value = value;
    g_mcu_info_app.runtime_status.update_count++;

    g_mcu_info_app.stats.update_status_count++;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    return MCU_INFO_APP_OK;
}

const McuInfoAppStats_t *McuInfoApp_GetStats(void)
{
    return &g_mcu_info_app.stats;
}

void McuInfoApp_ResetStats(void)
{
    memset(&g_mcu_info_app.stats, 0, sizeof(g_mcu_info_app.stats));
}

void McuInfoApp_PrintStats(void)
{
    const RingBufferStats_t *rb_stats;

    rb_stats = RingBuffer_GetStats(&g_mcu_info_app.event_rb);

    BoardLog_PrintSeparator();

    BoardLog_Info("McuInfoApp Stats:\r\n");
    BoardLog_Info("  initialized              = %u\r\n", g_mcu_info_app.initialized);
    BoardLog_Info("  init_count               = %lu\r\n", g_mcu_info_app.stats.init_count);
    BoardLog_Info("  run_count                = %lu\r\n", g_mcu_info_app.stats.run_count);
    BoardLog_Info("  last_run_us              = %lu\r\n", g_mcu_info_app.stats.last_run_us);
    BoardLog_Info("  max_run_us               = %lu\r\n", g_mcu_info_app.stats.max_run_us);
    BoardLog_Info("  register_command_count   = %lu\r\n", g_mcu_info_app.stats.register_command_count);
    BoardLog_Info("  register_command_fail    = %lu\r\n", g_mcu_info_app.stats.register_command_fail_count);
    BoardLog_Info("  ping_count               = %lu\r\n", g_mcu_info_app.stats.ping_count);
    BoardLog_Info("  get_version_count        = %lu\r\n", g_mcu_info_app.stats.get_version_count);
    BoardLog_Info("  get_status_count         = %lu\r\n", g_mcu_info_app.stats.get_status_count);
    BoardLog_Info("  get_reset_info_count     = %lu\r\n", g_mcu_info_app.stats.get_reset_info_count);
    BoardLog_Info("  get_time_info_count      = %lu\r\n", g_mcu_info_app.stats.get_time_info_count);
    BoardLog_Info("  get_fault_info_count     = %lu\r\n", g_mcu_info_app.stats.get_fault_info_count);
    BoardLog_Info("  get_uart_stats_count     = %lu\r\n", g_mcu_info_app.stats.get_uart_stats_count);
    BoardLog_Info("  get_app_stats_count      = %lu\r\n", g_mcu_info_app.stats.get_app_stats_count);
    BoardLog_Info("  get_event_stats_count    = %lu\r\n", g_mcu_info_app.stats.get_event_stats_count);
    BoardLog_Info("  post_event_count         = %lu\r\n", g_mcu_info_app.stats.post_event_count);
    BoardLog_Info("  event_forward_count      = %lu\r\n", g_mcu_info_app.stats.event_forward_count);
    BoardLog_Info("  event_drop_count         = %lu\r\n", g_mcu_info_app.stats.event_drop_count);
    BoardLog_Info("  update_snapshot_count    = %lu\r\n", g_mcu_info_app.stats.update_snapshot_count);
    BoardLog_Info("  get_snapshot_count       = %lu\r\n", g_mcu_info_app.stats.get_snapshot_count);
    BoardLog_Info("  update_reset_snapshot    = %lu\r\n", g_mcu_info_app.stats.update_reset_snapshot_count);
    BoardLog_Info("  get_reset_snapshot       = %lu\r\n", g_mcu_info_app.stats.get_reset_snapshot_count);
    BoardLog_Info("  update_status_count      = %lu\r\n", g_mcu_info_app.stats.update_status_count);
    BoardLog_Info("  invalid_param_count      = %lu\r\n", g_mcu_info_app.stats.invalid_param_count);
    BoardLog_Info("  not_found_count          = %lu\r\n", g_mcu_info_app.stats.not_found_count);
    BoardLog_Info("  error_count              = %lu\r\n", g_mcu_info_app.stats.error_count);
    BoardLog_Info("  last_cmd                 = 0x%02X\r\n", g_mcu_info_app.stats.last_cmd);
    BoardLog_Info("  last_snapshot_id         = 0x%02X\r\n", g_mcu_info_app.stats.last_snapshot_id);
    BoardLog_Info("  last_event_id            = 0x%02X\r\n", g_mcu_info_app.stats.last_event_id);
    BoardLog_Info("  last_error               = 0x%02X\r\n", g_mcu_info_app.stats.last_error);
    BoardLog_Info("  reset_snapshot_valid     = %u\r\n", g_mcu_info_app.reset_snapshot_valid);
    BoardLog_Info("  event_available          = %u\r\n", RingBuffer_Available(&g_mcu_info_app.event_rb));

    if (rb_stats != NULL)
    {
        BoardLog_Info("  event_rb_write_bytes     = %lu\r\n", rb_stats->write_bytes);
        BoardLog_Info("  event_rb_read_bytes      = %lu\r\n", rb_stats->read_bytes);
        BoardLog_Info("  event_rb_overflow        = %lu\r\n", rb_stats->overflow_count);
        BoardLog_Info("  event_rb_high            = %u\r\n", rb_stats->high_watermark);
    }
}

static int McuInfoApp_RegisterBuiltinCommands(void)
{
    int ret = MCU_INFO_APP_OK;

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_PING,
                                   McuInfoApp_HandlePing,
                                   NULL,
                                   "PING",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_VERSION,
                                   McuInfoApp_HandleGetVersion,
                                   NULL,
                                   "GET_VERSION",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_STATUS,
                                   McuInfoApp_HandleGetStatus,
                                   NULL,
                                   "GET_STATUS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_RESET_INFO,
                                   McuInfoApp_HandleGetResetInfo,
                                   NULL,
                                   "GET_RESET_INFO",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_TIME_INFO,
                                   McuInfoApp_HandleGetTimeInfo,
                                   NULL,
                                   "GET_TIME_INFO",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_FAULT_INFO,
                                   McuInfoApp_HandleGetFaultInfo,
                                   NULL,
                                   "GET_FAULT_INFO",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_UART_STATS,
                                   McuInfoApp_HandleGetUartStats,
                                   NULL,
                                   "GET_UART_STATS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_APP_STATS,
                                   McuInfoApp_HandleGetAppStats,
                                   NULL,
                                   "GET_APP_STATS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }

    if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_EVENT_STATS,
                                   McuInfoApp_HandleGetEventStats,
                                   NULL,
                                   "GET_EVENT_STATS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        ret = MCU_INFO_APP_ERROR;
    }
		
		if (McuInfoApp_RegisterCommand(MCU_INFO_CMD_GET_COMMAND_STATS,
																			 McuInfoApp_HandleGetCommandStats,
																			 NULL,
																			 "GET_EVENT_STATS",
																			 CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
				{
						ret = MCU_INFO_APP_ERROR;
				}
    return ret;
}

static int McuInfoApp_HandlePing(const ProtocolFrame_t *req_frame,
                                 CommandManagerResponse_t *resp,
                                 void *ctx)
{
    static const uint8_t pong_payload[] = { 'P', 'O', 'N', 'G' };

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.ping_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           pong_payload,
                           (uint16_t)sizeof(pong_payload));

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetVersion(const ProtocolFrame_t *req_frame,
                                       CommandManagerResponse_t *resp,
                                       void *ctx)
{
    const char *version = MCU_INFO_APP_VERSION_STRING;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_version_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)version,
                           (uint16_t)strlen(version));

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetStatus(const ProtocolFrame_t *req_frame,
                                      CommandManagerResponse_t *resp,
                                      void *ctx)
{
    const char *status = MCU_INFO_APP_STATUS_STRING;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_status_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)status,
                           (uint16_t)strlen(status));

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetResetInfo(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx)
{
    PlatformResetInfo_t reset_info;
    char text_buf[128];

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_reset_info_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;

    if (McuInfoApp_GetResetSnapshot(&reset_info) != MCU_INFO_APP_OK)
    {
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_STATE;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INVALID_STATE);

        return COMMAND_SERVICE_ERROR;
    }

    McuInfoApp_BuildResetInfoPayload(text_buf,
                                     (uint16_t)sizeof(text_buf),
                                     &reset_info);

    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)text_buf,
                           (uint16_t)strlen(text_buf));

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetTimeInfo(const ProtocolFrame_t *req_frame,
                                        CommandManagerResponse_t *resp,
                                        void *ctx)
{
    char text_buf[32];
    uint32_t tick;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_time_info_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;

    tick = PlatformTime_GetMs();

    len = snprintf(text_buf,
                   sizeof(text_buf),
                   "tick=%lu",
                   (unsigned long)tick);

    if (len < 0)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)text_buf,
                           (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetFaultInfo(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx)
{
    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_fault_info_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;

    /*
     * Fault snapshot/event will be connected in the next step.
     */
    CommandService_SetNack(resp,
                           req_frame->cmd,
                           PROTO_ERROR_UNKNOWN_CMD);

    return COMMAND_SERVICE_UNKNOWN_CMD;
}

static int McuInfoApp_HandleGetUartStats(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx)
{
    char text_buf[96];
    PlatformUartRxSnapshot_t snapshot;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_uart_stats_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;

    PlatformUart_GetRxSnapshot(&snapshot);

    len = snprintf(text_buf,
                   sizeof(text_buf),
                   "rx=%lu,avail=%u,free=%u,err=%lu",
                   (unsigned long)snapshot.rx_bytes,
                   snapshot.rx_ring_available,
                   snapshot.rx_ring_free,
                   (unsigned long)snapshot.rx_error_count);

    if (len < 0)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)text_buf,
                           (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetAppStats(const ProtocolFrame_t *req_frame,
                                        CommandManagerResponse_t *resp,
                                        void *ctx)
{
    char text_buf[96];
    const McuInfoAppStats_t *stats;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_app_stats_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;

    stats = McuInfoApp_GetStats();

    if (stats == NULL)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    len = snprintf(text_buf,
                   sizeof(text_buf),
                   "run=%lu,post=%lu,fwd=%lu,drop=%lu",
                   (unsigned long)stats->run_count,
                   (unsigned long)stats->post_event_count,
                   (unsigned long)stats->event_forward_count,
                   (unsigned long)stats->event_drop_count);

    if (len < 0)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)text_buf,
                           (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetEventStats(const ProtocolFrame_t *req_frame,
                                          CommandManagerResponse_t *resp,
                                          void *ctx)
{
    char text_buf[96];
    const McuInfoAppStats_t *stats;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_mcu_info_app.stats.get_event_stats_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;

    stats = McuInfoApp_GetStats();

    if (stats == NULL)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    len = snprintf(text_buf,
                   sizeof(text_buf),
                   "post=%lu,fwd=%lu,drop=%lu,last=0x%02X",
                   (unsigned long)stats->post_event_count,
                   (unsigned long)stats->event_forward_count,
                   (unsigned long)stats->event_drop_count,
                   stats->last_event_id);

    if (len < 0)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)text_buf,
                           (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int McuInfoApp_HandleGetCommandStats(const ProtocolFrame_t *req_frame,
                                            CommandManagerResponse_t *resp,
                                            void *ctx)
{
    const CommandServiceStats_t *stats;
    char text_buf[96];
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    stats = CommandService_GetStats();

    if (stats == NULL)
    {
        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    memset(text_buf, 0, sizeof(text_buf));

    len = snprintf(text_buf,
                   sizeof(text_buf),
                   "init=%lu,reg=%u,disp=%lu,unk=%lu,err=%lu,last=0x%02X",
                   (unsigned long)stats->init_count,
                   (unsigned int)stats->registered_count,
                   (unsigned long)stats->dispatch_count,
                   (unsigned long)stats->unknown_cmd_count,
                   (unsigned long)stats->handler_error_count,
                   stats->last_cmd);

    if (len < 0)
    {
        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           (const uint8_t *)text_buf,
                           (uint16_t)len);

    return COMMAND_SERVICE_OK;
}
static const char *McuInfoApp_ResetCauseToShortString(PlatformResetCause_t cause)
{
    switch (cause)
    {
        case PLATFORM_RESET_CAUSE_PIN:
            return "PIN";

        case PLATFORM_RESET_CAUSE_POR:
            return "POR";

        case PLATFORM_RESET_CAUSE_BOR:
            return "BOR";

        case PLATFORM_RESET_CAUSE_SOFTWARE:
            return "SOFTWARE";

        case PLATFORM_RESET_CAUSE_IWDG:
            return "IWDG";

        case PLATFORM_RESET_CAUSE_WWDG:
            return "WWDG";

        case PLATFORM_RESET_CAUSE_UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

static void McuInfoApp_BuildResetInfoPayload(char *buf,
                                             uint16_t buf_size,
                                             const PlatformResetInfo_t *info)
{
    int len;

    if ((buf == NULL) || (buf_size == 0U))
    {
        return;
    }

    buf[0] = '\0';

    if (info == NULL)
    {
        (void)snprintf(buf, buf_size, "cause=UNKNOWN,valid=0");
        return;
    }

    len = snprintf(buf,
                   buf_size,
                   "cause=%s,pin=%u,por=%u,bor=%u,sw=%u,iwdg=%u,wwdg=%u",
                   McuInfoApp_ResetCauseToShortString(info->primary_cause),
                   info->pin_reset,
                   info->por_reset,
                   info->bor_reset,
                   info->software_reset,
                   info->iwdg_reset,
                   info->wwdg_reset);

    if (len < 0)
    {
        buf[0] = '\0';
        return;
    }

    if (len >= (int)buf_size)
    {
        buf[buf_size - 1U] = '\0';
    }
}

static int McuInfoApp_PopEvent(McuInfoEventRecord_t *event)
{
    uint16_t read_len;

    if (event == NULL)
    {
        return MCU_INFO_APP_INVALID_PARAM;
    }

    if (RingBuffer_Available(&g_mcu_info_app.event_rb) < (uint16_t)sizeof(McuInfoEventRecord_t))
    {
        return MCU_INFO_APP_ERROR;
    }

    read_len = RingBuffer_Read(&g_mcu_info_app.event_rb,
                               (uint8_t *)event,
                               (uint16_t)sizeof(*event));

    if (read_len != (uint16_t)sizeof(*event))
    {
        return MCU_INFO_APP_ERROR;
    }

    return MCU_INFO_APP_OK;
}

static void McuInfoApp_RunEventForwarder(void)
{
    uint32_t now;
    McuInfoEventRecord_t event;
    uint8_t event_payload[MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE + 6U];
    uint16_t event_payload_len = 0U;
    int ret;

    if (RingBuffer_Available(&g_mcu_info_app.event_rb) < (uint16_t)sizeof(McuInfoEventRecord_t))
    {
        return;
    }

    now = PlatformTime_GetMs();

    if ((now - g_mcu_info_app.last_event_forward_ms) < MCU_INFO_APP_EVENT_FORWARD_INTERVAL_MS)
    {
        return;
    }

    g_mcu_info_app.last_event_forward_ms = now;

    ret = McuInfoApp_PopEvent(&event);
    if (ret != MCU_INFO_APP_OK)
    {
        return;
    }

    event_payload[0] = event.app_id;
    event_payload[1] = event.event_id;
    event_payload[2] = (uint8_t)(event.tick_ms & 0xFFU);
    event_payload[3] = (uint8_t)((event.tick_ms >> 8U) & 0xFFU);
    event_payload[4] = (uint8_t)((event.tick_ms >> 16U) & 0xFFU);
    event_payload[5] = (uint8_t)((event.tick_ms >> 24U) & 0xFFU);
    event_payload_len = 6U;

    if (event.payload_len > 0U)
    {
        uint16_t copy_len = event.payload_len;

        if (copy_len > MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE)
        {
            copy_len = MCU_INFO_APP_MAX_EVENT_PAYLOAD_SIZE;
        }

        memcpy(&event_payload[event_payload_len], event.payload, copy_len);
        event_payload_len = (uint16_t)(event_payload_len + copy_len);
    }

    ret = CommandManager_PostEvent(event.event_id,
                                   event_payload,
                                   event_payload_len);

    if (ret == COMMAND_MANAGER_OK)
    {
        g_mcu_info_app.stats.event_forward_count++;
        g_mcu_info_app.stats.last_event_id = event.event_id;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_OK;
    }
    else
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_BUSY;
    }
}
