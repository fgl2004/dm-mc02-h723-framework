#ifndef IMU_APP_H
#define IMU_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "state_machine.h"
#include "bsp_bmi088.h"
#include "attitude_estimator.h"

/*
 * Stage 4 IMU App.
 *
 * Design:
 *   BMI088 BSP       -> raw sensor access
 *   AttitudeEstimator -> 6-axis complementary filter
 *   ImuApp           -> state machine, sampling, command handlers, StreamManager output
 */

#ifndef IMU_APP_SAMPLE_PERIOD_MS
#define IMU_APP_SAMPLE_PERIOD_MS             10U
#endif

#ifndef IMU_APP_EVENT_PERIOD_MS
#define IMU_APP_EVENT_PERIOD_MS              100U
#endif

#ifndef IMU_APP_DEFAULT_EVENT_RATE_HZ
#define IMU_APP_DEFAULT_EVENT_RATE_HZ        10U
#endif

typedef enum
{
    IMU_APP_OK = 0,
    IMU_APP_ERROR = -1,
    IMU_APP_INVALID_PARAM = -2,
    IMU_APP_INVALID_STATE = -3,
    IMU_APP_BSP_ERROR = -4,
    IMU_APP_ALGO_ERROR = -5,
    IMU_APP_NOT_INITIALIZED = -6
} ImuAppResult_t;

typedef enum
{
    IMU_APP_STATE_UNINIT = 0,
    IMU_APP_STATE_IDLE,
    IMU_APP_STATE_READY,
    IMU_APP_STATE_RUNNING,
    IMU_APP_STATE_CALIBRATING,
    IMU_APP_STATE_ERROR
} ImuAppState_t;

typedef enum
{
    IMU_APP_EVT_INIT = 1,
    IMU_APP_EVT_PROBE_OK,
    IMU_APP_EVT_PROBE_FAIL,
    IMU_APP_EVT_START,
    IMU_APP_EVT_STOP,
    IMU_APP_EVT_CALIB_START,
    IMU_APP_EVT_CALIB_DONE,
    IMU_APP_EVT_ERROR
} ImuAppEvent_t;

/*
 * Local Stage 4 command extension.
 *
 * command_service.h already defines part of 0x30~0x39:
 *   IMU_CMD_GET_RAW, IMU_CMD_GET_FILTERED, IMU_CMD_GET_STATUS,
 *   IMU_CMD_GET_ALGO_STATS, IMU_CMD_SET_SAMPLE_RATE,
 *   IMU_CMD_START_STREAM, IMU_CMD_STOP_STREAM.
 *
 * These three IDs are kept in the same IMU domain and do not conflict with
 * the uploaded command_service.h.
 */
#ifndef IMU_APP_CMD_START
#define IMU_APP_CMD_START                    0x3AU
#endif

#ifndef IMU_APP_CMD_STOP
#define IMU_APP_CMD_STOP                     0x3BU
#endif

#ifndef IMU_APP_CMD_GET_ATTITUDE
#define IMU_APP_CMD_GET_ATTITUDE             0x3CU
#endif

#ifndef IMU_APP_CMD_CLEAR_STATS
#define IMU_APP_CMD_CLEAR_STATS              0x3DU
#endif

typedef enum
{
    IMU_EVENT_STARTED        = 0x90U,
    IMU_EVENT_STOPPED        = 0x91U,
    IMU_EVENT_ERROR          = 0x95U,
    IMU_EVENT_ATTITUDE       = 0x97U,
    IMU_EVENT_RAW_SAMPLE     = 0x98U
} ImuAppEventId_t;

typedef enum
{
    IMU_INFO_APP_ID = 5U
} ImuAppMcuInfoId_t;

typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;

    int16_t gx;
    int16_t gy;
    int16_t gz;

    int16_t temp;
    uint32_t tick_ms;
} ImuRawSample_t;

typedef struct
{
    float ax_g;
    float ay_g;
    float az_g;

    float gx_dps;
    float gy_dps;
    float gz_dps;

    float temp_c;
    uint32_t tick_ms;
} ImuScaledSample_t;

typedef struct
{
    float roll_deg;
    float pitch_deg;
    float yaw_deg;

    float q0;
    float q1;
    float q2;
    float q3;

    uint32_t tick_ms;
} ImuAttitude_t;

typedef struct
{
    uint32_t init_count;
    uint32_t run_count;

    uint32_t start_count;
    uint32_t stop_count;

    uint32_t sample_count;
    uint32_t sample_drop_count;

    uint32_t read_error_count;
    uint32_t bus_error_count;
    uint32_t invalid_sample_count;

    uint32_t attitude_update_count;
    uint32_t attitude_error_count;

    uint32_t event_stream_start_count;
    uint32_t event_stream_stop_count;
    uint32_t event_stream_send_count;
    uint32_t event_stream_drop_count;

    uint32_t register_command_count;
    uint32_t register_command_fail_count;

    uint32_t get_status_count;
    uint32_t get_raw_count;
    uint32_t get_filtered_count;
    uint32_t get_attitude_count;
    uint32_t get_algo_stats_count;
    uint32_t set_sample_rate_count;
    uint32_t clear_stats_count;

    uint32_t last_sample_tick_ms;
    uint32_t last_sample_interval_ms;
    uint32_t max_sample_interval_ms;

    uint32_t sample_period_ms;
    uint32_t event_period_ms;

    uint32_t last_run_us;
    uint32_t max_run_us;
    uint32_t last_read_us;
    uint32_t max_read_us;
    uint32_t last_algo_us;
    uint32_t max_algo_us;

    uint32_t state_transition_count;
    uint32_t state_dispatch_count;
    uint32_t state_error_count;

    uint8_t initialized;
    uint8_t state;
    uint8_t previous_state;
    uint8_t started;
    uint8_t event_stream_enabled;
    uint8_t last_error;
    uint8_t last_event;
} ImuAppStats_t;

void ImuApp_Init(void);
void ImuApp_Run(void);

int ImuApp_Start(void);
int ImuApp_Stop(void);
int ImuApp_StartStream(void);
int ImuApp_StopStream(void);

/* Backward-compatible names. The implementation now uses StreamManager. */
int ImuApp_StartEventStream(void);
int ImuApp_StopEventStream(void);
int ImuApp_SetSamplePeriodMs(uint32_t period_ms);

const ImuAppStats_t *ImuApp_GetStats(void);
const ImuRawSample_t *ImuApp_GetRawSample(void);
const ImuScaledSample_t *ImuApp_GetScaledSample(void);
const ImuAttitude_t *ImuApp_GetAttitude(void);

void ImuApp_ResetStats(void);
void ImuApp_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* IMU_APP_H */
