#include "command_service.h"

#include "board_log.h"
#include "platform_time.h"

#include <string.h>

typedef struct
{
    uint8_t initialized;
    CommandServiceStats_t stats;
    CommandServiceEntry_t table[COMMAND_SERVICE_MAX_COMMANDS];
} CommandServiceContext_t;

static CommandServiceContext_t g_command_service;

static int CommandService_FindIndex(uint8_t cmd);
static void CommandService_CountCategory(uint8_t category);

void CommandService_Init(void)
{
    memset(&g_command_service, 0, sizeof(g_command_service));

    g_command_service.initialized = 1U;
    g_command_service.stats.init_count++;

    BoardLog_Info("CommandService init OK\r\n");
}

int CommandService_Register(uint8_t cmd,
                            uint8_t category,
                            uint32_t flags,
                            CommandServiceHandler_t handler,
                            void *ctx,
                            const char *name)
{
    int existing_index;
    uint8_t index;

    if (handler == NULL)
    {
        g_command_service.stats.invalid_param_count++;
        g_command_service.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    if (g_command_service.initialized == 0U)
    {
        g_command_service.stats.last_error = PROTO_ERROR_INVALID_STATE;
        return COMMAND_SERVICE_NOT_INITIALIZED;
    }

    existing_index = CommandService_FindIndex(cmd);

    if (existing_index >= 0)
    {
        /*
         * Duplicate registration is treated as an update.
         * This is useful during refactor and later module replacement.
         */
        g_command_service.table[existing_index].category = category;
        g_command_service.table[existing_index].flags = flags;
        g_command_service.table[existing_index].handler = handler;
        g_command_service.table[existing_index].ctx = ctx;
        g_command_service.table[existing_index].name = name;

        g_command_service.stats.duplicate_register_count++;
        g_command_service.stats.last_cmd = cmd;
        g_command_service.stats.last_category = category;
        g_command_service.stats.last_error = PROTO_ERROR_OK;

        return COMMAND_SERVICE_OK;
    }

    if (g_command_service.stats.registered_count >= COMMAND_SERVICE_MAX_COMMANDS)
    {
        g_command_service.stats.table_full_count++;
        g_command_service.stats.last_error = PROTO_ERROR_BUSY;
        return COMMAND_SERVICE_TABLE_FULL;
    }

    index = g_command_service.stats.registered_count;

    g_command_service.table[index].cmd = cmd;
    g_command_service.table[index].category = category;
    g_command_service.table[index].flags = flags;
    g_command_service.table[index].handler = handler;
    g_command_service.table[index].ctx = ctx;
    g_command_service.table[index].name = name;

    g_command_service.stats.registered_count++;
    g_command_service.stats.register_count++;
    g_command_service.stats.last_cmd = cmd;
    g_command_service.stats.last_category = category;
    g_command_service.stats.last_error = PROTO_ERROR_OK;

    return COMMAND_SERVICE_OK;
}

int CommandService_Dispatch(const ProtocolFrame_t *req_frame,
                            CommandManagerResponse_t *resp)
{
    int index;
    int ret;
    uint8_t category;
    uint32_t start_cycle;
    uint32_t elapsed_us;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_command_service.stats.invalid_param_count++;
        g_command_service.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    start_cycle = PlatformTime_ProfileStart();

    memset(resp, 0, sizeof(*resp));

    g_command_service.stats.dispatch_count++;
    g_command_service.stats.last_cmd = req_frame->cmd;

    if (g_command_service.initialized == 0U)
    {
        g_command_service.stats.last_error = PROTO_ERROR_INVALID_STATE;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INVALID_STATE);

        elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
        g_command_service.stats.last_dispatch_us = elapsed_us;
        if (elapsed_us > g_command_service.stats.max_dispatch_us)
        {
            g_command_service.stats.max_dispatch_us = elapsed_us;
        }

        return COMMAND_SERVICE_NOT_INITIALIZED;
    }

    index = CommandService_FindIndex(req_frame->cmd);

    if (index < 0)
    {
        g_command_service.stats.unknown_cmd_count++;
        g_command_service.stats.last_error = PROTO_ERROR_UNKNOWN_CMD;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_UNKNOWN_CMD);

        elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
        g_command_service.stats.last_dispatch_us = elapsed_us;
        if (elapsed_us > g_command_service.stats.max_dispatch_us)
        {
            g_command_service.stats.max_dispatch_us = elapsed_us;
        }

        return COMMAND_SERVICE_UNKNOWN_CMD;
    }

    category = g_command_service.table[index].category;
    g_command_service.stats.last_category = category;
    CommandService_CountCategory(category);

    if (g_command_service.table[index].handler == NULL)
    {
        g_command_service.stats.handler_error_count++;
        g_command_service.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;

        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);

        elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
        g_command_service.stats.last_dispatch_us = elapsed_us;
        if (elapsed_us > g_command_service.stats.max_dispatch_us)
        {
            g_command_service.stats.max_dispatch_us = elapsed_us;
        }

        return COMMAND_SERVICE_ERROR;
    }

    ret = g_command_service.table[index].handler(req_frame,
                                                 resp,
                                                 g_command_service.table[index].ctx);

    if (ret == COMMAND_SERVICE_OK)
    {
        if (resp->frame_type == 0U)
        {
            CommandService_SetResp(resp,
                                   req_frame->cmd,
                                   NULL,
                                   0U);
        }

        g_command_service.stats.last_error = PROTO_ERROR_OK;

        elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
        g_command_service.stats.last_dispatch_us = elapsed_us;
        if (elapsed_us > g_command_service.stats.max_dispatch_us)
        {
            g_command_service.stats.max_dispatch_us = elapsed_us;
        }

        return COMMAND_SERVICE_OK;
    }

    if (resp->frame_type == 0U)
    {
        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);
    }

    g_command_service.stats.handler_error_count++;
    g_command_service.stats.last_error = resp->error_code;

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_command_service.stats.last_dispatch_us = elapsed_us;
    if (elapsed_us > g_command_service.stats.max_dispatch_us)
    {
        g_command_service.stats.max_dispatch_us = elapsed_us;
    }

    return COMMAND_SERVICE_ERROR;
}

