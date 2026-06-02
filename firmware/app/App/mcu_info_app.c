#include "mcu_info_app.h"

#include "command_manager.h"
#include "ring_buffer.h"

#include "platform_time.h"
#include "platform_uart.h"
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
    uint8_t app_id;
    uint16_t snapshot_len;
    uint8_t snapshot_data[MCU_INFO_APP_MAX_SNAPSHOT_PAYLOAD_SIZE];
} McuInfoSnapshotSlot_t;

typedef struct
{
    uint8_t initialized;

    McuInfoAppStats_t stats;
    McuInfoRuntimeStatus_t runtime_status;

    McuInfoSnapshotSlot_t snapshots[4];

    RingBuffer_t event_rb;
    uint8_t event_storage[MCU_INFO_APP_EVENT_QUEUE_SIZE * sizeof(McuInfoEventRecord_t)];

    uint32_t last_event_forward_ms;
} McuInfoAppContext_t;

static McuInfoAppContext_t g_mcu_info_app;

static void McuInfoApp_SetResp(McuInfoAppResponse_t *resp,
                               uint8_t cmd,
                               const uint8_t *payload,
                               uint16_t payload_len);

static void McuInfoApp_SetNack(McuInfoAppResponse_t *resp,
                               uint8_t cmd,
                               uint8_t error_code);

static void McuInfoApp_HandlePing(const ProtocolFrame_t *req_frame,
                                  McuInfoAppResponse_t *resp);

static void McuInfoApp_HandleGetVersion(const ProtocolFrame_t *req_frame,
                                        McuInfoAppResponse_t *resp);

static void McuInfoApp_HandleGetStatus(const ProtocolFrame_t *req_frame,
                                       McuInfoAppResponse_t *resp);

static void McuInfoApp_HandleGetResetInfo(const ProtocolFrame_t *req_frame,
                                          McuInfoAppResponse_t *resp);

static void McuInfoApp_HandleGetTimeInfo(const ProtocolFrame_t *req_frame,
                                         McuInfoAppResponse_t *resp);

static void McuInfoApp_HandleGetFaultInfo(const ProtocolFrame_t *req_frame,
                                          McuInfoAppResponse_t *resp);

static void McuInfoApp_HandleGetUartStats(const ProtocolFrame_t *req_frame,
                                          McuInfoAppResponse_t *resp);

static void McuInfoApp_HandleGetAppStats(const ProtocolFrame_t *req_frame,
                                         McuInfoAppResponse_t *resp);

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
    if (g_mcu_info_app.initialized == 0U)
    {
        return;
    }

    g_mcu_info_app.stats.run_count++;

    /*
     * McuInfoApp is the central information app.
     *
     * It does not send EVENT frames directly through ProtocolManager.
     * Instead, it forwards prepared events to CommandManager.
     *
     * CommandManager owns the pending PC-facing event queue.
     * ProtocolManager later fetches events from CommandManager and sends them.
     */
    McuInfoApp_RunEventForwarder();
}

int McuInfoApp_HandleCommand(const ProtocolFrame_t *req_frame,
                             McuInfoAppResponse_t *resp)
{
    if ((req_frame == NULL) || (resp == NULL))
    {
        g_mcu_info_app.stats.invalid_param_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return MCU_INFO_APP_INVALID_PARAM;
    }

    memset(resp, 0, sizeof(*resp));

    g_mcu_info_app.stats.dispatch_count++;
    g_mcu_info_app.stats.last_cmd = req_frame->cmd;

    if (g_mcu_info_app.initialized == 0U)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INVALID_STATE;

        McuInfoApp_SetNack(resp,
                           req_frame->cmd,
                           PROTO_ERROR_INVALID_STATE);

        return MCU_INFO_APP_ERROR;
    }

    switch (req_frame->cmd)
    {
        case MCU_INFO_CMD_PING:
            McuInfoApp_HandlePing(req_frame, resp);
            break;

        case MCU_INFO_CMD_GET_VERSION:
            McuInfoApp_HandleGetVersion(req_frame, resp);
            break;

        case MCU_INFO_CMD_GET_STATUS:
            McuInfoApp_HandleGetStatus(req_frame, resp);
            break;

        case MCU_INFO_CMD_GET_RESET_INFO:
            McuInfoApp_HandleGetResetInfo(req_frame, resp);
            break;

        case MCU_INFO_CMD_GET_TIME_INFO:
            McuInfoApp_HandleGetTimeInfo(req_frame, resp);
            break;

        case MCU_INFO_CMD_GET_FAULT_INFO:
            McuInfoApp_HandleGetFaultInfo(req_frame, resp);
            break;

        case MCU_INFO_CMD_GET_UART_STATS:
            McuInfoApp_HandleGetUartStats(req_frame, resp);
            break;

        case MCU_INFO_CMD_GET_APP_STATS:
            McuInfoApp_HandleGetAppStats(req_frame, resp);
            break;

        default:
            g_mcu_info_app.stats.unknown_cmd_count++;
            g_mcu_info_app.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;

            McuInfoApp_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_UNKNOWN_CMD);

            return MCU_INFO_APP_UNKNOWN_CMD;
    }

    return MCU_INFO_APP_OK;
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

