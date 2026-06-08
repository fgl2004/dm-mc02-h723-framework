#include "command_manager.h"

#include "command_service.h"
#include "board_log.h"
#include "platform_time.h"

#include <string.h>

typedef struct
{
    uint8_t initialized;
    CommandManagerStats_t stats;
} CommandManagerContext_t;

static CommandManagerContext_t g_command_manager;

static void CommandManager_SetNack(CommandManagerResponse_t *resp,
                                   uint8_t cmd,
                                   uint8_t error_code);

void CommandManager_Init(void)
{
    memset(&g_command_manager, 0, sizeof(g_command_manager));

    g_command_manager.initialized = 1U;
    g_command_manager.stats.init_count++;

    BoardLog_Info("CommandManager init OK\r\n");
}

int CommandManager_Dispatch(const ProtocolFrame_t *req_frame,
                            CommandManagerResponse_t *resp)
{
    int ret;
    uint32_t start_cycle;
    uint32_t elapsed_us;

    if ((req_frame == 0) || (resp == 0))
    {
        g_command_manager.stats.invalid_param_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return COMMAND_MANAGER_INVALID_PARAM;
    }

    start_cycle = PlatformTime_ProfileStart();

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

        elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
        g_command_manager.stats.last_dispatch_us = elapsed_us;
        if (elapsed_us > g_command_manager.stats.max_dispatch_us)
        {
            g_command_manager.stats.max_dispatch_us = elapsed_us;
        }

        return COMMAND_MANAGER_ERROR;
    }

    ret = CommandService_Dispatch(req_frame, resp);

    g_command_manager.stats.routed_to_command_service_count++;

    if (ret == COMMAND_SERVICE_OK)
    {
        g_command_manager.stats.last_error = PROTO_ERROR_OK;

        elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
        g_command_manager.stats.last_dispatch_us = elapsed_us;
        if (elapsed_us > g_command_manager.stats.max_dispatch_us)
        {
            g_command_manager.stats.max_dispatch_us = elapsed_us;
        }

        return COMMAND_MANAGER_OK;
    }

    if (ret == COMMAND_SERVICE_UNKNOWN_CMD)
    {
        g_command_manager.stats.unknown_cmd_count++;
        g_command_manager.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;

        elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
        g_command_manager.stats.last_dispatch_us = elapsed_us;
        if (elapsed_us > g_command_manager.stats.max_dispatch_us)
        {
            g_command_manager.stats.max_dispatch_us = elapsed_us;
        }

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

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_command_manager.stats.last_dispatch_us = elapsed_us;
    if (elapsed_us > g_command_manager.stats.max_dispatch_us)
    {
        g_command_manager.stats.max_dispatch_us = elapsed_us;
    }

    return COMMAND_MANAGER_ERROR;
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
    BoardLog_PrintSeparator();

    BoardLog_Info("CommandManager Stats:\r\n");
    BoardLog_Info("  initialized              = %u\r\n", g_command_manager.initialized);
    BoardLog_Info("  init_count               = %lu\r\n", g_command_manager.stats.init_count);
    BoardLog_Info("  dispatch_count           = %lu\r\n", g_command_manager.stats.dispatch_count);
    BoardLog_Info("  last_dispatch_us         = %lu\r\n", g_command_manager.stats.last_dispatch_us);
    BoardLog_Info("  max_dispatch_us          = %lu\r\n", g_command_manager.stats.max_dispatch_us);
    BoardLog_Info("  routed_to_cmd_service    = %lu\r\n", g_command_manager.stats.routed_to_command_service_count);
    BoardLog_Info("  unknown_cmd_count        = %lu\r\n", g_command_manager.stats.unknown_cmd_count);
    BoardLog_Info("  invalid_param_count      = %lu\r\n", g_command_manager.stats.invalid_param_count);
    BoardLog_Info("  error_count              = %lu\r\n", g_command_manager.stats.error_count);
    BoardLog_Info("  last_cmd                 = 0x%02X\r\n", g_command_manager.stats.last_cmd);
    BoardLog_Info("  last_error               = 0x%02X\r\n", g_command_manager.stats.last_error);
}

static void CommandManager_SetNack(CommandManagerResponse_t *resp,
                                   uint8_t cmd,
                                   uint8_t error_code)
{
    if (resp == 0)
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