const CommandServiceStats_t *CommandService_GetStats(void)
{
    return &g_command_service.stats;
}

void CommandService_ResetStats(void)
{
    uint8_t registered_count = g_command_service.stats.registered_count;

    memset(&g_command_service.stats, 0, sizeof(g_command_service.stats));
    g_command_service.stats.registered_count = registered_count;
}

void CommandService_PrintStats(void)
{
    BoardLog_PrintSeparator();

    BoardLog_Info("CommandService Stats:\r\n");
    BoardLog_Info("  initialized          = %u\r\n", g_command_service.initialized);
    BoardLog_Info("  init_count           = %lu\r\n", g_command_service.stats.init_count);
    BoardLog_Info("  registered_count     = %u\r\n", g_command_service.stats.registered_count);
    BoardLog_Info("  register_count       = %lu\r\n", g_command_service.stats.register_count);
    BoardLog_Info("  duplicate_register   = %lu\r\n", g_command_service.stats.duplicate_register_count);
    BoardLog_Info("  dispatch_count       = %lu\r\n", g_command_service.stats.dispatch_count);
    BoardLog_Info("  last_dispatch_us     = %lu\r\n", g_command_service.stats.last_dispatch_us);
    BoardLog_Info("  max_dispatch_us      = %lu\r\n", g_command_service.stats.max_dispatch_us);
    BoardLog_Info("  system_cmd_count     = %lu\r\n", g_command_service.stats.system_cmd_count);
    BoardLog_Info("  diag_cmd_count       = %lu\r\n", g_command_service.stats.diag_cmd_count);
    BoardLog_Info("  imu_cmd_count        = %lu\r\n", g_command_service.stats.imu_cmd_count);
    BoardLog_Info("  can_cmd_count        = %lu\r\n", g_command_service.stats.can_cmd_count);
    BoardLog_Info("  param_cmd_count      = %lu\r\n", g_command_service.stats.param_cmd_count);
    BoardLog_Info("  boot_cmd_count       = %lu\r\n", g_command_service.stats.boot_cmd_count);
    BoardLog_Info("  security_cmd_count   = %lu\r\n", g_command_service.stats.security_cmd_count);
    BoardLog_Info("  power_cmd_count      = %lu\r\n", g_command_service.stats.power_cmd_count);
    BoardLog_Info("  chaos_cmd_count      = %lu\r\n", g_command_service.stats.chaos_cmd_count);
    BoardLog_Info("  storage_cmd_count    = %lu\r\n", g_command_service.stats.storage_cmd_count);
    BoardLog_Info("  debug_cmd_count      = %lu\r\n", g_command_service.stats.debug_cmd_count);
    BoardLog_Info("  unknown_cmd_count    = %lu\r\n", g_command_service.stats.unknown_cmd_count);
    BoardLog_Info("  table_full_count     = %lu\r\n", g_command_service.stats.table_full_count);
    BoardLog_Info("  invalid_param_count  = %lu\r\n", g_command_service.stats.invalid_param_count);
    BoardLog_Info("  handler_error_count  = %lu\r\n", g_command_service.stats.handler_error_count);
    BoardLog_Info("  last_cmd             = 0x%02X\r\n", g_command_service.stats.last_cmd);
    BoardLog_Info("  last_category        = %u(%s)\r\n",
                  g_command_service.stats.last_category,
                  CommandService_GetCategoryName(g_command_service.stats.last_category));
    BoardLog_Info("  last_error           = 0x%02X\r\n", g_command_service.stats.last_error);
}

