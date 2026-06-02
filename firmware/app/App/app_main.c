#include "app_main.h"

#include "platform_time.h"
#include "platform_uart.h"
#include "platform_reset.h"
#include "board_log.h"
#include "ring_buffer.h"
#include "crc16.h"
#include "state_machine.h"

#include "main.h"

#include <stdio.h>

#define ENABLE_SOFTWARE_RESET_TEST   0
#define SOFTWARE_RESET_DELAY_MS      5000U

#define ENABLE_HARDFAULT_TEST        0
#define HARDFAULT_TEST_DELAY_MS      5000U

#define ENABLE_DWT_TEST              0

#define ENABLE_RING_BUFFER_TEST      0

#define ENABLE_CRC16_TEST            0

#define ENABLE_STATE_MACHINE_TEST    0

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

void App_Init(void)
{
    PlatformUart_Init();
    PlatformTime_Init();
    BoardLog_Init();

    BoardLog_PrintBootBanner();

    PlatformReset_Capture(&g_reset_info);
    PlatformReset_PrintInfo(&g_reset_info);
    PlatformReset_ClearFlags();

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
    printf("========================================\r\n");
}

void App_Run(void)
{
    BoardLog_Info("[RUN] uptime = %lu ms\r\n", PlatformTime_GetMs());

    App_RunSoftwareResetTest();
    App_RunHardFaultTest();

    PlatformTime_DelayMs(1000U);
}