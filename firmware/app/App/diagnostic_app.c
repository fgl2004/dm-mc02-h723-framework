#include "diagnostic_app.h"

#include "mcu_info_app.h"
#include "command_service.h"
#include "command_manager.h"
#include "protocol_manager.h"

#include "platform_time.h"
#include "platform_uart.h"
#include "board_log.h"

#include <stdio.h>
#include <string.h>

#define DIAG_APP_TEXT_BUF_SIZE    128U

typedef struct
{
    uint8_t initialized;
    DiagnosticAppStats_t stats;
    char text_buf[DIAG_APP_TEXT_BUF_SIZE];
} DiagnosticAppContext_t;

static DiagnosticAppContext_t g_diag_app;

static int DiagnosticApp_RegisterCommands(void);

static int DiagnosticApp_HandleGetHealth(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx);

static int DiagnosticApp_HandleGetErrorCounters(const ProtocolFrame_t *req_frame,
                                                CommandManagerResponse_t *resp,
                                                void *ctx);

static int DiagnosticApp_HandleGetBufferStats(const ProtocolFrame_t *req_frame,
                                              CommandManagerResponse_t *resp,
                                              void *ctx);

static int DiagnosticApp_HandleGetLastRecords(const ProtocolFrame_t *req_frame,
                                              CommandManagerResponse_t *resp,
                                              void *ctx);

static int DiagnosticApp_HandleGetTimingStats(const ProtocolFrame_t *req_frame,
                                               CommandManagerResponse_t *resp,
                                               void *ctx);

static int DiagnosticApp_HandleGetPipelineStats(const ProtocolFrame_t *req_frame,
                                                 CommandManagerResponse_t *resp,
                                                 void *ctx);

static int DiagnosticApp_HandleClearCounters(const ProtocolFrame_t *req_frame,
                                             CommandManagerResponse_t *resp,
                                             void *ctx);

static void DiagnosticApp_SetRespText(CommandManagerResponse_t *resp,
                                      uint8_t cmd,
                                      const char *text);

static void DiagnosticApp_SetNack(CommandManagerResponse_t *resp,
                                  uint8_t cmd,
                                  uint8_t error_code);

static const char *DiagnosticApp_HealthName(uint8_t health);
static uint8_t DiagnosticApp_EvaluateHealth(void);

void DiagnosticApp_Init(void)
{
    memset(&g_diag_app, 0, sizeof(g_diag_app));

    g_diag_app.initialized = 1U;
    g_diag_app.stats.init_count++;
    g_diag_app.stats.last_loop_tick_ms = PlatformTime_GetMs();
    g_diag_app.stats.health_state = DIAG_HEALTH_OK;

    (void)DiagnosticApp_RegisterCommands();

    BoardLog_Info("DiagnosticApp init OK\r\n");
}

void DiagnosticApp_Run(void)
{
    uint32_t now;
    uint32_t gap;
    uint32_t start_cycle;
    uint32_t elapsed_us;

    if (g_diag_app.initialized == 0U)
    {
        return;
    }

    start_cycle = PlatformTime_ProfileStart();

    now = PlatformTime_GetMs();

    if (g_diag_app.stats.last_loop_tick_ms != 0U)
    {
        gap = now - g_diag_app.stats.last_loop_tick_ms;

        if (gap > g_diag_app.stats.max_loop_gap_ms)
        {
            g_diag_app.stats.max_loop_gap_ms = gap;
        }
    }

    g_diag_app.stats.last_loop_tick_ms = now;
    g_diag_app.stats.main_loop_count++;
    g_diag_app.stats.run_count++;
    g_diag_app.stats.health_state = DiagnosticApp_EvaluateHealth();

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_diag_app.stats.last_run_us = elapsed_us;

    if (elapsed_us > g_diag_app.stats.max_run_us)
    {
        g_diag_app.stats.max_run_us = elapsed_us;
    }
}

const DiagnosticAppStats_t *DiagnosticApp_GetStats(void)
{
    return &g_diag_app.stats;
}

void DiagnosticApp_ResetStats(void)
{
    memset(&g_diag_app.stats, 0, sizeof(g_diag_app.stats));

    g_diag_app.stats.last_loop_tick_ms = PlatformTime_GetMs();
    g_diag_app.stats.health_state = DIAG_HEALTH_OK;
}