void CommandService_SetResp(CommandManagerResponse_t *resp,
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

void CommandService_SetNack(CommandManagerResponse_t *resp,
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

uint8_t CommandService_GetCategoryByCmd(uint8_t cmd)
{
    if ((cmd >= CMD_DOMAIN_SYSTEM_START) && (cmd <= CMD_DOMAIN_SYSTEM_END))
    {
        return CMD_CATEGORY_SYSTEM;
    }

    if ((cmd >= CMD_DOMAIN_DIAG_START) && (cmd <= CMD_DOMAIN_DIAG_END))
    {
        return CMD_CATEGORY_DIAG;
    }

    if ((cmd >= CMD_DOMAIN_IMU_START) && (cmd <= CMD_DOMAIN_IMU_END))
    {
        return CMD_CATEGORY_IMU;
    }

    if ((cmd >= CMD_DOMAIN_CAN_START) && (cmd <= CMD_DOMAIN_CAN_END))
    {
        return CMD_CATEGORY_CAN;
    }

    if ((cmd >= CMD_DOMAIN_PARAM_START) && (cmd <= CMD_DOMAIN_PARAM_END))
    {
        return CMD_CATEGORY_PARAM;
    }

    if ((cmd >= CMD_DOMAIN_BOOT_START) && (cmd <= CMD_DOMAIN_BOOT_END))
    {
        return CMD_CATEGORY_BOOT;
    }

    if ((cmd >= CMD_DOMAIN_SECURITY_START) && (cmd <= CMD_DOMAIN_SECURITY_END))
    {
        return CMD_CATEGORY_SECURITY;
    }

    if ((cmd >= CMD_DOMAIN_POWER_START) && (cmd <= CMD_DOMAIN_POWER_END))
    {
        return CMD_CATEGORY_POWER;
    }

    if ((cmd >= CMD_DOMAIN_CHAOS_START) && (cmd <= CMD_DOMAIN_CHAOS_END))
    {
        return CMD_CATEGORY_CHAOS;
    }

    if ((cmd >= CMD_DOMAIN_STORAGE_START) && (cmd <= CMD_DOMAIN_STORAGE_END))
    {
        return CMD_CATEGORY_STORAGE;
    }

    if ((cmd >= CMD_DOMAIN_DEBUG_START) && (cmd <= CMD_DOMAIN_DEBUG_END))
    {
        return CMD_CATEGORY_DEBUG;
    }

    return CMD_CATEGORY_DEBUG;
}

const char *CommandService_GetCategoryName(uint8_t category)
{
    switch (category)
    {
        case CMD_CATEGORY_SYSTEM:
            return "SYSTEM";

        case CMD_CATEGORY_DIAG:
            return "DIAG";

        case CMD_CATEGORY_IMU:
            return "IMU";

        case CMD_CATEGORY_CAN:
            return "CAN";

        case CMD_CATEGORY_PARAM:
            return "PARAM";

        case CMD_CATEGORY_BOOT:
            return "BOOT";

        case CMD_CATEGORY_SECURITY:
            return "SECURITY";

        case CMD_CATEGORY_POWER:
            return "POWER";

        case CMD_CATEGORY_CHAOS:
            return "CHAOS";

        case CMD_CATEGORY_STORAGE:
            return "STORAGE";

        case CMD_CATEGORY_DEBUG:
            return "DEBUG";

        default:
            return "UNKNOWN";
    }
}

static int CommandService_FindIndex(uint8_t cmd)
{
    uint8_t i;

    for (i = 0U; i < g_command_service.stats.registered_count; i++)
    {
        if (g_command_service.table[i].cmd == cmd)
        {
            return (int)i;
        }
    }

    return -1;
}

static void CommandService_CountCategory(uint8_t category)
{
    switch (category)
    {
        case CMD_CATEGORY_SYSTEM:
            g_command_service.stats.system_cmd_count++;
            break;

        case CMD_CATEGORY_DIAG:
            g_command_service.stats.diag_cmd_count++;
            break;

        case CMD_CATEGORY_IMU:
            g_command_service.stats.imu_cmd_count++;
            break;

        case CMD_CATEGORY_CAN:
            g_command_service.stats.can_cmd_count++;
            break;

        case CMD_CATEGORY_PARAM:
            g_command_service.stats.param_cmd_count++;
            break;

        case CMD_CATEGORY_BOOT:
            g_command_service.stats.boot_cmd_count++;
            break;

        case CMD_CATEGORY_SECURITY:
            g_command_service.stats.security_cmd_count++;
            break;

        case CMD_CATEGORY_POWER:
            g_command_service.stats.power_cmd_count++;
            break;

        case CMD_CATEGORY_CHAOS:
            g_command_service.stats.chaos_cmd_count++;
            break;

        case CMD_CATEGORY_STORAGE:
            g_command_service.stats.storage_cmd_count++;
            break;

        case CMD_CATEGORY_DEBUG:
        default:
            g_command_service.stats.debug_cmd_count++;
            break;
    }
}
