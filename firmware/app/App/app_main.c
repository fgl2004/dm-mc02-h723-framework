#include "app_main.h"

#include "platform_time.h"
#include "platform_uart.h"
#include "platform_reset.h"
#include "board_log.h"

#include "main.h"

#include <stdio.h>

#define ENABLE_SOFTWARE_RESET_TEST   0
#define SOFTWARE_RESET_DELAY_MS      5000U

#define ENABLE_HARDFAULT_TEST        0
#define HARDFAULT_TEST_DELAY_MS      5000U

#define ENABLE_DWT_TEST              0

static PlatformResetInfo_t g_reset_info;

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

    printf("========================================\r\n");
}

void App_Run(void)
{
    BoardLog_Info("[RUN] uptime = %lu ms\r\n", PlatformTime_GetMs());

    App_RunSoftwareResetTest();
    App_RunHardFaultTest();

    PlatformTime_DelayMs(1000U);
}