int McuInfoApp_UpdateSnapshot(uint8_t app_id,
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
            (g_mcu_info_app.snapshots[i].app_id == app_id))
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
    slot->app_id = app_id;

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
    BoardLog_Info("  initialized           = %u\r\n", g_mcu_info_app.initialized);
    BoardLog_Info("  init_count            = %lu\r\n", g_mcu_info_app.stats.init_count);
    BoardLog_Info("  run_count             = %lu\r\n", g_mcu_info_app.stats.run_count);
    BoardLog_Info("  dispatch_count        = %lu\r\n", g_mcu_info_app.stats.dispatch_count);
    BoardLog_Info("  ping_count            = %lu\r\n", g_mcu_info_app.stats.ping_count);
    BoardLog_Info("  get_version_count     = %lu\r\n", g_mcu_info_app.stats.get_version_count);
    BoardLog_Info("  get_status_count      = %lu\r\n", g_mcu_info_app.stats.get_status_count);
    BoardLog_Info("  get_reset_info_count  = %lu\r\n", g_mcu_info_app.stats.get_reset_info_count);
    BoardLog_Info("  get_time_info_count   = %lu\r\n", g_mcu_info_app.stats.get_time_info_count);
    BoardLog_Info("  get_fault_info_count  = %lu\r\n", g_mcu_info_app.stats.get_fault_info_count);
    BoardLog_Info("  get_uart_stats_count  = %lu\r\n", g_mcu_info_app.stats.get_uart_stats_count);
    BoardLog_Info("  get_app_stats_count   = %lu\r\n", g_mcu_info_app.stats.get_app_stats_count);
    BoardLog_Info("  post_event_count      = %lu\r\n", g_mcu_info_app.stats.post_event_count);
    BoardLog_Info("  event_forward_count   = %lu\r\n", g_mcu_info_app.stats.event_forward_count);
    BoardLog_Info("  event_drop_count      = %lu\r\n", g_mcu_info_app.stats.event_drop_count);
    BoardLog_Info("  update_snapshot_count = %lu\r\n", g_mcu_info_app.stats.update_snapshot_count);
    BoardLog_Info("  update_status_count   = %lu\r\n", g_mcu_info_app.stats.update_status_count);
    BoardLog_Info("  unknown_cmd_count     = %lu\r\n", g_mcu_info_app.stats.unknown_cmd_count);
    BoardLog_Info("  invalid_param_count   = %lu\r\n", g_mcu_info_app.stats.invalid_param_count);
    BoardLog_Info("  error_count           = %lu\r\n", g_mcu_info_app.stats.error_count);
    BoardLog_Info("  last_cmd              = 0x%02X\r\n", g_mcu_info_app.stats.last_cmd);
    BoardLog_Info("  last_event_id         = 0x%02X\r\n", g_mcu_info_app.stats.last_event_id);
    BoardLog_Info("  last_error            = 0x%02X\r\n", g_mcu_info_app.stats.last_error);
    BoardLog_Info("  event_available       = %u\r\n", RingBuffer_Available(&g_mcu_info_app.event_rb));

    if (rb_stats != NULL)
    {
        BoardLog_Info("  event_rb_write_bytes  = %lu\r\n", rb_stats->write_bytes);
        BoardLog_Info("  event_rb_read_bytes   = %lu\r\n", rb_stats->read_bytes);
        BoardLog_Info("  event_rb_overflow     = %lu\r\n", rb_stats->overflow_count);
        BoardLog_Info("  event_rb_high         = %u\r\n", rb_stats->high_watermark);
    }
}

static void McuInfoApp_SetResp(McuInfoAppResponse_t *resp,
                               uint8_t cmd,
                               const uint8_t *payload,
                               uint16_t payload_len)
{
    uint16_t copy_len;

    if (resp == NULL)
    {
        return;
    }

    memset(resp, 0, sizeof(*resp));

    resp->frame_type = PROTO_FRAME_TYPE_RESP;
    resp->cmd = cmd;
    resp->error_code = PROTO_ERROR_OK;

    if ((payload != NULL) && (payload_len > 0U))
    {
        copy_len = payload_len;

        if (copy_len > PROTO_FRAME_MAX_PAYLOAD_SIZE)
        {
            copy_len = PROTO_FRAME_MAX_PAYLOAD_SIZE;
        }

        memcpy(resp->payload, payload, copy_len);
        resp->payload_len = copy_len;
    }
}

static void McuInfoApp_SetNack(McuInfoAppResponse_t *resp,
                               uint8_t cmd,
                               uint8_t error_code)
{
    if (resp == NULL)
    {
        return;
    }

    memset(resp, 0, sizeof(*resp));

    resp->frame_type = PROTO_FRAME_TYPE_NACK;
    resp->cmd = cmd;
    resp->error_code = error_code;

    resp->payload[0] = error_code;
    resp->payload[1] = cmd;
    resp->payload_len = 2U;
}

