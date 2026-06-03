#ifndef DIAGNOSTIC_APP_H
#define DIAGNOSTIC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    DIAGNOSTIC_APP_OK = 0,
    DIAGNOSTIC_APP_ERROR = -1,
    DIAGNOSTIC_APP_INVALID_PARAM = -2
} DiagnosticAppResult_t;

typedef enum
{
    DIAG_HEALTH_OK = 0,
    DIAG_HEALTH_WARN = 1,
    DIAG_HEALTH_ERROR = 2
} DiagnosticHealthState_t;

typedef struct
{
    uint32_t init_count;
    uint32_t run_count;
    uint32_t register_command_count;
    uint32_t register_command_fail_count;

    uint32_t get_health_count;
    uint32_t get_error_counters_count;
    uint32_t get_buffer_stats_count;
    uint32_t get_last_records_count;
    uint32_t get_timing_stats_count;
    uint32_t get_pipeline_stats_count;
    uint32_t clear_counters_count;

    uint32_t main_loop_count;
    uint32_t last_loop_tick_ms;
    uint32_t max_loop_gap_ms;

    uint32_t last_run_us;
    uint32_t max_run_us;

    uint32_t invalid_param_count;
    uint32_t error_count;

    uint8_t health_state;
    uint8_t last_cmd;
    uint8_t last_event;
    uint8_t last_error;
} DiagnosticAppStats_t;

void DiagnosticApp_Init(void);
void DiagnosticApp_Run(void);

const DiagnosticAppStats_t *DiagnosticApp_GetStats(void);
void DiagnosticApp_ResetStats(void);
void DiagnosticApp_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* DIAGNOSTIC_APP_H */