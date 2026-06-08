#include "data_router.h"

#include "stream_manager.h"
#include "block_manager.h"
#include "board_log.h"

#include <string.h>

typedef struct
{
    uint8_t initialized;
    DataRouterStats_t stats;
} DataRouterContext_t;

static DataRouterContext_t g_data_router;

void DataRouter_Init(void)
{
    memset(&g_data_router, 0, sizeof(g_data_router));

    g_data_router.initialized = 1U;
    g_data_router.stats.init_count++;

    BoardLog_Info("DataRouter init OK\r\n");
}

int DataRouter_HandleData(const ProtocolFrame_t *frame)
{
    int ret;

    if (frame == 0)
    {
        g_data_router.stats.invalid_param_count++;
        g_data_router.stats.last_error = DATA_ROUTER_INVALID_PARAM;
        return DATA_ROUTER_INVALID_PARAM;
    }

    if (g_data_router.initialized == 0U)
    {
        g_data_router.stats.not_initialized_count++;
        g_data_router.stats.last_error = DATA_ROUTER_NOT_INITIALIZED;
        return DATA_ROUTER_NOT_INITIALIZED;
    }

    g_data_router.stats.data_frame_count++;
    g_data_router.stats.last_type = frame->type;
    g_data_router.stats.last_cmd = frame->cmd;

    switch (frame->cmd)
    {
        case STREAM_MANAGER_DATA_CMD:
            ret = StreamManager_HandleRxData(frame);
            if (ret == STREAM_MANAGER_OK)
            {
                g_data_router.stats.stream_data_count++;
                g_data_router.stats.last_error = DATA_ROUTER_OK;
                return DATA_ROUTER_OK;
            }
            g_data_router.stats.route_error_count++;
            g_data_router.stats.last_error = DATA_ROUTER_ERROR;
            return DATA_ROUTER_ERROR;

        case BLOCK_MANAGER_DATA_CMD:
            ret = BlockManager_HandleRxData(frame);
            if (ret == BLOCK_MANAGER_OK)
            {
                g_data_router.stats.block_data_count++;
                g_data_router.stats.last_error = DATA_ROUTER_OK;
                return DATA_ROUTER_OK;
            }
            g_data_router.stats.route_error_count++;
            g_data_router.stats.last_error = DATA_ROUTER_ERROR;
            return DATA_ROUTER_ERROR;

        default:
            g_data_router.stats.unknown_data_cmd_count++;
            g_data_router.stats.last_error = DATA_ROUTER_UNKNOWN_CMD;
            return DATA_ROUTER_UNKNOWN_CMD;
    }
}

int DataRouter_HandleAck(const ProtocolFrame_t *frame)
{
    int ret;

    if (frame == 0)
    {
        g_data_router.stats.invalid_param_count++;
        g_data_router.stats.last_error = DATA_ROUTER_INVALID_PARAM;
        return DATA_ROUTER_INVALID_PARAM;
    }

    if (g_data_router.initialized == 0U)
    {
        g_data_router.stats.not_initialized_count++;
        g_data_router.stats.last_error = DATA_ROUTER_NOT_INITIALIZED;
        return DATA_ROUTER_NOT_INITIALIZED;
    }

    g_data_router.stats.ack_frame_count++;
    g_data_router.stats.last_type = frame->type;
    g_data_router.stats.last_cmd = frame->cmd;

    switch (frame->cmd)
    {
        case BLOCK_MANAGER_DATA_CMD:
            ret = BlockManager_HandleRxAck(frame);
            if (ret == BLOCK_MANAGER_OK)
            {
                g_data_router.stats.block_ack_count++;
                g_data_router.stats.last_error = DATA_ROUTER_OK;
                return DATA_ROUTER_OK;
            }
            g_data_router.stats.route_error_count++;
            g_data_router.stats.last_error = DATA_ROUTER_ERROR;
            return DATA_ROUTER_ERROR;

        default:
            g_data_router.stats.unknown_ack_cmd_count++;
            g_data_router.stats.last_error = DATA_ROUTER_UNKNOWN_CMD;
            return DATA_ROUTER_UNKNOWN_CMD;
    }
}

