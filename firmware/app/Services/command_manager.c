#include "command_manager.h"

#include "command_service.h"
#include "ring_buffer.h"
#include "board_log.h"

#include <string.h>

typedef struct
{
    uint8_t initialized;
    CommandManagerStats_t stats;

    RingBuffer_t event_rb;
    uint8_t event_storage[COMMAND_MANAGER_EVENT_QUEUE_SIZE * sizeof(CommandManagerEventRecord_t)];
} CommandManagerContext_t;

static CommandManagerContext_t g_command_manager;

static void CommandManager_SetNack(CommandManagerResponse_t *resp,
                                   uint8_t cmd,
                                   uint8_t error_code);

void CommandManager_Init(void)
{
    memset(&g_command_manager, 0, sizeof(g_command_manager));

    RingBuffer_Init(&g_command_manager.event_rb,
                    g_command_manager.event_storage,
                    (uint16_t)sizeof(g_command_manager.event_storage));

    g_command_manager.initialized = 1U;
    g_command_manager.stats.init_count++;

    BoardLog_Info("CommandManager init OK\r\n");
}

int CommandManager_Dispatch(const ProtocolFrame_t *req_frame,
                            CommandManagerResponse_t *resp)
{
    int ret;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_command_manager.stats.invalid_param_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return COMMAND_MANAGER_INVALID_PARAM;
    }

    memset(resp, 0, sizeof(*resp));

    g_command_manager.stats.dispatch_count++;
    g_command_manager.stats.last_cmd = req_frame->cmd;

    if (g_command_manager.initialized == 0U)
    {
        g_command_manager.stats.error_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INVALID_STATE;

        CommandManager_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INVALID_STATE);

        return COMMAND_MANAGER_ERROR;
    }

    ret = CommandService_Dispatch(req_frame, resp);

    g_command_manager.stats.routed_to_command_service_count++;

    if (ret == COMMAND_SERVICE_OK)
    {
        g_command_manager.stats.last_error = PROTO_ERROR_OK;
        return COMMAND_MANAGER_OK;
    }

    if (ret == COMMAND_SERVICE_UNKNOWN_CMD)
    {
        g_command_manager.stats.unknown_cmd_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;
        return COMMAND_MANAGER_UNKNOWN_CMD;
    }

    g_command_manager.stats.error_count++;

    if (resp->frame_type == 0U)
    {
        g_command_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandManager_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);
    }
    else
    {
        g_command_manager.stats.last_error = resp->error_code;
    }

    return COMMAND_MANAGER_ERROR;
}

int CommandManager_PostEvent(uint8_t event_id,
                             const uint8_t *payload,
                             uint16_t payload_len)
{
    CommandManagerEventRecord_t record;
    uint16_t copy_len;
    uint16_t written;

    if (g_command_manager.initialized == 0U)
    {
        g_command_manager.stats.error_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INVALID_STATE;
        return COMMAND_MANAGER_ERROR;
    }

    if ((payload_len > 0U) && (payload == NULL))
    {
        g_command_manager.stats.invalid_param_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return COMMAND_MANAGER_INVALID_PARAM;
    }

    if (RingBuffer_Free(&g_command_manager.event_rb) < (uint16_t)sizeof(CommandManagerEventRecord_t))
    {
        g_command_manager.stats.event_drop_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_BUSY;
        return COMMAND_MANAGER_QUEUE_FULL;
    }

    memset(&record, 0, sizeof(record));

    record.event_id = event_id;

    copy_len = payload_len;
    if (copy_len > COMMAND_MANAGER_MAX_EVENT_PAYLOAD_SIZE)
    {
        copy_len = COMMAND_MANAGER_MAX_EVENT_PAYLOAD_SIZE;
    }

    if ((payload != NULL) && (copy_len > 0U))
    {
        memcpy(record.payload, payload, copy_len);
        record.payload_len = copy_len;
    }

    written = RingBuffer_Write(&g_command_manager.event_rb,
                               (const uint8_t *)&record,
                               (uint16_t)sizeof(record));

    if (written != (uint16_t)sizeof(record))
    {
        g_command_manager.stats.event_drop_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_BUSY;
        return COMMAND_MANAGER_QUEUE_FULL;
    }

    g_command_manager.stats.post_event_count++;
    g_command_manager.stats.last_event_id = event_id;
    g_command_manager.stats.last_error = PROTO_ERROR_OK;

    return COMMAND_MANAGER_OK;
}