static void McuInfoApp_HandlePing(const ProtocolFrame_t *req_frame,
                                  McuInfoAppResponse_t *resp)
{
    static const uint8_t pong_payload[] = { 'P', 'O', 'N', 'G' };

    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.ping_count++;

    McuInfoApp_SetResp(resp,
                       req_frame->cmd,
                       pong_payload,
                       (uint16_t)sizeof(pong_payload));
}

static void McuInfoApp_HandleGetVersion(const ProtocolFrame_t *req_frame,
                                        McuInfoAppResponse_t *resp)
{
    const char *version = MCU_INFO_APP_VERSION_STRING;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.get_version_count++;

    McuInfoApp_SetResp(resp,
                       req_frame->cmd,
                       (const uint8_t *)version,
                       (uint16_t)strlen(version));
}

static void McuInfoApp_HandleGetStatus(const ProtocolFrame_t *req_frame,
                                       McuInfoAppResponse_t *resp)
{
    const char *status = MCU_INFO_APP_STATUS_STRING;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.get_status_count++;

    McuInfoApp_SetResp(resp,
                       req_frame->cmd,
                       (const uint8_t *)status,
                       (uint16_t)strlen(status));
}

static void McuInfoApp_HandleGetResetInfo(const ProtocolFrame_t *req_frame,
                                          McuInfoAppResponse_t *resp)
{
    /*
     * TODO:
     * Reset info should be posted as snapshot at boot stage.
     */
    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.get_reset_info_count++;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;

    McuInfoApp_SetNack(resp,
                       req_frame->cmd,
                       PROTO_ERROR_UNKNOWN_CMD);
}

static void McuInfoApp_HandleGetTimeInfo(const ProtocolFrame_t *req_frame,
                                         McuInfoAppResponse_t *resp)
{
    char text_buf[32];
    uint32_t tick;
    int len;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.get_time_info_count++;

    tick = PlatformTime_GetMs();

    len = snprintf(text_buf,
                   sizeof(text_buf),
                   "tick=%lu",
                   (unsigned long)tick);

    if (len < 0)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        McuInfoApp_SetNack(resp,
                           req_frame->cmd,
                           PROTO_ERROR_INTERNAL_ERROR);
        return;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    McuInfoApp_SetResp(resp,
                       req_frame->cmd,
                       (const uint8_t *)text_buf,
                       (uint16_t)len);
}

static void McuInfoApp_HandleGetFaultInfo(const ProtocolFrame_t *req_frame,
                                          McuInfoAppResponse_t *resp)
{
    /*
     * TODO:
     * Fault info should be posted as snapshot/event by fault module.
     */
    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.get_fault_info_count++;
    g_mcu_info_app.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;

    McuInfoApp_SetNack(resp,
                       req_frame->cmd,
                       PROTO_ERROR_UNKNOWN_CMD);
}

static void McuInfoApp_HandleGetUartStats(const ProtocolFrame_t *req_frame,
                                          McuInfoAppResponse_t *resp)
{
    char text_buf[96];
    PlatformUartRxSnapshot_t snapshot;
    int len;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.get_uart_stats_count++;

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

        McuInfoApp_SetNack(resp,
                           req_frame->cmd,
                           PROTO_ERROR_INTERNAL_ERROR);
        return;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    McuInfoApp_SetResp(resp,
                       req_frame->cmd,
                       (const uint8_t *)text_buf,
                       (uint16_t)len);
}

static void McuInfoApp_HandleGetAppStats(const ProtocolFrame_t *req_frame,
                                         McuInfoAppResponse_t *resp)
{
    char text_buf[96];
    int len;

    if ((req_frame == NULL) || (resp == NULL))
    {
        return;
    }

    g_mcu_info_app.stats.get_app_stats_count++;

    len = snprintf(text_buf,
                   sizeof(text_buf),
                   "run=%lu,post=%lu,fwd=%lu,drop=%lu",
                   (unsigned long)g_mcu_info_app.stats.run_count,
                   (unsigned long)g_mcu_info_app.stats.post_event_count,
                   (unsigned long)g_mcu_info_app.stats.event_forward_count,
                   (unsigned long)g_mcu_info_app.stats.event_drop_count);

    if (len < 0)
    {
        g_mcu_info_app.stats.error_count++;
        g_mcu_info_app.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        McuInfoApp_SetNack(resp,
                           req_frame->cmd,
                           PROTO_ERROR_INTERNAL_ERROR);
        return;
    }

    if (len >= (int)sizeof(text_buf))
    {
        len = (int)(sizeof(text_buf) - 1);
        text_buf[len] = '\0';
    }

    McuInfoApp_SetResp(resp,
                       req_frame->cmd,
                       (const uint8_t *)text_buf,
                       (uint16_t)len);
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

    /*
     * Event payload format sent to CommandManager:
     *   [0] app_id
     *   [1] original_event_id
     *   [2] tick LSB
     *   [3] tick
     *   [4] tick
     *   [5] tick MSB
     *   [6..] original payload
     */
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