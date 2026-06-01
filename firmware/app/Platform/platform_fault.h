#ifndef PLATFORM_FAULT_H
#define PLATFORM_FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;

    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t dfsr;
    uint32_t afsr;
    uint32_t mmfar;
    uint32_t bfar;
} PlatformFaultInfo_t;

void PlatformFault_Capture(uint32_t *stack_frame, PlatformFaultInfo_t *info);
void PlatformFault_PrintInfo(const PlatformFaultInfo_t *info);

void PlatformFault_PrintHfsrDecode(uint32_t hfsr);
void PlatformFault_PrintCfsrDecode(uint32_t cfsr);

/**
 * @brief C-level HardFault handler entry.
 *
 * This function is called from HardFault_Handler assembly wrapper.
 */
void PlatformFault_HandlerC(uint32_t *stack_frame);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_FAULT_H */