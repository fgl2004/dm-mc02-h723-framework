#include "app_main.h"



#include "platform_time.h"
#include "platform_uart.h"
#include "platform_reset.h"
#include "board_log.h"
#include "ring_buffer.h"
#include "crc16.h"
#include "state_machine.h"
#include "uart_rx_consumer.h"
#include "protocol_frame.h"
#include "command_manager.h"
#include "protocol_manager.h"
#include "mcu_info_app.h"


#include <stdio.h>

#define ENABLE_SOFTWARE_RESET_TEST   0
#define SOFTWARE_RESET_DELAY_MS      5000U

#define ENABLE_HARDFAULT_TEST        0
#define HARDFAULT_TEST_DELAY_MS      5000U

#define ENABLE_DWT_TEST              0

#define ENABLE_RING_BUFFER_TEST      0

#define ENABLE_CRC16_TEST            0

#define ENABLE_STATE_MACHINE_TEST    0



#define ENABLE_UART_RX_DUMP_TEST       0
#define ENABLE_UART_RX_STATS_REPORT    0
#define UART_RX_STATS_PERIOD_MS        100U

#define ENABLE_UART_RX_SLOW_FAST_TEST   0

#define ENABLE_PROTOCOL_FRAME_TEST     0

#define ENABLE_PROTOCOL_MANAGER_TEST     1

#define ENABLE_MCU_INFO_APP_EVENT_TEST     1

static PlatformResetInfo_t g_reset_info;

typedef enum
{
    APP_TEST_STATE_IDLE = 0,
    APP_TEST_STATE_RUNNING,
    APP_TEST_STATE_ERROR
} AppTestState_t;

typedef enum
{
    APP_TEST_EVENT_START = 1,
    APP_TEST_EVENT_STOP,
    APP_TEST_EVENT_ERROR
} AppTestEvent_t;

typedef struct
{
    StateMachine_t sm;
    uint32_t enter_idle_count;
    uint32_t enter_running_count;
    uint32_t enter_error_count;
} AppTestStateMachineCtx_t;

static AppTestStateMachineCtx_t g_sm_test_ctx;

static void App_RunMcuInfoEventTest(void)
{
#if ENABLE_MCU_INFO_APP_EVENT_TEST
    static uint8_t posted = 0U;
    static const uint8_t test_payload[] = { 'T', 'E', 'S', 'T' };

    if ((posted == 0U) && (PlatformTime_GetMs() > 3000U))
    {
        posted = 1U;

        (void)McuInfoApp_PostEvent(MCU_INFO_APP_ID_USER,
                                   MCU_INFO_EVENT_APP_MESSAGE,
                                   test_payload,
                                   (uint16_t)sizeof(test_payload));
    }
#endif
}
static void App_TestProtocolFrame(void)
{
    int ret;

    BoardLog_PrintSeparator();
    BoardLog_Info("ProtocolFrame Test Start\r\n");

    ret = ProtocolFrame_RunSelfTest();

    if (ret == 0)
    {
        BoardLog_Info("ProtocolFrame self test = PASS\r\n");
    }
    else
    {
        BoardLog_Error("ProtocolFrame self test = FAIL, ret=%d\r\n", ret);
    }

    BoardLog_Info("ProtocolFrame Test End\r\n");
}

static void App_ProcessUartRxTest(void)
{
    uint8_t buf[128];
    uint16_t len;

    /*
     * This function simulates the future ProtocolManager consuming
     * bytes from UART RX RingBuffer.
     *
     * In high-speed stress test, do not print every received byte,
     * otherwise UART TX printf will heavily disturb UART RX measurement.
     */
    len = PlatformUart_ReadRx(buf, sizeof(buf));

#if ENABLE_UART_RX_DUMP_TEST
    if (len > 0U)
    {
        BoardLog_Info("UART RX: len=%u, data=", len);

        for (uint16_t i = 0U; i < len; i++)
        {
            printf("%02X ", buf[i]);
        }

        printf("\r\n");
    }
#else
    (void)len;
#endif
}

