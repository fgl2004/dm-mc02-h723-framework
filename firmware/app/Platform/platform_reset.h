#ifndef PLATFORM_RESET_H
#define PLATFORM_RESET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    PLATFORM_RESET_CAUSE_UNKNOWN = 0,
    PLATFORM_RESET_CAUSE_PIN,
    PLATFORM_RESET_CAUSE_POR,
    PLATFORM_RESET_CAUSE_BOR,
    PLATFORM_RESET_CAUSE_SOFTWARE,
    PLATFORM_RESET_CAUSE_IWDG,
    PLATFORM_RESET_CAUSE_WWDG
} PlatformResetCause_t;

typedef struct
{
    uint8_t pin_reset;
    uint8_t por_reset;
    uint8_t bor_reset;
    uint8_t software_reset;
    uint8_t iwdg_reset;
    uint8_t wwdg_reset;

    PlatformResetCause_t primary_cause;
} PlatformResetInfo_t;

void PlatformReset_Capture(PlatformResetInfo_t *info);
void PlatformReset_ClearFlags(void);
void PlatformReset_SoftwareReset(void);

uint8_t PlatformReset_IsSoftwareReset(const PlatformResetInfo_t *info);

const char *PlatformReset_CauseToString(PlatformResetCause_t cause);
void PlatformReset_PrintInfo(const PlatformResetInfo_t *info);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_RESET_H */