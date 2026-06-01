#include "platform_fault.h"

#include "main.h"

#include <stdio.h>
#include <string.h>

void PlatformFault_Capture(uint32_t *stack_frame, PlatformFaultInfo_t *info)
{
    if ((stack_frame == NULL) || (info == NULL))
    {
        return;
    }

    memset(info, 0, sizeof(*info));

    info->r0   = stack_frame[0];
    info->r1   = stack_frame[1];
    info->r2   = stack_frame[2];
    info->r3   = stack_frame[3];
    info->r12  = stack_frame[4];
    info->lr   = stack_frame[5];
    info->pc   = stack_frame[6];
    info->xpsr = stack_frame[7];

    info->cfsr  = SCB->CFSR;
    info->hfsr  = SCB->HFSR;
    info->dfsr  = SCB->DFSR;
    info->afsr  = SCB->AFSR;
    info->mmfar = SCB->MMFAR;
    info->bfar  = SCB->BFAR;
}

void PlatformFault_PrintHfsrDecode(uint32_t hfsr)
{
    printf("\r\n");
    printf("HFSR Decode:\r\n");

    if ((hfsr & (1UL << 1)) != 0U)
    {
        printf("  - VECTTBL : Bus fault on vector table read\r\n");
    }

    if ((hfsr & (1UL << 30)) != 0U)
    {
        printf("  - FORCED  : Configurable fault escalated to HardFault\r\n");
    }

    if ((hfsr & (1UL << 31)) != 0U)
    {
        printf("  - DEBUGEVT: Debug event occurred\r\n");
    }

    if (hfsr == 0U)
    {
        printf("  - None\r\n");
    }
}

void PlatformFault_PrintCfsrDecode(uint32_t cfsr)
{
    uint32_t mmfsr = (cfsr & 0x000000FFUL);
    uint32_t bfsr  = (cfsr & 0x0000FF00UL) >> 8;
    uint32_t ufsr  = (cfsr & 0xFFFF0000UL) >> 16;

    printf("\r\n");
    printf("CFSR Decode:\r\n");

    printf("  MMFSR = 0x%02lX\r\n", mmfsr);
    printf("  BFSR  = 0x%02lX\r\n", bfsr);
    printf("  UFSR  = 0x%04lX\r\n", ufsr);

    printf("\r\n");
    printf("MemManage Fault:\r\n");

    if ((cfsr & (1UL << 0)) != 0U)
    {
        printf("  - IACCVIOL : Instruction access violation\r\n");
    }

    if ((cfsr & (1UL << 1)) != 0U)
    {
        printf("  - DACCVIOL : Data access violation\r\n");
    }

    if ((cfsr & (1UL << 3)) != 0U)
    {
        printf("  - MUNSTKERR: MemManage fault on exception return unstacking\r\n");
    }

    if ((cfsr & (1UL << 4)) != 0U)
    {
        printf("  - MSTKERR  : MemManage fault on exception entry stacking\r\n");
    }

    if ((cfsr & (1UL << 5)) != 0U)
    {
        printf("  - MLSPERR  : MemManage fault during lazy FP state preservation\r\n");
    }

    if ((cfsr & (1UL << 7)) != 0U)
    {
        printf("  - MMARVALID: MMFAR holds a valid fault address\r\n");
    }

    if (mmfsr == 0U)
    {
        printf("  - None\r\n");
    }

    printf("\r\n");
    printf("BusFault:\r\n");

    if ((cfsr & (1UL << 8)) != 0U)
    {
        printf("  - IBUSERR    : Instruction bus error\r\n");
    }

    if ((cfsr & (1UL << 9)) != 0U)
    {
        printf("  - PRECISERR  : Precise data bus error\r\n");
    }

    if ((cfsr & (1UL << 10)) != 0U)
    {
        printf("  - IMPRECISERR: Imprecise data bus error\r\n");
    }

    if ((cfsr & (1UL << 11)) != 0U)
    {
        printf("  - UNSTKERR   : BusFault on exception return unstacking\r\n");
    }

    if ((cfsr & (1UL << 12)) != 0U)
    {
        printf("  - STKERR     : BusFault on exception entry stacking\r\n");
    }

    if ((cfsr & (1UL << 13)) != 0U)
    {
        printf("  - LSPERR     : BusFault during lazy FP state preservation\r\n");
    }

    if ((cfsr & (1UL << 15)) != 0U)
    {
        printf("  - BFARVALID  : BFAR holds a valid fault address\r\n");
    }

    if (bfsr == 0U)
    {
        printf("  - None\r\n");
    }

    printf("\r\n");
    printf("UsageFault:\r\n");

    if ((cfsr & (1UL << 16)) != 0U)
    {
        printf("  - UNDEFINSTR : Undefined instruction\r\n");
    }

    if ((cfsr & (1UL << 17)) != 0U)
    {
        printf("  - INVSTATE   : Invalid EPSR/T-bit state\r\n");
    }

    if ((cfsr & (1UL << 18)) != 0U)
    {
        printf("  - INVPC      : Invalid PC load / EXC_RETURN\r\n");
    }

    if ((cfsr & (1UL << 19)) != 0U)
    {
        printf("  - NOCP       : No coprocessor\r\n");
    }

    if ((cfsr & (1UL << 24)) != 0U)
    {
        printf("  - UNALIGNED  : Unaligned memory access\r\n");
    }

    if ((cfsr & (1UL << 25)) != 0U)
    {
        printf("  - DIVBYZERO  : Divide by zero\r\n");
    }

    if (ufsr == 0U)
    {
        printf("  - None\r\n");
    }
}

void PlatformFault_PrintInfo(const PlatformFaultInfo_t *info)
{
    if (info == NULL)
    {
        return;
    }

    printf("\r\n");
    printf("========== HardFault ==========\r\n");
    printf("R0   = 0x%08lX\r\n", info->r0);
    printf("R1   = 0x%08lX\r\n", info->r1);
    printf("R2   = 0x%08lX\r\n", info->r2);
    printf("R3   = 0x%08lX\r\n", info->r3);
    printf("R12  = 0x%08lX\r\n", info->r12);
    printf("LR   = 0x%08lX\r\n", info->lr);
    printf("PC   = 0x%08lX\r\n", info->pc);
    printf("xPSR = 0x%08lX\r\n", info->xpsr);

    printf("\r\n");
    printf("CFSR = 0x%08lX\r\n", info->cfsr);
    printf("HFSR = 0x%08lX\r\n", info->hfsr);
    printf("DFSR = 0x%08lX\r\n", info->dfsr);
    printf("AFSR = 0x%08lX\r\n", info->afsr);
    printf("MMFAR= 0x%08lX\r\n", info->mmfar);
    printf("BFAR = 0x%08lX\r\n", info->bfar);

    PlatformFault_PrintHfsrDecode(info->hfsr);
    PlatformFault_PrintCfsrDecode(info->cfsr);

    printf("================================\r\n");
}

void PlatformFault_HandlerC(uint32_t *stack_frame)
{
    PlatformFaultInfo_t info;

    PlatformFault_Capture(stack_frame, &info);
    PlatformFault_PrintInfo(&info);

    /*
     * Stay here for debugging.
     * Future improvement:
     * - Save info to blackbox
     * - Trigger software reset
     * - Print previous fault after reboot
     */
    while (1)
    {
    }
}