void DiagnosticApp_PrintStats(void)
{
    BoardLog_PrintSeparator();

    BoardLog_Info("DiagnosticApp Stats:\r\n");
    BoardLog_Info("  initialized             = %u\r\n", g_diag_app.initialized);
    BoardLog_Info("  init_count              = %lu\r\n", g_diag_app.stats.init_count);
    BoardLog_Info("  run_count               = %lu\r\n", g_diag_app.stats.run_count);
    BoardLog_Info("  register_command_count  = %lu\r\n", g_diag_app.stats.register_command_count);
    BoardLog_Info("  register_command_fail   = %lu\r\n", g_diag_app.stats.register_command_fail_count);
    BoardLog_Info("  get_health_count        = %lu\r\n", g_diag_app.stats.get_health_count);
    BoardLog_Info("  get_error_count         = %lu\r\n", g_diag_app.stats.get_error_counters_count);
    BoardLog_Info("  get_buffer_count        = %lu\r\n", g_diag_app.stats.get_buffer_stats_count);
    BoardLog_Info("  get_last_count          = %lu\r\n", g_diag_app.stats.get_last_records_count);
    BoardLog_Info("  get_timing_count        = %lu\r\n", g_diag_app.stats.get_timing_stats_count);
    BoardLog_Info("  get_pipeline_count      = %lu\r\n", g_diag_app.stats.get_pipeline_stats_count);
    BoardLog_Info("  clear_counters_count    = %lu\r\n", g_diag_app.stats.clear_counters_count);
    BoardLog_Info("  main_loop_count         = %lu\r\n", g_diag_app.stats.main_loop_count);
    BoardLog_Info("  max_loop_gap_ms         = %lu\r\n", g_diag_app.stats.max_loop_gap_ms);
    BoardLog_Info("  last_run_us             = %lu\r\n", g_diag_app.stats.last_run_us);
    BoardLog_Info("  max_run_us              = %lu\r\n", g_diag_app.stats.max_run_us);
    BoardLog_Info("  health_state            = %u(%s)\r\n",
                  g_diag_app.stats.health_state,
                  DiagnosticApp_HealthName(g_diag_app.stats.health_state));
    BoardLog_Info("  invalid_param_count     = %lu\r\n", g_diag_app.stats.invalid_param_count);
    BoardLog_Info("  error_count             = %lu\r\n", g_diag_app.stats.error_count);
    BoardLog_Info("  last_cmd                = 0x%02X\r\n", g_diag_app.stats.last_cmd);
    BoardLog_Info("  last_event              = 0x%02X\r\n", g_diag_app.stats.last_event);
    BoardLog_Info("  last_error              = 0x%02X\r\n", g_diag_app.stats.last_error);
}

