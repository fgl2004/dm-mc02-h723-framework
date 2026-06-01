#include "platform_time.h"

#include "main.h"
#include <stdio.h>

void PlatformTime_Init(void)
{
    /*
     * Enable trace and debug block.
     * DWT cycle counter depends on CoreDebug DEMCR.TRCENA.
     */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /*
     * Reset cycle counter.
     */
    DWT->CYCCNT = 0U;

    /*
     * Enable cycle counter.
     */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t PlatformTime_GetMs(void)
{
    return HAL_GetTick();
}

uint32_t PlatformTime_GetCycle(void)
{
    return DWT->CYCCNT;
}

uint32_t PlatformTime_CyclesToUs(uint32_t cycles)
{
    uint32_t hclk_hz = HAL_RCC_GetHCLKFreq();

    if (hclk_hz == 0U)
    {
        return 0U;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / hclk_hz);
}

uint32_t PlatformTime_CyclesToNs(uint32_t cycles)
{
    uint32_t hclk_hz = HAL_RCC_GetHCLKFreq();

    if (hclk_hz == 0U)
    {
        return 0U;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000000ULL) / hclk_hz);
}

uint32_t PlatformTime_ProfileStart(void)
{
    return PlatformTime_GetCycle();
}

uint32_t PlatformTime_ProfileEndCycles(uint32_t start_cycle)
{
    uint32_t end_cycle = PlatformTime_GetCycle();

    /*
     * Unsigned subtraction naturally handles 32-bit counter overflow.
     */
    return end_cycle - start_cycle;
}

uint32_t PlatformTime_ProfileEndUs(uint32_t start_cycle)
{
    uint32_t elapsed_cycles = PlatformTime_ProfileEndCycles(start_cycle);

    return PlatformTime_CyclesToUs(elapsed_cycles);
}

void PlatformTime_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

void PlatformTime_PrintStatus(void)
{
    printf("----------------------------------------\r\n");
    printf(" Platform Time Status:\r\n");
    printf("  HAL Tick     = %lu ms\r\n", PlatformTime_GetMs());
    printf("  HCLK         = %lu Hz\r\n", HAL_RCC_GetHCLKFreq());
    printf("  DEMCR        = 0x%08lX\r\n", CoreDebug->DEMCR);
    printf("  DWT_CTRL     = 0x%08lX\r\n", DWT->CTRL);
    printf("  DWT_CYCCNT   = 0x%08lX\r\n", DWT->CYCCNT);
}

void PlatformTime_RunDwtTest(void)
{
    uint32_t start_cycle;
    uint32_t elapsed_cycles;
    uint32_t elapsed_us;
    uint32_t elapsed_ns;

    start_cycle = PlatformTime_ProfileStart();

    for (volatile uint32_t i = 0U; i < 10000U; i++)
    {
        __NOP();
    }

    elapsed_cycles = PlatformTime_ProfileEndCycles(start_cycle);
    elapsed_us = PlatformTime_CyclesToUs(elapsed_cycles);
    elapsed_ns = PlatformTime_CyclesToNs(elapsed_cycles);

    printf("----------------------------------------\r\n");
    printf(" DWT Cycle Counter Test:\r\n");
    printf("  HCLK   = %lu Hz\r\n", HAL_RCC_GetHCLKFreq());
    printf("  cycles = %lu\r\n", elapsed_cycles);
    printf("  time   = %lu us\r\n", elapsed_us);
    printf("  time   = %lu ns\r\n", elapsed_ns);
}