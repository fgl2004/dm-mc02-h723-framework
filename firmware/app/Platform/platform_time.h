#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Initialize platform time module.
 *
 * This function initializes the DWT cycle counter.
 * HAL tick is initialized by HAL_Init().
 */
void PlatformTime_Init(void);

/**
 * @brief Get system tick in milliseconds.
 *
 * @return Current system tick in ms.
 */
uint32_t PlatformTime_GetMs(void);

/**
 * @brief Get raw CPU cycle counter.
 *
 * @return Current DWT cycle counter value.
 */
uint32_t PlatformTime_GetCycle(void);

/**
 * @brief Convert CPU cycles to microseconds.
 *
 * @param cycles CPU cycles.
 * @return Time in microseconds.
 */
uint32_t PlatformTime_CyclesToUs(uint32_t cycles);

/**
 * @brief Convert CPU cycles to nanoseconds.
 *
 * @param cycles CPU cycles.
 * @return Time in nanoseconds.
 */
uint32_t PlatformTime_CyclesToNs(uint32_t cycles);

/**
 * @brief Start a profiling measurement.
 *
 * @return Start cycle counter.
 */
uint32_t PlatformTime_ProfileStart(void);

/**
 * @brief End a profiling measurement and return elapsed cycles.
 *
 * @param start_cycle Start cycle counter.
 * @return Elapsed cycles.
 */
uint32_t PlatformTime_ProfileEndCycles(uint32_t start_cycle);

/**
 * @brief End a profiling measurement and return elapsed microseconds.
 *
 * @param start_cycle Start cycle counter.
 * @return Elapsed time in microseconds.
 */
uint32_t PlatformTime_ProfileEndUs(uint32_t start_cycle);

/**
 * @brief Blocking delay in milliseconds.
 *
 * Bring-up stage wrapper of HAL_Delay().
 *
 * @param ms Delay time in milliseconds.
 */
void PlatformTime_DelayMs(uint32_t ms);

/**
 * @brief Print platform time status.
 *
 * This is mainly used in bring-up stage.
 */
void PlatformTime_PrintStatus(void);

/**
 * @brief Run a simple DWT self-test.
 *
 * This is mainly used in bring-up stage.
 */
void PlatformTime_RunDwtTest(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_TIME_H */