static void App_ReportUartRxStatsPeriodically(void)
{
#if ENABLE_UART_RX_STATS_REPORT
    static uint32_t last_report_ms = 0U;
    uint32_t now;
    PlatformUartRxSnapshot_t s;
    const UartRxConsumerStats_t *c;

    now = PlatformTime_GetMs();

    if ((now - last_report_ms) < UART_RX_STATS_PERIOD_MS)
    {
        return;
    }

    last_report_ms = now;

    PlatformUart_GetRxSnapshot(&s);
    c = UartRxConsumer_GetStats();

    printf("@UARTSTAT,t=%lu,rx_bytes=%lu,avail=%u,free=%u,rb_write=%lu,rb_read=%lu,overflow=%lu,rb_overflow=%lu,high=%u,half=%lu,full=%lu,idle=%lu,err=%lu,consumer_mode=%lu,consumer_read=%lu,consumer_mismatch=%lu,consumer_drop=%lu,consumer_expected=%u,consumer_last_actual=%u,consumer_last_expected=%u\r\n",
           now,
           s.rx_bytes,
           s.rx_ring_available,
           s.rx_ring_free,
           s.rb_write_bytes,
           s.rb_read_bytes,
           s.rx_ring_overflow,
           s.rb_overflow_count,
           s.rb_high_watermark,
           s.rx_half_count,
           s.rx_full_count,
           s.rx_idle_count,
           s.rx_error_count,
           c->mode,
           c->read_bytes,
           c->mismatch_count,
           c->estimated_drop_bytes,
           c->expected_counter,
           c->last_actual,
           c->last_expected);
#endif
}

static void AppTest_OnEnterIdle(void *ctx)
{
    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx != NULL)
    {
        test_ctx->enter_idle_count++;
    }

    BoardLog_Info("StateMachine: enter IDLE\r\n");
}

static void AppTest_OnEnterRunning(void *ctx)
{
    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx != NULL)
    {
        test_ctx->enter_running_count++;
    }

    BoardLog_Info("StateMachine: enter RUNNING\r\n");
}

static void AppTest_OnEnterError(void *ctx)
{
    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx != NULL)
    {
        test_ctx->enter_error_count++;
    }

    BoardLog_Info("StateMachine: enter ERROR\r\n");
}

static int AppTest_IdleOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    (void)event_data;

    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == APP_TEST_EVENT_START)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_RUNNING,
                                       PlatformTime_GetMs());
    }

    if (event == APP_TEST_EVENT_ERROR)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int AppTest_RunningOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    (void)event_data;

    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == APP_TEST_EVENT_STOP)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_IDLE,
                                       PlatformTime_GetMs());
    }

    if (event == APP_TEST_EVENT_ERROR)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int AppTest_ErrorOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    (void)event_data;

    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == APP_TEST_EVENT_STOP)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_IDLE,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static const StateDef_t g_app_test_state_table[] =
{
    {
        APP_TEST_STATE_IDLE,
        AppTest_OnEnterIdle,
        NULL,
        AppTest_IdleOnEvent
    },
    {
        APP_TEST_STATE_RUNNING,
        AppTest_OnEnterRunning,
        NULL,
        AppTest_RunningOnEvent
    },
    {
        APP_TEST_STATE_ERROR,
        AppTest_OnEnterError,
        NULL,
        AppTest_ErrorOnEvent
    }
};