int DataRouter_HandleWindowAck(const ProtocolFrame_t *frame)
{
    int ret;

    if (frame == 0)
    {
        g_data_router.stats.invalid_param_count++;
        g_data_router.stats.last_error = DATA_ROUTER_INVALID_PARAM;
        return DATA_ROUTER_INVALID_PARAM;
    }

    if (g_data_router.initialized == 0U)
    {
        g_data_router.stats.not_initialized_count++;
        g_data_router.stats.last_error = DATA_ROUTER_NOT_INITIALIZED;
        return DATA_ROUTER_NOT_INITIALIZED;
    }

    g_data_router.stats.window_ack_frame_count++;
    g_data_router.stats.last_type = frame->type;
    g_data_router.stats.last_cmd = frame->cmd;

    switch (frame->cmd)
    {
        case BLOCK_MANAGER_DATA_CMD:
            ret = BlockManager_HandleRxWindowAck(frame);
            if (ret == BLOCK_MANAGER_OK)
            {
                g_data_router.stats.block_window_ack_count++;
                g_data_router.stats.last_error = DATA_ROUTER_OK;
                return DATA_ROUTER_OK;
            }
            g_data_router.stats.route_error_count++;
            g_data_router.stats.last_error = DATA_ROUTER_ERROR;
            return DATA_ROUTER_ERROR;

        default:
            g_data_router.stats.unknown_window_ack_cmd_count++;
            g_data_router.stats.last_error = DATA_ROUTER_UNKNOWN_CMD;
            return DATA_ROUTER_UNKNOWN_CMD;
    }
}

const DataRouterStats_t *DataRouter_GetStats(void)
{
    return &g_data_router.stats;
}

void DataRouter_ResetStats(void)
{
    memset(&g_data_router.stats, 0, sizeof(g_data_router.stats));
}

void DataRouter_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("DataRouter Stats:\r\n");
    BoardLog_Info("  initialized                 = %u\r\n", g_data_router.initialized);
    BoardLog_Info("  init_count                  = %lu\r\n", g_data_router.stats.init_count);
    BoardLog_Info("  data_frame_count            = %lu\r\n", g_data_router.stats.data_frame_count);
    BoardLog_Info("  ack_frame_count             = %lu\r\n", g_data_router.stats.ack_frame_count);
    BoardLog_Info("  window_ack_frame_count      = %lu\r\n", g_data_router.stats.window_ack_frame_count);
    BoardLog_Info("  stream_data_count           = %lu\r\n", g_data_router.stats.stream_data_count);
    BoardLog_Info("  block_data_count            = %lu\r\n", g_data_router.stats.block_data_count);
    BoardLog_Info("  block_ack_count             = %lu\r\n", g_data_router.stats.block_ack_count);
    BoardLog_Info("  block_window_ack_count      = %lu\r\n", g_data_router.stats.block_window_ack_count);
    BoardLog_Info("  unknown_data_cmd_count      = %lu\r\n", g_data_router.stats.unknown_data_cmd_count);
    BoardLog_Info("  unknown_ack_cmd_count       = %lu\r\n", g_data_router.stats.unknown_ack_cmd_count);
    BoardLog_Info("  unknown_window_ack_count    = %lu\r\n", g_data_router.stats.unknown_window_ack_cmd_count);
    BoardLog_Info("  invalid_param_count         = %lu\r\n", g_data_router.stats.invalid_param_count);
    BoardLog_Info("  not_initialized_count       = %lu\r\n", g_data_router.stats.not_initialized_count);
    BoardLog_Info("  route_error_count           = %lu\r\n", g_data_router.stats.route_error_count);
    BoardLog_Info("  last_type                   = 0x%02X\r\n", g_data_router.stats.last_type);
    BoardLog_Info("  last_cmd                    = 0x%02X\r\n", g_data_router.stats.last_cmd);
    BoardLog_Info("  last_error                  = %d\r\n", g_data_router.stats.last_error);
}