static int DiagnosticApp_RegisterCommands(void)
{
    int result = DIAGNOSTIC_APP_OK;

    if (McuInfoApp_RegisterCommand(DIAG_CMD_GET_HEALTH,
                                   DiagnosticApp_HandleGetHealth,
                                   NULL,
                                   "GET_HEALTH",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        result = DIAGNOSTIC_APP_ERROR;
        g_diag_app.stats.register_command_fail_count++;
    }
    else
    {
        g_diag_app.stats.register_command_count++;
    }

    if (McuInfoApp_RegisterCommand(DIAG_CMD_GET_ERROR_COUNTERS,
                                   DiagnosticApp_HandleGetErrorCounters,
                                   NULL,
                                   "GET_ERROR_COUNTERS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        result = DIAGNOSTIC_APP_ERROR;
        g_diag_app.stats.register_command_fail_count++;
    }
    else
    {
        g_diag_app.stats.register_command_count++;
    }

    if (McuInfoApp_RegisterCommand(DIAG_CMD_GET_BUFFER_STATS,
                                   DiagnosticApp_HandleGetBufferStats,
                                   NULL,
                                   "GET_BUFFER_STATS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        result = DIAGNOSTIC_APP_ERROR;
        g_diag_app.stats.register_command_fail_count++;
    }
    else
    {
        g_diag_app.stats.register_command_count++;
    }

    if (McuInfoApp_RegisterCommand(DIAG_CMD_GET_TIMING_STATS,
                                   DiagnosticApp_HandleGetTimingStats,
                                   NULL,
                                   "GET_TIMING_STATS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        result = DIAGNOSTIC_APP_ERROR;
        g_diag_app.stats.register_command_fail_count++;
    }
    else
    {
        g_diag_app.stats.register_command_count++;
    }

    if (McuInfoApp_RegisterCommand(DIAG_CMD_GET_LAST_RECORDS,
                                   DiagnosticApp_HandleGetLastRecords,
                                   NULL,
                                   "GET_LAST_RECORDS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        result = DIAGNOSTIC_APP_ERROR;
        g_diag_app.stats.register_command_fail_count++;
    }
    else
    {
        g_diag_app.stats.register_command_count++;
    }

    if (McuInfoApp_RegisterCommand(DIAG_CMD_GET_PIPELINE_STATS,
                                   DiagnosticApp_HandleGetPipelineStats,
                                   NULL,
                                   "GET_PIPELINE_STATS",
                                   CMD_FLAG_READ_ONLY) != MCU_INFO_APP_OK)
    {
        result = DIAGNOSTIC_APP_ERROR;
        g_diag_app.stats.register_command_fail_count++;
    }
    else
    {
        g_diag_app.stats.register_command_count++;
    }

    if (McuInfoApp_RegisterCommand(DIAG_CMD_CLEAR_COUNTERS,
                                   DiagnosticApp_HandleClearCounters,
                                   NULL,
                                   "CLEAR_COUNTERS",
                                   CMD_FLAG_WRITE) != MCU_INFO_APP_OK)
    {
        result = DIAGNOSTIC_APP_ERROR;
        g_diag_app.stats.register_command_fail_count++;
    }
    else
    {
        g_diag_app.stats.register_command_count++;
    }

    return result;
}

static int DiagnosticApp_HandleGetHealth(const ProtocolFrame_t *req_frame,
                                         CommandManagerResponse_t *resp,
                                         void *ctx)
{
    const CommandServiceStats_t *cmd_svc;
    const CommandManagerStats_t *cmd_mgr;
    const ProtocolManagerStats_t *proto;
    const McuInfoAppStats_t *mcu;
    PlatformUartRxSnapshot_t uart;
    uint32_t err_total;
    uint32_t drop_total;
    uint32_t rx_ovf;
    uint8_t health;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_diag_app.stats.invalid_param_count++;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    cmd_svc = CommandService_GetStats();
    cmd_mgr = CommandManager_GetStats();
    proto = ProtocolManager_GetStats();
    mcu = McuInfoApp_GetStats();
    PlatformUart_GetRxSnapshot(&uart);

    if ((cmd_svc == NULL) || (cmd_mgr == NULL) || (proto == NULL) || (mcu == NULL))
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    rx_ovf = uart.rx_ring_overflow + uart.rb_overflow_count;

    drop_total = mcu->event_drop_count + cmd_mgr->event_drop_count;

    err_total = cmd_svc->handler_error_count +
                cmd_svc->invalid_param_count +
                cmd_mgr->error_count +
                proto->parser_error_count +
                proto->tx_error_count +
                proto->build_error_count +
                mcu->error_count +
                uart.rx_error_count +
                rx_ovf;

    health = DiagnosticApp_EvaluateHealth();

    memset(g_diag_app.text_buf, 0, sizeof(g_diag_app.text_buf));

    len = snprintf(g_diag_app.text_buf,
                   sizeof(g_diag_app.text_buf),
                   "health=%s,uptime=%lu,loop=%lu,err=%lu,drop=%lu,rx_ovf=%lu,last=0x%02X",
                   DiagnosticApp_HealthName(health),
                   (unsigned long)PlatformTime_GetMs(),
                   (unsigned long)g_diag_app.stats.main_loop_count,
                   (unsigned long)err_total,
                   (unsigned long)drop_total,
                   (unsigned long)rx_ovf,
                   cmd_svc->last_error);

    if (len < 0)
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(g_diag_app.text_buf))
    {
        len = (int)(sizeof(g_diag_app.text_buf) - 1);
        g_diag_app.text_buf[len] = '\0';
    }

    g_diag_app.stats.get_health_count++;
    g_diag_app.stats.last_cmd = req_frame->cmd;
    g_diag_app.stats.last_error = PROTO_ERROR_OK;

    DiagnosticApp_SetRespText(resp, req_frame->cmd, g_diag_app.text_buf);

    return COMMAND_SERVICE_OK;
}

static int DiagnosticApp_HandleGetErrorCounters(const ProtocolFrame_t *req_frame,
                                                CommandManagerResponse_t *resp,
                                                void *ctx)
{
    const CommandServiceStats_t *cmd_svc;
    const CommandManagerStats_t *cmd_mgr;
    const ProtocolManagerStats_t *proto;
    const McuInfoAppStats_t *mcu;
    PlatformUartRxSnapshot_t uart;
    uint32_t rx_ovf;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_diag_app.stats.invalid_param_count++;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    cmd_svc = CommandService_GetStats();
    cmd_mgr = CommandManager_GetStats();
    proto = ProtocolManager_GetStats();
    mcu = McuInfoApp_GetStats();
    PlatformUart_GetRxSnapshot(&uart);

    if ((cmd_svc == NULL) || (cmd_mgr == NULL) || (proto == NULL) || (mcu == NULL))
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    rx_ovf = uart.rx_ring_overflow + uart.rb_overflow_count;

    memset(g_diag_app.text_buf, 0, sizeof(g_diag_app.text_buf));

    len = snprintf(g_diag_app.text_buf,
                   sizeof(g_diag_app.text_buf),
                   "unknown=%lu,handler=%lu,cmd_err=%lu,mcu_err=%lu,parser=%lu,tx=%lu,uart=%lu,ovf=%lu",
                   (unsigned long)cmd_svc->unknown_cmd_count,
                   (unsigned long)cmd_svc->handler_error_count,
                   (unsigned long)cmd_mgr->error_count,
                   (unsigned long)mcu->error_count,
                   (unsigned long)proto->parser_error_count,
                   (unsigned long)(proto->tx_error_count + proto->build_error_count),
                   (unsigned long)uart.rx_error_count,
                   (unsigned long)rx_ovf);

    if (len < 0)
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(g_diag_app.text_buf))
    {
        len = (int)(sizeof(g_diag_app.text_buf) - 1);
        g_diag_app.text_buf[len] = '\0';
    }

    g_diag_app.stats.get_error_counters_count++;
    g_diag_app.stats.last_cmd = req_frame->cmd;
    g_diag_app.stats.last_error = PROTO_ERROR_OK;

    DiagnosticApp_SetRespText(resp, req_frame->cmd, g_diag_app.text_buf);

    return COMMAND_SERVICE_OK;
}

static int DiagnosticApp_HandleGetBufferStats(const ProtocolFrame_t *req_frame,
                                              CommandManagerResponse_t *resp,
                                              void *ctx)
{
    const CommandManagerStats_t *cmd_mgr;
    const McuInfoAppStats_t *mcu;
    PlatformUartRxSnapshot_t uart;
    uint32_t rx_ovf;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_diag_app.stats.invalid_param_count++;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    cmd_mgr = CommandManager_GetStats();
    mcu = McuInfoApp_GetStats();
    PlatformUart_GetRxSnapshot(&uart);

    if ((cmd_mgr == NULL) || (mcu == NULL))
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    rx_ovf = uart.rx_ring_overflow + uart.rb_overflow_count;

    memset(g_diag_app.text_buf, 0, sizeof(g_diag_app.text_buf));

    len = snprintf(g_diag_app.text_buf,
                   sizeof(g_diag_app.text_buf),
                   "uart_avail=%u,uart_free=%u,uart_high=%u,uart_ovf=%lu,mcu_drop=%lu,cmd_drop=%lu",
                   uart.rx_ring_available,
                   uart.rx_ring_free,
                   uart.rb_high_watermark,
                   (unsigned long)rx_ovf,
                   (unsigned long)mcu->event_drop_count,
                   (unsigned long)cmd_mgr->event_drop_count);

    if (len < 0)
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(g_diag_app.text_buf))
    {
        len = (int)(sizeof(g_diag_app.text_buf) - 1);
        g_diag_app.text_buf[len] = '\0';
    }

    g_diag_app.stats.get_buffer_stats_count++;
    g_diag_app.stats.last_cmd = req_frame->cmd;
    g_diag_app.stats.last_error = PROTO_ERROR_OK;

    DiagnosticApp_SetRespText(resp, req_frame->cmd, g_diag_app.text_buf);

    return COMMAND_SERVICE_OK;
}

static int DiagnosticApp_HandleGetLastRecords(const ProtocolFrame_t *req_frame,
                                              CommandManagerResponse_t *resp,
                                              void *ctx)
{
    const CommandServiceStats_t *cmd_svc;
    const CommandManagerStats_t *cmd_mgr;
    const ProtocolManagerStats_t *proto;
    const McuInfoAppStats_t *mcu;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_diag_app.stats.invalid_param_count++;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    cmd_svc = CommandService_GetStats();
    cmd_mgr = CommandManager_GetStats();
    proto = ProtocolManager_GetStats();
    mcu = McuInfoApp_GetStats();

    if ((cmd_svc == NULL) || (cmd_mgr == NULL) || (proto == NULL) || (mcu == NULL))
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    memset(g_diag_app.text_buf, 0, sizeof(g_diag_app.text_buf));

    len = snprintf(g_diag_app.text_buf,
                   sizeof(g_diag_app.text_buf),
                   "last_cmd=0x%02X,last_evt=0x%02X,last_err=0x%02X,last_rx=0x%02X,last_tx=0x%02X",
                   cmd_svc->last_cmd,
                   mcu->last_event_id,
                   cmd_svc->last_error,
                   proto->last_rx_cmd,
                   proto->last_tx_cmd);

    if (len < 0)
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(g_diag_app.text_buf))
    {
        len = (int)(sizeof(g_diag_app.text_buf) - 1);
        g_diag_app.text_buf[len] = '\0';
    }

    g_diag_app.stats.get_last_records_count++;
    g_diag_app.stats.last_cmd = req_frame->cmd;
    g_diag_app.stats.last_event = cmd_mgr->last_event_id;
    g_diag_app.stats.last_error = PROTO_ERROR_OK;

    DiagnosticApp_SetRespText(resp, req_frame->cmd, g_diag_app.text_buf);

    return COMMAND_SERVICE_OK;
}

static int DiagnosticApp_HandleGetTimingStats(const ProtocolFrame_t *req_frame,
                                               CommandManagerResponse_t *resp,
                                               void *ctx)
{
    const ProtocolManagerStats_t *proto;
    const CommandManagerStats_t *cmd_mgr;
    const CommandServiceStats_t *cmd_svc;
    const McuInfoAppStats_t *mcu;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_diag_app.stats.invalid_param_count++;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    proto = ProtocolManager_GetStats();
    cmd_mgr = CommandManager_GetStats();
    cmd_svc = CommandService_GetStats();
    mcu = McuInfoApp_GetStats();

    if ((proto == NULL) || (cmd_mgr == NULL) || (cmd_svc == NULL) || (mcu == NULL))
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    memset(g_diag_app.text_buf, 0, sizeof(g_diag_app.text_buf));

    len = snprintf(g_diag_app.text_buf,
                   sizeof(g_diag_app.text_buf),
                   "loop=%lu,max_gap=%lu,proto_us=%lu,cmd_us=%lu,svc_us=%lu,mcu_us=%lu,diag_us=%lu",
                   (unsigned long)g_diag_app.stats.main_loop_count,
                   (unsigned long)g_diag_app.stats.max_loop_gap_ms,
                   (unsigned long)proto->max_process_us,
                   (unsigned long)cmd_mgr->max_dispatch_us,
                   (unsigned long)cmd_svc->max_dispatch_us,
                   (unsigned long)mcu->max_run_us,
                   (unsigned long)g_diag_app.stats.max_run_us);

    if (len < 0)
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(g_diag_app.text_buf))
    {
        len = (int)(sizeof(g_diag_app.text_buf) - 1);
        g_diag_app.text_buf[len] = '\0';
    }

    g_diag_app.stats.get_timing_stats_count++;
    g_diag_app.stats.last_cmd = req_frame->cmd;
    g_diag_app.stats.last_error = PROTO_ERROR_OK;

    DiagnosticApp_SetRespText(resp, req_frame->cmd, g_diag_app.text_buf);

    return COMMAND_SERVICE_OK;
}