static void App_TestStateMachine(void)
{
    int ret;

    BoardLog_PrintSeparator();
    BoardLog_Info("StateMachine Test Start\r\n");

    ret = StateMachine_Init(&g_sm_test_ctx.sm,
                            g_app_test_state_table,
                            (uint16_t)(sizeof(g_app_test_state_table) / sizeof(g_app_test_state_table[0])),
                            APP_TEST_STATE_IDLE,
                            &g_sm_test_ctx,
                            PlatformTime_GetMs());

    BoardLog_Info("StateMachine init ret=%d, state=%u\r\n",
                  ret,
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    StateMachine_Dispatch(&g_sm_test_ctx.sm, APP_TEST_EVENT_START, NULL);
    BoardLog_Info("After START: state=%u\r\n",
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    StateMachine_Dispatch(&g_sm_test_ctx.sm, APP_TEST_EVENT_ERROR, NULL);
    BoardLog_Info("After ERROR: state=%u\r\n",
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    StateMachine_Dispatch(&g_sm_test_ctx.sm, APP_TEST_EVENT_STOP, NULL);
    BoardLog_Info("After STOP: state=%u\r\n",
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    BoardLog_Info("transition_count=%lu, dispatch_count=%lu, error_count=%lu\r\n",
                  StateMachine_GetTransitionCount(&g_sm_test_ctx.sm),
                  StateMachine_GetDispatchCount(&g_sm_test_ctx.sm),
                  StateMachine_GetErrorCount(&g_sm_test_ctx.sm));

    BoardLog_Info("enter_idle=%lu, enter_running=%lu, enter_error=%lu\r\n",
                  g_sm_test_ctx.enter_idle_count,
                  g_sm_test_ctx.enter_running_count,
                  g_sm_test_ctx.enter_error_count);

    BoardLog_Info("StateMachine Test End\r\n");
}

static void App_TestCrc16(void)
{
    uint8_t passed;
    uint16_t crc;

    static const uint8_t test_data[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

    BoardLog_PrintSeparator();
    BoardLog_Info("CRC16 Test Start\r\n");

    crc = Crc16_CcittFalse(test_data, (uint16_t)sizeof(test_data));
    passed = Crc16_SelfTest();

    BoardLog_Info("CRC16-CCITT-FALSE test data: 123456789\r\n");
    BoardLog_Info("CRC16 result = 0x%04X\r\n", crc);
    BoardLog_Info("Expected     = 0x29B1\r\n");
    BoardLog_Info("Self test    = %s\r\n", passed ? "PASS" : "FAIL");

    BoardLog_Info("CRC16 Test End\r\n");
}
static void App_TestRingBuffer(void)
{
    static uint8_t rb_mem[8];
    RingBuffer_t rb;

    uint8_t input1[] = {1, 2, 3, 4, 5};
    uint8_t input2[] = {6, 7, 8, 9};
    uint8_t out[8];
    uint16_t written;
    uint16_t read_len;
    const RingBufferStats_t *stats;

    RingBuffer_Init(&rb, rb_mem, sizeof(rb_mem));

    BoardLog_PrintSeparator();
    BoardLog_Info("RingBuffer Test Start\r\n");

    written = RingBuffer_Write(&rb, input1, sizeof(input1));
    BoardLog_Info("Write input1: written=%u, available=%u, free=%u\r\n",
                  written,
                  RingBuffer_Available(&rb),
                  RingBuffer_Free(&rb));

    read_len = RingBuffer_Read(&rb, out, 3U);
    BoardLog_Info("Read 3 bytes: read=%u, data=%u %u %u, available=%u, free=%u\r\n",
                  read_len,
                  out[0],
                  out[1],
                  out[2],
                  RingBuffer_Available(&rb),
                  RingBuffer_Free(&rb));

    written = RingBuffer_Write(&rb, input2, sizeof(input2));
    BoardLog_Info("Write input2: written=%u, available=%u, free=%u\r\n",
                  written,
                  RingBuffer_Available(&rb),
                  RingBuffer_Free(&rb));

    read_len = RingBuffer_Read(&rb, out, sizeof(out));
    BoardLog_Info("Read all: read=%u\r\n", read_len);

    for (uint16_t i = 0U; i < read_len; i++)
    {
        BoardLog_Info("  out[%u]=%u\r\n", i, out[i]);
    }

    written = RingBuffer_Write(&rb, input1, sizeof(input1));
    written += RingBuffer_Write(&rb, input2, sizeof(input2));

    stats = RingBuffer_GetStats(&rb);

    BoardLog_Info("Overflow test: available=%u, overflow=%lu, high_watermark=%u\r\n",
                  RingBuffer_Available(&rb),
                  stats->overflow_count,
                  stats->high_watermark);

    BoardLog_Info("RingBuffer Test End\r\n");
}

static void App_PrintClockInfo(void)
{
    printf("----------------------------------------\r\n");
    printf(" Clock Info:\r\n");
    printf("  SYSCLK = %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("  HCLK   = %lu Hz\r\n", HAL_RCC_GetHCLKFreq());
    printf("  PCLK1  = %lu Hz\r\n", HAL_RCC_GetPCLK1Freq());
    printf("  PCLK2  = %lu Hz\r\n", HAL_RCC_GetPCLK2Freq());
}

static void App_PrintTickTest(void)
{
    printf("----------------------------------------\r\n");
    printf(" Tick Test:\r\n");
    printf("  HAL_GetTick = %lu ms\r\n", PlatformTime_GetMs());
}

static void App_RunSoftwareResetTest(void)
{
#if ENABLE_SOFTWARE_RESET_TEST
    if ((PlatformReset_IsSoftwareReset(&g_reset_info) == 0U) &&
        (PlatformTime_GetMs() > SOFTWARE_RESET_DELAY_MS))
    {
        BoardLog_Info("[RESET_TEST] Trigger software reset by NVIC_SystemReset()\r\n");
        PlatformTime_DelayMs(100U);
        PlatformReset_SoftwareReset();
    }
#endif
}

static void App_RunHardFaultTest(void)
{
#if ENABLE_HARDFAULT_TEST
    if (PlatformTime_GetMs() > HARDFAULT_TEST_DELAY_MS)
    {
        BoardLog_Info("[FAULT_TEST] Trigger HardFault test\r\n");
        PlatformTime_DelayMs(100U);

        volatile uint32_t *bad_addr = (uint32_t *)0xFFFFFFFFU;
        *bad_addr = 0x12345678U;
    }
#endif
}
static void App_PrintUptimePeriodically(void)
{
    static uint32_t last_log_ms = 0U;
    uint32_t now;

    now = PlatformTime_GetMs();

    if ((now - last_log_ms) >= 1000U)
    {
        last_log_ms = now;
        BoardLog_Info("[RUN] uptime = %lu ms\r\n", now);
    }
}
void App_Init(void)
{
    PlatformUart_Init();
    PlatformTime_Init();
    BoardLog_Init();

    BoardLog_PrintBootBanner();
    if (PlatformUart_StartRxDma() == PLATFORM_UART_OK)
    {
        BoardLog_Info("UART RX DMA started\r\n");
    }
    else
    {
        BoardLog_Error("UART RX DMA start failed\r\n");
    }

    
    PlatformReset_Capture(&g_reset_info);
    PlatformReset_PrintInfo(&g_reset_info);
    PlatformReset_ClearFlags();
#if ENABLE_UART_RX_SLOW_FAST_TEST
    UartRxConsumer_Init();
#endif
    App_PrintClockInfo();
    App_PrintTickTest();

#if ENABLE_DWT_TEST
    PlatformTime_PrintStatus();
    PlatformTime_RunDwtTest();
#endif
	
#if ENABLE_RING_BUFFER_TEST
    App_TestRingBuffer();
#endif

#if ENABLE_CRC16_TEST
    App_TestCrc16();
#endif

#if ENABLE_STATE_MACHINE_TEST
    App_TestStateMachine();
#endif
#if ENABLE_PROTOCOL_FRAME_TEST
    App_TestProtocolFrame();
#endif
#if ENABLE_PROTOCOL_MANAGER_TEST
    McuInfoApp_Init();
    CommandManager_Init();
    ProtocolManager_Init();
#endif
    printf("========================================\r\n");
}

void App_Run(void)
{
#if ENABLE_UART_RX_SLOW_FAST_TEST
        UartRxConsumer_Run();
#endif 
				App_RunMcuInfoEventTest();
#if ENABLE_PROTOCOL_MANAGER_TEST
        McuInfoApp_Run();
        ProtocolManager_Process();
#endif
       
        App_ReportUartRxStatsPeriodically();

        App_RunSoftwareResetTest();
        App_RunHardFaultTest();

		App_PrintUptimePeriodically();
}