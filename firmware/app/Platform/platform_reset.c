#include "platform_reset.h"

#include "main.h"

#include <stdio.h>
#include <string.h>

static PlatformResetCause_t PlatformReset_DetectPrimaryCause(const PlatformResetInfo_t *info)
{
    if (info == NULL)
    {
        return PLATFORM_RESET_CAUSE_UNKNOWN;
    }

    /*
     * Reset flags are not mutually exclusive.
     * PINRST may be set together with other flags.
     *
     * Therefore, watchdog and software reset should have higher diagnostic
     * priority than PINRST.
     */
    if (info->iwdg_reset != 0U)
    {
        return PLATFORM_RESET_CAUSE_IWDG;
    }

    if (info->wwdg_reset != 0U)
    {
        return PLATFORM_RESET_CAUSE_WWDG;
    }

    if (info->software_reset != 0U)
    {
        return PLATFORM_RESET_CAUSE_SOFTWARE;
    }

    if (info->bor_reset != 0U)
    {
        return PLATFORM_RESET_CAUSE_BOR;
    }

    if (info->por_reset != 0U)
    {
        return PLATFORM_RESET_CAUSE_POR;
    }

    if (info->pin_reset != 0U)
    {
        return PLATFORM_RESET_CAUSE_PIN;
    }

    return PLATFORM_RESET_CAUSE_UNKNOWN;
}

void PlatformReset_Capture(PlatformResetInfo_t *info)
{
    if (info == NULL)
    {
        return;
    }

    memset(info, 0, sizeof(*info));

    info->pin_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0U) ? 1U : 0U;
    info->por_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0U) ? 1U : 0U;

#ifdef RCC_FLAG_BORRST
    info->bor_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != 0U) ? 1U : 0U;
#else
    info->bor_reset = 0U;
#endif

    info->software_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != 0U) ? 1U : 0U;

#ifdef RCC_FLAG_IWDG1RST
    info->iwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST) != 0U) ? 1U : 0U;
#elif defined(RCC_FLAG_IWDGRST)
    info->iwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0U) ? 1U : 0U;
#else
    info->iwdg_reset = 0U;
#endif

#ifdef RCC_FLAG_WWDG1RST
    info->wwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDG1RST) != 0U) ? 1U : 0U;
#elif defined(RCC_FLAG_WWDGRST)
    info->wwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != 0U) ? 1U : 0U;
#else
    info->wwdg_reset = 0U;
#endif

    info->primary_cause = PlatformReset_DetectPrimaryCause(info);
}

void PlatformReset_ClearFlags(void)
{
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

void PlatformReset_SoftwareReset(void)
{
    NVIC_SystemReset();
}

uint8_t PlatformReset_IsSoftwareReset(const PlatformResetInfo_t *info)
{
    if (info == NULL)
    {
        return 0U;
    }

    return (info->primary_cause == PLATFORM_RESET_CAUSE_SOFTWARE) ? 1U : 0U;
}

const char *PlatformReset_CauseToString(PlatformResetCause_t cause)
{
    switch (cause)
    {
        case PLATFORM_RESET_CAUSE_PIN:
            return "Pin Reset";

        case PLATFORM_RESET_CAUSE_POR:
            return "Power On / Power Down Reset";

        case PLATFORM_RESET_CAUSE_BOR:
            return "Brown-out Reset";

        case PLATFORM_RESET_CAUSE_SOFTWARE:
            return "Software Reset";

        case PLATFORM_RESET_CAUSE_IWDG:
            return "Independent Watchdog Reset";

        case PLATFORM_RESET_CAUSE_WWDG:
            return "Window Watchdog Reset";

        case PLATFORM_RESET_CAUSE_UNKNOWN:
        default:
            return "Unknown Reset";
    }
}

void PlatformReset_PrintInfo(const PlatformResetInfo_t *info)
{
    if (info == NULL)
    {
        return;
    }

    printf("----------------------------------------\r\n");
    printf(" Reset Flags:\r\n");
    printf("  PINRST  : %s\r\n", info->pin_reset ? "SET" : "RESET");
    printf("  PORRST  : %s\r\n", info->por_reset ? "SET" : "RESET");
    printf("  BORRST  : %s\r\n", info->bor_reset ? "SET" : "RESET");
    printf("  SFTRST  : %s\r\n", info->software_reset ? "SET" : "RESET");
    printf("  IWDGRST : %s\r\n", info->iwdg_reset ? "SET" : "RESET");
    printf("  WWDGRST : %s\r\n", info->wwdg_reset ? "SET" : "RESET");

    printf("\r\n");
    printf(" Primary Reset Cause: %s\r\n",
           PlatformReset_CauseToString(info->primary_cause));
}