#include "app_main.h"

#include "platform_time.h"
#include "platform_uart.h"
#include "platform_reset.h"
#include "board_log.h"
#include "ring_buffer.h"


#include "main.h"

#include <stdio.h>

#define ENABLE_SOFTWARE_RESET_TEST   0
#define SOFTWARE_RESET_DELAY_MS      5000U

#define ENABLE_HARDFAULT_TEST        0
#define HARDFAULT_TEST_DELAY_MS      5000U

#define ENABLE_DWT_TEST              1

#define ENABLE_RING_BUFFER_TEST      1

static PlatformResetInfo_t g_reset_info;

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

    printf("========================================\r\n");
}

void App_Run(void)
{
    BoardLog_Info("[RUN] uptime = %lu ms\r\n", PlatformTime_GetMs());

    App_RunSoftwareResetTest();
    App_RunHardFaultTest();

    PlatformTime_DelayMs(1000U);
}