int CommandManager_TryGetPendingEvent(CommandManagerEventRecord_t *event)
{
    uint16_t read_len;

    if (event == NULL)
    {
        g_command_manager.stats.invalid_param_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return COMMAND_MANAGER_INVALID_PARAM;
    }

    if (g_command_manager.initialized == 0U)
    {
        g_command_manager.stats.error_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INVALID_STATE;
        return COMMAND_MANAGER_ERROR;
    }

    if (RingBuffer_Available(&g_command_manager.event_rb) < (uint16_t)sizeof(CommandManagerEventRecord_t))
    {
        return COMMAND_MANAGER_NO_EVENT;
    }

    read_len = RingBuffer_Read(&g_command_manager.event_rb,
                               (uint8_t *)event,
                               (uint16_t)sizeof(*event));

    if (read_len != (uint16_t)sizeof(*event))
    {
        g_command_manager.stats.error_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;
        return COMMAND_MANAGER_ERROR;
    }

    g_command_manager.stats.event_pop_count++;
    g_command_manager.stats.last_event_id = event->event_id;
    g_command_manager.stats.last_error = PROTO_ERROR_OK;

    return COMMAND_MANAGER_OK;
}

const CommandManagerStats_t *CommandManager_GetStats(void)
{
    return &g_command_manager.stats;
}

void CommandManager_ResetStats(void)
{
    memset(&g_command_manager.stats, 0, sizeof(g_command_manager.stats));
}

void CommandManager_PrintStats(void)
{
    const RingBufferStats_t *rb_stats;

    rb_stats = RingBuffer_GetStats(&g_command_manager.event_rb);

    BoardLog_PrintSeparator();

    BoardLog_Info("CommandManager Stats:\r\n");
    BoardLog_Info("  initialized              = %u\r\n", g_command_manager.initialized);
    BoardLog_Info("  init_count               = %lu\r\n", g_command_manager.stats.init_count);
    BoardLog_Info("  dispatch_count           = %lu\r\n", g_command_manager.stats.dispatch_count);
    BoardLog_Info("  routed_to_cmd_service    = %lu\r\n", g_command_manager.stats.routed_to_command_service_count);
    BoardLog_Info("  post_event_count         = %lu\r\n", g_command_manager.stats.post_event_count);
    BoardLog_Info("  event_pop_count          = %lu\r\n", g_command_manager.stats.event_pop_count);
    BoardLog_Info("  event_drop_count         = %lu\r\n", g_command_manager.stats.event_drop_count);
    BoardLog_Info("  unknown_cmd_count        = %lu\r\n", g_command_manager.stats.unknown_cmd_count);
    BoardLog_Info("  invalid_param_count      = %lu\r\n", g_command_manager.stats.invalid_param_count);
    BoardLog_Info("  error_count              = %lu\r\n", g_command_manager.stats.error_count);
    BoardLog_Info("  last_cmd                 = 0x%02X\r\n", g_command_manager.stats.last_cmd);
    BoardLog_Info("  last_event_id            = 0x%02X\r\n", g_command_manager.stats.last_event_id);
    BoardLog_Info("  last_error               = 0x%02X\r\n", g_command_manager.stats.last_error);
    BoardLog_Info("  event_available          = %u\r\n", RingBuffer_Available(&g_command_manager.event_rb));

    if (rb_stats != NULL)
    {
        BoardLog_Info("  event_rb_write_bytes     = %lu\r\n", rb_stats->write_bytes);
        BoardLog_Info("  event_rb_read_bytes      = %lu\r\n", rb_stats->read_bytes);
        BoardLog_Info("  event_rb_overflow        = %lu\r\n", rb_stats->overflow_count);
        BoardLog_Info("  event_rb_high            = %u\r\n", rb_stats->high_watermark);
    }
}

static void CommandManager_SetNack(CommandManagerResponse_t *resp,
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