static int DiagnosticApp_HandleGetPipelineStats(const ProtocolFrame_t *req_frame,
                                                 CommandManagerResponse_t *resp,
                                                 void *ctx)
{
    const ProtocolManagerStats_t *proto;
    const ProtocolFrameParserStats_t *parser;
    const CommandServiceStats_t *cmd_svc;
    int len;

    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_diag_app.stats.invalid_param_count++;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    proto = ProtocolManager_GetStats();
    parser = ProtocolManager_GetParserStats();
    cmd_svc = CommandService_GetStats();

    if ((proto == NULL) || (parser == NULL) || (cmd_svc == NULL))
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    memset(g_diag_app.text_buf, 0, sizeof(g_diag_app.text_buf));

    len = snprintf(g_diag_app.text_buf,
                   sizeof(g_diag_app.text_buf),
                   "rx=%lu,parser_ok=%lu,proto_rx=%lu,req=%lu,resp=%lu,nack=%lu,cmd=%lu,unk=%lu,herr=%lu,event=%lu",
                   (unsigned long)proto->rx_bytes_consumed,
                   (unsigned long)parser->frame_ok_count,
                   (unsigned long)proto->frame_received_count,
                   (unsigned long)proto->req_frame_count,
                   (unsigned long)proto->resp_sent_count,
                   (unsigned long)proto->nack_sent_count,
                   (unsigned long)cmd_svc->dispatch_count,
                   (unsigned long)cmd_svc->unknown_cmd_count,
                   (unsigned long)cmd_svc->handler_error_count,
                   (unsigned long)proto->event_sent_count);

    if (len < 0)
    {
        g_diag_app.stats.error_count++;
        DiagnosticApp_SetNack(resp, req_frame->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(g_diag_app.text_buf))
    {
        len = (int)(sizeof(g_diag_app.text_buf) - 1);
        g_diag_app.text_buf[len] = '\0';
    }

    g_diag_app.stats.get_pipeline_stats_count++;
    g_diag_app.stats.last_cmd = req_frame->cmd;
    g_diag_app.stats.last_error = PROTO_ERROR_OK;

    DiagnosticApp_SetRespText(resp, req_frame->cmd, g_diag_app.text_buf);

    return COMMAND_SERVICE_OK;
}

static int DiagnosticApp_HandleClearCounters(const ProtocolFrame_t *req_frame,
                                             CommandManagerResponse_t *resp,
                                             void *ctx)
{
    (void)ctx;

    if ((req_frame == NULL) || (resp == NULL))
    {
        g_diag_app.stats.invalid_param_count++;
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    ProtocolManager_ResetStats();
    ProtocolManager_ResetParserStats();
    CommandManager_ResetStats();
    CommandService_ResetStats();
    McuInfoApp_ResetStats();
    DiagnosticApp_ResetStats();

    g_diag_app.stats.clear_counters_count++;
    g_diag_app.stats.last_cmd = req_frame->cmd;
    g_diag_app.stats.last_error = PROTO_ERROR_OK;

    DiagnosticApp_SetRespText(resp, req_frame->cmd, "cleared=1");

    return COMMAND_SERVICE_OK;
}

static void DiagnosticApp_SetRespText(CommandManagerResponse_t *resp,
                                      uint8_t cmd,
                                      const char *text)
{
    uint16_t len = 0U;

    if (text != NULL)
    {
        len = (uint16_t)strlen(text);
    }

    CommandService_SetResp(resp,
                           cmd,
                           (const uint8_t *)text,
                           len);
}

static void DiagnosticApp_SetNack(CommandManagerResponse_t *resp,
                                  uint8_t cmd,
                                  uint8_t error_code)
{
    CommandService_SetNack(resp, cmd, error_code);
}

static const char *DiagnosticApp_HealthName(uint8_t health)
{
    switch (health)
    {
        case DIAG_HEALTH_OK:
            return "OK";

        case DIAG_HEALTH_WARN:
            return "WARN";

        case DIAG_HEALTH_ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}

static uint8_t DiagnosticApp_EvaluateHealth(void)
{
    const CommandServiceStats_t *cmd_svc;
    const CommandManagerStats_t *cmd_mgr;
    const ProtocolManagerStats_t *proto;
    const McuInfoAppStats_t *mcu;
    PlatformUartRxSnapshot_t uart;
    uint32_t rx_ovf;
    uint32_t drop_total;

    cmd_svc = CommandService_GetStats();
    cmd_mgr = CommandManager_GetStats();
    proto = ProtocolManager_GetStats();
    mcu = McuInfoApp_GetStats();
    PlatformUart_GetRxSnapshot(&uart);

    if ((cmd_svc == NULL) || (cmd_mgr == NULL) || (proto == NULL) || (mcu == NULL))
    {
        return DIAG_HEALTH_ERROR;
    }

    rx_ovf = uart.rx_ring_overflow + uart.rb_overflow_count;
    drop_total = mcu->event_drop_count + cmd_mgr->event_drop_count;

    if ((cmd_svc->handler_error_count > 0U) ||
        (cmd_mgr->error_count > 0U) ||
        (proto->tx_error_count > 0U) ||
        (proto->build_error_count > 0U) ||
        (mcu->error_count > 0U))
    {
        return DIAG_HEALTH_ERROR;
    }

    if ((cmd_svc->unknown_cmd_count > 0U) ||
        (cmd_svc->invalid_param_count > 0U) ||
        (proto->parser_error_count > 0U) ||
        (rx_ovf > 0U) ||
        (drop_total > 0U) ||
        (uart.rx_error_count > 0U))
    {
        return DIAG_HEALTH_WARN;
    }

    return DIAG_HEALTH_OK;
}