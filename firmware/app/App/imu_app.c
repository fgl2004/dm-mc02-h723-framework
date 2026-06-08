#include "imu_app.h"

#include "mcu_info_app.h"
#include "command_service.h"
#include "stream_manager.h"
#include "platform_time.h"
#include "board_log.h"

#include <stdio.h>
#include <string.h>

#define IMU_APP_RESP_BUF_SIZE                160U
#define IMU_APP_EVENT_BUF_SIZE               64U
#define IMU_APP_STREAM_BUF_SIZE              96U

/*
 * Current BMI088 BSP configuration:
 *   accelerometer range: +-6g
 *   gyroscope range:     +-2000 dps
 */
#define IMU_APP_ACC_RANGE_MG                 6000L
#define IMU_APP_GYRO_RANGE_MDPS              2000000L
#define IMU_APP_RAW_FULL_SCALE               32768L

#define IMU_APP_MIN_SAMPLE_PERIOD_MS         1U
#define IMU_APP_MAX_SAMPLE_PERIOD_MS         1000U
#define IMU_APP_MIN_EVENT_PERIOD_MS          20U

typedef struct
{
    uint8_t initialized;
    uint8_t started;
    uint8_t event_stream_enabled;

    StateMachine_t sm;

    ImuAppStats_t stats;

    ImuRawSample_t raw;
    ImuScaledSample_t scaled;
    ImuAttitude_t attitude;

    uint32_t sample_period_ms;
    uint32_t event_period_ms;

    uint32_t last_run_tick_ms;
    uint32_t last_event_tick_ms;
    uint32_t last_raw_tick_ms;
} ImuAppContext_t;

static ImuAppContext_t g_imu_app;

static int ImuApp_RegisterCommands(void);
static int ImuApp_ReadAndUpdate(void);
static void ImuApp_UpdateStatsState(void);
static void ImuApp_ConvertRawToScaled(const BspBmi088RawData_t *raw, ImuScaledSample_t *scaled);
static void ImuApp_UpdateAttitudeCache(void);
static void ImuApp_PostSimpleEvent(uint8_t event_id, const char *payload);
static void ImuApp_PostAttitudeEvent(void);
static void ImuApp_SendAttitudeStream(void);
static int ImuApp_ParseUint32Payload(const ProtocolFrame_t *req, uint32_t *value);
static int32_t ImuApp_FloatToCenti(float value);
static int32_t ImuApp_FloatToMilli(float value);
static const char *ImuApp_StateToString(uint8_t state);
static void ImuApp_SetLastError(uint8_t error);

static void ImuApp_OnEnterIdle(void *ctx);
static void ImuApp_OnEnterReady(void *ctx);
static void ImuApp_OnEnterRunning(void *ctx);
static void ImuApp_OnEnterCalibrating(void *ctx);
static void ImuApp_OnEnterError(void *ctx);

static int ImuApp_UninitOnEvent(void *ctx, EventId_t event, const void *event_data);
static int ImuApp_IdleOnEvent(void *ctx, EventId_t event, const void *event_data);
static int ImuApp_ReadyOnEvent(void *ctx, EventId_t event, const void *event_data);
static int ImuApp_RunningOnEvent(void *ctx, EventId_t event, const void *event_data);
static int ImuApp_CalibratingOnEvent(void *ctx, EventId_t event, const void *event_data);
static int ImuApp_ErrorOnEvent(void *ctx, EventId_t event, const void *event_data);

static int ImuApp_HandleGetRaw(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleGetFiltered(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleGetStatus(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleGetAlgoStats(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleSetSampleRate(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleStartStream(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleStopStream(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleStart(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleStop(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleGetAttitude(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);
static int ImuApp_HandleClearStats(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx);

static const StateDef_t g_imu_app_state_table[] =
{
    { IMU_APP_STATE_UNINIT,      NULL,                    NULL, ImuApp_UninitOnEvent      },
    { IMU_APP_STATE_IDLE,        ImuApp_OnEnterIdle,       NULL, ImuApp_IdleOnEvent        },
    { IMU_APP_STATE_READY,       ImuApp_OnEnterReady,      NULL, ImuApp_ReadyOnEvent       },
    { IMU_APP_STATE_RUNNING,     ImuApp_OnEnterRunning,    NULL, ImuApp_RunningOnEvent     },
    { IMU_APP_STATE_CALIBRATING, ImuApp_OnEnterCalibrating,NULL, ImuApp_CalibratingOnEvent },
    { IMU_APP_STATE_ERROR,       ImuApp_OnEnterError,      NULL, ImuApp_ErrorOnEvent       }
};

static void ImuApp_UpdateStatsState(void)
{
    g_imu_app.stats.state = (uint8_t)StateMachine_GetState(&g_imu_app.sm);
    g_imu_app.stats.previous_state = (uint8_t)StateMachine_GetPreviousState(&g_imu_app.sm);
    g_imu_app.stats.state_transition_count = StateMachine_GetTransitionCount(&g_imu_app.sm);
    g_imu_app.stats.state_dispatch_count = StateMachine_GetDispatchCount(&g_imu_app.sm);
    g_imu_app.stats.state_error_count = StateMachine_GetErrorCount(&g_imu_app.sm);
    g_imu_app.stats.started = g_imu_app.started;
    g_imu_app.stats.event_stream_enabled = g_imu_app.event_stream_enabled;
    g_imu_app.stats.sample_period_ms = g_imu_app.sample_period_ms;
    g_imu_app.stats.event_period_ms = g_imu_app.event_period_ms;
    g_imu_app.stats.initialized = g_imu_app.initialized;
}

static void ImuApp_SetLastError(uint8_t error)
{
    g_imu_app.stats.last_error = error;
}

void ImuApp_Init(void)
{
    int ret;

    memset(&g_imu_app, 0, sizeof(g_imu_app));

    g_imu_app.sample_period_ms = IMU_APP_SAMPLE_PERIOD_MS;
    g_imu_app.event_period_ms = IMU_APP_EVENT_PERIOD_MS;

    g_imu_app.stats.init_count++;

    AttitudeEstimator_Init();

    ret = StateMachine_Init(&g_imu_app.sm,
                            g_imu_app_state_table,
                            (uint16_t)(sizeof(g_imu_app_state_table) / sizeof(g_imu_app_state_table[0])),
                            IMU_APP_STATE_UNINIT,
                            &g_imu_app,
                            PlatformTime_GetMs());
    if (ret != STATE_MACHINE_OK)
    {
        g_imu_app.initialized = 0U;
        ImuApp_SetLastError((uint8_t)IMU_APP_INVALID_STATE);
        BoardLog_Error("ImuApp state machine init failed, ret=%d\r\n", ret);
        return;
    }

    ret = BspBmi088_Init();
    if (ret == BSP_BMI088_OK)
    {
        (void)StateMachine_Dispatch(&g_imu_app.sm, IMU_APP_EVT_PROBE_OK, NULL);
        g_imu_app.initialized = 1U;
        ImuApp_SetLastError((uint8_t)IMU_APP_OK);
    }
    else
    {
        g_imu_app.stats.bus_error_count++;
        (void)StateMachine_Dispatch(&g_imu_app.sm, IMU_APP_EVT_PROBE_FAIL, NULL);
        g_imu_app.initialized = 1U;
        ImuApp_SetLastError((uint8_t)IMU_APP_BSP_ERROR);
        BoardLog_Error("ImuApp BMI088 init failed, ret=%d\r\n", ret);
    }

    (void)ImuApp_RegisterCommands();

    ImuApp_UpdateStatsState();

    BoardLog_Info("ImuApp init done, state=%s\r\n",
                  ImuApp_StateToString(g_imu_app.stats.state));
}

void ImuApp_Run(void)
{
    uint32_t start_cycle;
    uint32_t elapsed_us;
    uint32_t now;
    uint8_t state;

    if (g_imu_app.initialized == 0U)
    {
        return;
    }

    start_cycle = PlatformTime_ProfileStart();

    g_imu_app.stats.run_count++;

    now = PlatformTime_GetMs();
    state = (uint8_t)StateMachine_GetState(&g_imu_app.sm);

    if ((state == IMU_APP_STATE_RUNNING) && (g_imu_app.started != 0U))
    {
        if ((now - g_imu_app.last_run_tick_ms) >= g_imu_app.sample_period_ms)
        {
            g_imu_app.last_run_tick_ms = now;
            (void)ImuApp_ReadAndUpdate();
        }

        if ((g_imu_app.event_stream_enabled != 0U) &&
            ((now - g_imu_app.last_event_tick_ms) >= g_imu_app.event_period_ms))
        {
            g_imu_app.last_event_tick_ms = now;
            ImuApp_SendAttitudeStream();
        }
    }

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);

    g_imu_app.stats.last_run_us = elapsed_us;
    if (elapsed_us > g_imu_app.stats.max_run_us)
    {
        g_imu_app.stats.max_run_us = elapsed_us;
    }

    ImuApp_UpdateStatsState();
}

int ImuApp_Start(void)
{
    int ret;

    if (g_imu_app.initialized == 0U)
    {
        return IMU_APP_NOT_INITIALIZED;
    }

    ret = StateMachine_Dispatch(&g_imu_app.sm, IMU_APP_EVT_START, NULL);
    if (ret != STATE_MACHINE_OK)
    {
        ImuApp_SetLastError((uint8_t)IMU_APP_INVALID_STATE);
        return IMU_APP_INVALID_STATE;
    }

    g_imu_app.started = 1U;
    g_imu_app.last_run_tick_ms = PlatformTime_GetMs();

    g_imu_app.stats.start_count++;
    ImuApp_SetLastError((uint8_t)IMU_APP_OK);

    ImuApp_PostSimpleEvent(IMU_EVENT_STARTED, "started=1");

    return IMU_APP_OK;
}

int ImuApp_Stop(void)
{
    int ret;

    if (g_imu_app.initialized == 0U)
    {
        return IMU_APP_NOT_INITIALIZED;
    }

    ret = StateMachine_Dispatch(&g_imu_app.sm, IMU_APP_EVT_STOP, NULL);
    if (ret != STATE_MACHINE_OK)
    {
        ImuApp_SetLastError((uint8_t)IMU_APP_INVALID_STATE);
        return IMU_APP_INVALID_STATE;
    }

    g_imu_app.started = 0U;
    g_imu_app.event_stream_enabled = 0U;

    g_imu_app.stats.stop_count++;
    ImuApp_SetLastError((uint8_t)IMU_APP_OK);

    ImuApp_PostSimpleEvent(IMU_EVENT_STOPPED, "started=0");

    return IMU_APP_OK;
}

int ImuApp_StartStream(void)
{
    int ret;

    if (g_imu_app.initialized == 0U)
    {
        return IMU_APP_NOT_INITIALIZED;
    }

    /*
     * Stream output is now routed through StreamManager as TYPE=DATA/CMD=0x70.
     * If the app is not sampling yet, START_STREAM should also start the IMU
     * state machine so the PC can recover the whole function with one button.
     */
    if (g_imu_app.started == 0U)
    {
        ret = ImuApp_Start();
        if (ret != IMU_APP_OK)
        {
            return ret;
        }
    }

    g_imu_app.event_stream_enabled = 1U;
    g_imu_app.last_event_tick_ms = PlatformTime_GetMs();
    g_imu_app.stats.event_stream_start_count++;

    ImuApp_SetLastError((uint8_t)IMU_APP_OK);

    return IMU_APP_OK;
}

int ImuApp_StopStream(void)
{
    if (g_imu_app.initialized == 0U)
    {
        return IMU_APP_NOT_INITIALIZED;
    }

    g_imu_app.event_stream_enabled = 0U;
    g_imu_app.stats.event_stream_stop_count++;

    ImuApp_SetLastError((uint8_t)IMU_APP_OK);

    return IMU_APP_OK;
}

int ImuApp_StartEventStream(void)
{
    /*
     * Backward-compatible API name.
     * New implementation sends stream samples through StreamManager.
     */
    return ImuApp_StartStream();
}

int ImuApp_StopEventStream(void)
{
    /*
     * Backward-compatible API name.
     * New implementation sends stream samples through StreamManager.
     */
    return ImuApp_StopStream();
}

int ImuApp_SetSamplePeriodMs(uint32_t period_ms)
{
    if ((period_ms < IMU_APP_MIN_SAMPLE_PERIOD_MS) ||
        (period_ms > IMU_APP_MAX_SAMPLE_PERIOD_MS))
    {
        ImuApp_SetLastError((uint8_t)IMU_APP_INVALID_PARAM);
        return IMU_APP_INVALID_PARAM;
    }

    g_imu_app.sample_period_ms = period_ms;

    /*
     * Keep event rate at about 10 samples per event, but not faster than
     * IMU_APP_MIN_EVENT_PERIOD_MS.
     */
    g_imu_app.event_period_ms = period_ms * 10U;
    if (g_imu_app.event_period_ms < IMU_APP_MIN_EVENT_PERIOD_MS)
    {
        g_imu_app.event_period_ms = IMU_APP_MIN_EVENT_PERIOD_MS;
    }

    g_imu_app.stats.set_sample_rate_count++;
    ImuApp_SetLastError((uint8_t)IMU_APP_OK);

    return IMU_APP_OK;
}

static int ImuApp_ReadAndUpdate(void)
{
    BspBmi088RawData_t bsp_raw;
    const AttitudeEstimatorState_t *att;
    uint32_t start_cycle;
    uint32_t elapsed_us;
    uint32_t dt_ms;
    float dt_s;
    int ret;

    start_cycle = PlatformTime_ProfileStart();

    ret = BspBmi088_ReadRaw(&bsp_raw);

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_imu_app.stats.last_read_us = elapsed_us;
    if (elapsed_us > g_imu_app.stats.max_read_us)
    {
        g_imu_app.stats.max_read_us = elapsed_us;
    }

    if (ret != BSP_BMI088_OK)
    {
        g_imu_app.stats.read_error_count++;
        g_imu_app.stats.bus_error_count++;
        ImuApp_SetLastError((uint8_t)IMU_APP_BSP_ERROR);

        if (g_imu_app.stats.read_error_count >= 10U)
        {
            (void)StateMachine_Dispatch(&g_imu_app.sm, IMU_APP_EVT_ERROR, NULL);
            ImuApp_PostSimpleEvent(IMU_EVENT_ERROR, "err=read");
        }

        return IMU_APP_BSP_ERROR;
    }

    g_imu_app.raw.ax = bsp_raw.ax;
    g_imu_app.raw.ay = bsp_raw.ay;
    g_imu_app.raw.az = bsp_raw.az;
    g_imu_app.raw.gx = bsp_raw.gx;
    g_imu_app.raw.gy = bsp_raw.gy;
    g_imu_app.raw.gz = bsp_raw.gz;
    g_imu_app.raw.temp = bsp_raw.temp;
    g_imu_app.raw.tick_ms = bsp_raw.tick_ms;

    ImuApp_ConvertRawToScaled(&bsp_raw, &g_imu_app.scaled);

    if (g_imu_app.last_raw_tick_ms == 0U)
    {
        dt_ms = g_imu_app.sample_period_ms;
    }
    else
    {
        dt_ms = bsp_raw.tick_ms - g_imu_app.last_raw_tick_ms;
    }

    g_imu_app.last_raw_tick_ms = bsp_raw.tick_ms;

    if (dt_ms == 0U)
    {
        dt_ms = g_imu_app.sample_period_ms;
    }

    dt_s = ((float)dt_ms) / 1000.0f;

    start_cycle = PlatformTime_ProfileStart();

    ret = AttitudeEstimator_Update6Axis(g_imu_app.scaled.ax_g,
                                        g_imu_app.scaled.ay_g,
                                        g_imu_app.scaled.az_g,
                                        g_imu_app.scaled.gx_dps,
                                        g_imu_app.scaled.gy_dps,
                                        g_imu_app.scaled.gz_dps,
                                        dt_s,
                                        bsp_raw.tick_ms);

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_imu_app.stats.last_algo_us = elapsed_us;
    if (elapsed_us > g_imu_app.stats.max_algo_us)
    {
        g_imu_app.stats.max_algo_us = elapsed_us;
    }

    if (ret != ATTITUDE_ESTIMATOR_OK)
    {
        g_imu_app.stats.attitude_error_count++;
        ImuApp_SetLastError((uint8_t)IMU_APP_ALGO_ERROR);
        return IMU_APP_ALGO_ERROR;
    }

    att = AttitudeEstimator_GetState();
    g_imu_app.attitude.roll_deg = att->roll_deg;
    g_imu_app.attitude.pitch_deg = att->pitch_deg;
    g_imu_app.attitude.yaw_deg = att->yaw_deg;
    g_imu_app.attitude.q0 = att->q0;
    g_imu_app.attitude.q1 = att->q1;
    g_imu_app.attitude.q2 = att->q2;
    g_imu_app.attitude.q3 = att->q3;
    g_imu_app.attitude.tick_ms = bsp_raw.tick_ms;

    g_imu_app.stats.sample_count++;
    g_imu_app.stats.attitude_update_count++;

    g_imu_app.stats.last_sample_interval_ms = dt_ms;
    if (dt_ms > g_imu_app.stats.max_sample_interval_ms)
    {
        g_imu_app.stats.max_sample_interval_ms = dt_ms;
    }

    g_imu_app.stats.last_sample_tick_ms = bsp_raw.tick_ms;

    ImuApp_SetLastError((uint8_t)IMU_APP_OK);

    return IMU_APP_OK;
}

static void ImuApp_ConvertRawToScaled(const BspBmi088RawData_t *raw, ImuScaledSample_t *scaled)
{
    if ((raw == NULL) || (scaled == NULL))
    {
        return;
    }

    scaled->ax_g = ((float)raw->ax * 6.0f) / 32768.0f;
    scaled->ay_g = ((float)raw->ay * 6.0f) / 32768.0f;
    scaled->az_g = ((float)raw->az * 6.0f) / 32768.0f;

    scaled->gx_dps = ((float)raw->gx * 2000.0f) / 32768.0f;
    scaled->gy_dps = ((float)raw->gy * 2000.0f) / 32768.0f;
    scaled->gz_dps = ((float)raw->gz * 2000.0f) / 32768.0f;

    scaled->temp_c = 23.0f + (((float)raw->temp) * 0.125f);
    scaled->tick_ms = raw->tick_ms;
}

static void ImuApp_UpdateAttitudeCache(void)
{
    const AttitudeEstimatorState_t *att;

    att = AttitudeEstimator_GetState();

    g_imu_app.attitude.roll_deg = att->roll_deg;
    g_imu_app.attitude.pitch_deg = att->pitch_deg;
    g_imu_app.attitude.yaw_deg = att->yaw_deg;
    g_imu_app.attitude.q0 = att->q0;
    g_imu_app.attitude.q1 = att->q1;
    g_imu_app.attitude.q2 = att->q2;
    g_imu_app.attitude.q3 = att->q3;
    g_imu_app.attitude.tick_ms = att->last_update_tick_ms;
}

static int32_t ImuApp_FloatToCenti(float value)
{
    if (value >= 0.0f)
    {
        return (int32_t)((value * 100.0f) + 0.5f);
    }

    return (int32_t)((value * 100.0f) - 0.5f);
}

static int32_t ImuApp_FloatToMilli(float value)
{
    if (value >= 0.0f)
    {
        return (int32_t)((value * 1000.0f) + 0.5f);
    }

    return (int32_t)((value * 1000.0f) - 0.5f);
}

static void ImuApp_PostSimpleEvent(uint8_t event_id, const char *payload)
{
    const uint8_t *data = NULL;
    uint16_t len = 0U;

    if (payload != NULL)
    {
        data = (const uint8_t *)payload;
        len = (uint16_t)strlen(payload);
    }

    if (McuInfoApp_PostEvent(IMU_INFO_APP_ID,
                             event_id,
                             data,
                             len) != MCU_INFO_APP_OK)
    {
        g_imu_app.stats.event_stream_drop_count++;
    }

    g_imu_app.stats.last_event = event_id;
}

static void ImuApp_PostAttitudeEvent(void)
{
    char payload[IMU_APP_EVENT_BUF_SIZE];
    int len;

    ImuApp_UpdateAttitudeCache();

    len = snprintf(payload,
                   sizeof(payload),
                   "att,t=%lu,r=%ld,p=%ld,y=%ld",
                   (unsigned long)g_imu_app.attitude.tick_ms,
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.roll_deg),
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.pitch_deg),
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.yaw_deg));

    if (len < 0)
    {
        g_imu_app.stats.event_stream_drop_count++;
        return;
    }

    if (len >= (int)sizeof(payload))
    {
        len = (int)(sizeof(payload) - 1);
        payload[len] = '\0';
    }

    if (McuInfoApp_PostEvent(IMU_INFO_APP_ID,
                             IMU_EVENT_ATTITUDE,
                             (const uint8_t *)payload,
                             (uint16_t)len) == MCU_INFO_APP_OK)
    {
        g_imu_app.stats.event_stream_send_count++;
        g_imu_app.stats.last_event = IMU_EVENT_ATTITUDE;
    }
    else
    {
        g_imu_app.stats.event_stream_drop_count++;
    }
}


static void ImuApp_SendAttitudeStream(void)
{
    char payload[IMU_APP_STREAM_BUF_SIZE];
    int len;

    ImuApp_UpdateAttitudeCache();

    /*
     * PC StreamClient decodes StreamManager payload and MainWindow then tries
     * to parse the sample as ASCII key-value text.
     *
     * Keep this payload short and stable:
     *   t = sensor timestamp ms
     *   r/p/y = roll/pitch/yaw in centi-degrees
     */
    len = snprintf(payload,
                   sizeof(payload),
                   "t=%lu,r=%ld,p=%ld,y=%ld",
                   (unsigned long)g_imu_app.attitude.tick_ms,
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.roll_deg),
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.pitch_deg),
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.yaw_deg));

    if (len < 0)
    {
        g_imu_app.stats.event_stream_drop_count++;
        return;
    }

    if (len >= (int)sizeof(payload))
    {
        len = (int)(sizeof(payload) - 1);
        payload[len] = '\0';
    }

    if (StreamManager_SendSample(STREAM_MANAGER_CHANNEL_IMU,
                                 (const uint8_t *)payload,
                                 (uint16_t)len,
                                 PROTOCOL_TX_PRIORITY_LOW) == STREAM_MANAGER_OK)
    {
        g_imu_app.stats.event_stream_send_count++;
        g_imu_app.stats.last_event = IMU_EVENT_ATTITUDE;
    }
    else
    {
        g_imu_app.stats.event_stream_drop_count++;
    }
}

static const char *ImuApp_StateToString(uint8_t state)
{
    switch (state)
    {
        case IMU_APP_STATE_UNINIT:
            return "UNINIT";
        case IMU_APP_STATE_IDLE:
            return "IDLE";
        case IMU_APP_STATE_READY:
            return "READY";
        case IMU_APP_STATE_RUNNING:
            return "RUNNING";
        case IMU_APP_STATE_CALIBRATING:
            return "CALIBRATING";
        case IMU_APP_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static int ImuApp_ParseUint32Payload(const ProtocolFrame_t *req, uint32_t *value)
{
    uint32_t v = 0U;
    uint16_t i;

    if ((req == NULL) || (value == NULL) || (req->payload_len == 0U))
    {
        return IMU_APP_INVALID_PARAM;
    }

    for (i = 0U; i < req->payload_len; i++)
    {
        uint8_t ch = req->payload[i];

        if ((ch < (uint8_t)'0') || (ch > (uint8_t)'9'))
        {
            return IMU_APP_INVALID_PARAM;
        }

        v = (v * 10U) + (uint32_t)(ch - (uint8_t)'0');
    }

    *value = v;

    return IMU_APP_OK;
}

/* ========================= State machine callbacks ========================= */

static void ImuApp_OnEnterIdle(void *ctx)
{
    (void)ctx;
    g_imu_app.started = 0U;
}

static void ImuApp_OnEnterReady(void *ctx)
{
    (void)ctx;
    g_imu_app.started = 0U;
}

static void ImuApp_OnEnterRunning(void *ctx)
{
    (void)ctx;
    g_imu_app.started = 1U;
}

static void ImuApp_OnEnterCalibrating(void *ctx)
{
    (void)ctx;
}

static void ImuApp_OnEnterError(void *ctx)
{
    (void)ctx;
    g_imu_app.started = 0U;
    g_imu_app.event_stream_enabled = 0U;
}

static int ImuApp_UninitOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    ImuAppContext_t *app = (ImuAppContext_t *)ctx;
    (void)event_data;

    if (app == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == IMU_APP_EVT_PROBE_OK)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_READY,
                                       PlatformTime_GetMs());
    }

    if (event == IMU_APP_EVT_PROBE_FAIL)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int ImuApp_IdleOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    ImuAppContext_t *app = (ImuAppContext_t *)ctx;
    (void)event_data;

    if (app == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == IMU_APP_EVT_PROBE_OK)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_READY,
                                       PlatformTime_GetMs());
    }

    if (event == IMU_APP_EVT_ERROR)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int ImuApp_ReadyOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    ImuAppContext_t *app = (ImuAppContext_t *)ctx;
    (void)event_data;

    if (app == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == IMU_APP_EVT_START)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_RUNNING,
                                       PlatformTime_GetMs());
    }

    if (event == IMU_APP_EVT_CALIB_START)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_CALIBRATING,
                                       PlatformTime_GetMs());
    }

    if (event == IMU_APP_EVT_ERROR)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int ImuApp_RunningOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    ImuAppContext_t *app = (ImuAppContext_t *)ctx;
    (void)event_data;

    if (app == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == IMU_APP_EVT_STOP)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_READY,
                                       PlatformTime_GetMs());
    }

    if (event == IMU_APP_EVT_ERROR)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int ImuApp_CalibratingOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    ImuAppContext_t *app = (ImuAppContext_t *)ctx;
    (void)event_data;

    if (app == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if ((event == IMU_APP_EVT_CALIB_DONE) || (event == IMU_APP_EVT_STOP))
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_READY,
                                       PlatformTime_GetMs());
    }

    if (event == IMU_APP_EVT_ERROR)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int ImuApp_ErrorOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    ImuAppContext_t *app = (ImuAppContext_t *)ctx;
    (void)event_data;

    if (app == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == IMU_APP_EVT_PROBE_OK)
    {
        return StateMachine_Transition(&app->sm,
                                       IMU_APP_STATE_READY,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

/* ========================= Command registration ========================= */

static int ImuApp_RegisterOneCommand(uint8_t cmd,
                                     CommandServiceHandler_t handler,
                                     const char *name,
                                     uint32_t flags)
{
    if (McuInfoApp_RegisterCommand(cmd,
                                   handler,
                                   NULL,
                                   name,
                                   flags) == MCU_INFO_APP_OK)
    {
        g_imu_app.stats.register_command_count++;
        return IMU_APP_OK;
    }

    g_imu_app.stats.register_command_fail_count++;
    return IMU_APP_ERROR;
}

static int ImuApp_RegisterCommands(void)
{
    int ret = IMU_APP_OK;

    if (ImuApp_RegisterOneCommand(IMU_CMD_GET_RAW,
                                  ImuApp_HandleGetRaw,
                                  "IMU_GET_RAW",
                                  CMD_FLAG_READ_ONLY) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_CMD_GET_FILTERED,
                                  ImuApp_HandleGetFiltered,
                                  "IMU_GET_FILTERED",
                                  CMD_FLAG_READ_ONLY) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_CMD_GET_STATUS,
                                  ImuApp_HandleGetStatus,
                                  "IMU_GET_STATUS",
                                  CMD_FLAG_READ_ONLY) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_CMD_GET_ALGO_STATS,
                                  ImuApp_HandleGetAlgoStats,
                                  "IMU_GET_ALGO_STATS",
                                  CMD_FLAG_READ_ONLY) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_CMD_SET_SAMPLE_RATE,
                                  ImuApp_HandleSetSampleRate,
                                  "IMU_SET_SAMPLE_RATE",
                                  CMD_FLAG_WRITE) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_CMD_START_STREAM,
                                  ImuApp_HandleStartStream,
                                  "IMU_START_STREAM",
                                  CMD_FLAG_STREAM_CONTROL) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_CMD_STOP_STREAM,
                                  ImuApp_HandleStopStream,
                                  "IMU_STOP_STREAM",
                                  CMD_FLAG_STREAM_CONTROL) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_APP_CMD_START,
                                  ImuApp_HandleStart,
                                  "IMU_START",
                                  CMD_FLAG_WRITE) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_APP_CMD_STOP,
                                  ImuApp_HandleStop,
                                  "IMU_STOP",
                                  CMD_FLAG_WRITE) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_APP_CMD_GET_ATTITUDE,
                                  ImuApp_HandleGetAttitude,
                                  "IMU_GET_ATTITUDE",
                                  CMD_FLAG_READ_ONLY) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    if (ImuApp_RegisterOneCommand(IMU_APP_CMD_CLEAR_STATS,
                                  ImuApp_HandleClearStats,
                                  "IMU_CLEAR_STATS",
                                  CMD_FLAG_WRITE) != IMU_APP_OK)
    {
        ret = IMU_APP_ERROR;
    }

    return ret;
}

/* ========================= Command handlers ========================= */

static int ImuApp_HandleGetRaw(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    char text[IMU_APP_RESP_BUF_SIZE];
    int len;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_imu_app.stats.get_raw_count++;

    len = snprintf(text,
                   sizeof(text),
                   "ax=%d,ay=%d,az=%d,gx=%d,gy=%d,gz=%d,temp=%d,tick=%lu",
                   g_imu_app.raw.ax,
                   g_imu_app.raw.ay,
                   g_imu_app.raw.az,
                   g_imu_app.raw.gx,
                   g_imu_app.raw.gy,
                   g_imu_app.raw.gz,
                   g_imu_app.raw.temp,
                   (unsigned long)g_imu_app.raw.tick_ms);

    if (len < 0)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text))
    {
        len = (int)(sizeof(text) - 1);
        text[len] = '\0';
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleGetFiltered(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    char text[IMU_APP_RESP_BUF_SIZE];
    int len;

    long ax_mg;
    long ay_mg;
    long az_mg;
    long gx_mdps;
    long gy_mdps;
    long gz_mdps;
    long temp_mc;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_imu_app.stats.get_filtered_count++;

    ax_mg = ((long)g_imu_app.raw.ax * IMU_APP_ACC_RANGE_MG) / IMU_APP_RAW_FULL_SCALE;
    ay_mg = ((long)g_imu_app.raw.ay * IMU_APP_ACC_RANGE_MG) / IMU_APP_RAW_FULL_SCALE;
    az_mg = ((long)g_imu_app.raw.az * IMU_APP_ACC_RANGE_MG) / IMU_APP_RAW_FULL_SCALE;

    gx_mdps = ((long)g_imu_app.raw.gx * IMU_APP_GYRO_RANGE_MDPS) / IMU_APP_RAW_FULL_SCALE;
    gy_mdps = ((long)g_imu_app.raw.gy * IMU_APP_GYRO_RANGE_MDPS) / IMU_APP_RAW_FULL_SCALE;
    gz_mdps = ((long)g_imu_app.raw.gz * IMU_APP_GYRO_RANGE_MDPS) / IMU_APP_RAW_FULL_SCALE;

    temp_mc = 23000L + ((long)g_imu_app.raw.temp * 125L);

    len = snprintf(text,
                   sizeof(text),
                   "ax_mg=%ld,ay_mg=%ld,az_mg=%ld,gx_mdps=%ld,gy_mdps=%ld,gz_mdps=%ld,temp_mc=%ld,tick=%lu",
                   ax_mg,
                   ay_mg,
                   az_mg,
                   gx_mdps,
                   gy_mdps,
                   gz_mdps,
                   temp_mc,
                   (unsigned long)g_imu_app.scaled.tick_ms);

    if (len < 0)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text))
    {
        len = (int)(sizeof(text) - 1);
        text[len] = '\0';
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleGetStatus(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    char text[IMU_APP_RESP_BUF_SIZE];
    int len;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_imu_app.stats.get_status_count++;
    ImuApp_UpdateStatsState();

    len = snprintf(text,
                   sizeof(text),
                   "state=%s,started=%u,stream=%u,err=%d,sample=%lu,period=%lu",
                   ImuApp_StateToString(g_imu_app.stats.state),
                   g_imu_app.started,
                   g_imu_app.event_stream_enabled,
                   (int)g_imu_app.stats.last_error,
                   (unsigned long)g_imu_app.stats.sample_count,
                   (unsigned long)g_imu_app.sample_period_ms);

    if (len < 0)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text))
    {
        len = (int)(sizeof(text) - 1);
        text[len] = '\0';
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleGetAlgoStats(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    char text[IMU_APP_RESP_BUF_SIZE];
    int len;
    const AttitudeEstimatorState_t *att;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    att = AttitudeEstimator_GetState();

    g_imu_app.stats.get_algo_stats_count++;

    len = snprintf(text,
                   sizeof(text),
                   "run=%lu,sample=%lu,read_err=%lu,att=%lu,att_err=%lu,evt=%lu,drop=%lu,dt_us=%lu,read_us=%lu,algo_us=%lu,run_us=%lu",
                   (unsigned long)g_imu_app.stats.run_count,
                   (unsigned long)g_imu_app.stats.sample_count,
                   (unsigned long)g_imu_app.stats.read_error_count,
                   (unsigned long)att->update_count,
                   (unsigned long)g_imu_app.stats.attitude_error_count,
                   (unsigned long)g_imu_app.stats.event_stream_send_count,
                   (unsigned long)g_imu_app.stats.event_stream_drop_count,
                   (unsigned long)att->last_dt_us,
                   (unsigned long)g_imu_app.stats.last_read_us,
                   (unsigned long)g_imu_app.stats.last_algo_us,
                   (unsigned long)g_imu_app.stats.last_run_us);

    if (len < 0)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text))
    {
        len = (int)(sizeof(text) - 1);
        text[len] = '\0';
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleSetSampleRate(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    char text[64];
    uint32_t period_ms;
    int len;
    int ret;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    ret = ImuApp_ParseUint32Payload(req, &period_ms);
    if (ret != IMU_APP_OK)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INVALID_PARAM);
        return COMMAND_SERVICE_ERROR;
    }

    ret = ImuApp_SetSamplePeriodMs(period_ms);
    if (ret != IMU_APP_OK)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INVALID_PARAM);
        return COMMAND_SERVICE_ERROR;
    }

    len = snprintf(text,
                   sizeof(text),
                   "period=%lu,event_period=%lu",
                   (unsigned long)g_imu_app.sample_period_ms,
                   (unsigned long)g_imu_app.event_period_ms);

    if (len < 0)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleStartStream(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    static const char text[] = "stream=1";

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    if (ImuApp_StartStream() != IMU_APP_OK)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INVALID_STATE);
        return COMMAND_SERVICE_ERROR;
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)strlen(text));

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleStopStream(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    static const char text[] = "stream=0";

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    if (ImuApp_StopStream() != IMU_APP_OK)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INVALID_STATE);
        return COMMAND_SERVICE_ERROR;
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)strlen(text));

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleStart(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    static const char text[] = "started=1";
    int ret;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    ret = ImuApp_Start();
    if (ret != IMU_APP_OK)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INVALID_STATE);
        return COMMAND_SERVICE_ERROR;
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)strlen(text));

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleStop(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    static const char text[] = "started=0";
    int ret;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    ret = ImuApp_Stop();
    if (ret != IMU_APP_OK)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INVALID_STATE);
        return COMMAND_SERVICE_ERROR;
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)strlen(text));

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleGetAttitude(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    char text[IMU_APP_RESP_BUF_SIZE];
    int len;

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    g_imu_app.stats.get_attitude_count++;
    ImuApp_UpdateAttitudeCache();

    len = snprintf(text,
                   sizeof(text),
                   "roll_cdeg=%ld,pitch_cdeg=%ld,yaw_cdeg=%ld,q0_m=%ld,q1_m=%ld,q2_m=%ld,q3_m=%ld,tick=%lu",
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.roll_deg),
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.pitch_deg),
                   (long)ImuApp_FloatToCenti(g_imu_app.attitude.yaw_deg),
                   (long)ImuApp_FloatToMilli(g_imu_app.attitude.q0),
                   (long)ImuApp_FloatToMilli(g_imu_app.attitude.q1),
                   (long)ImuApp_FloatToMilli(g_imu_app.attitude.q2),
                   (long)ImuApp_FloatToMilli(g_imu_app.attitude.q3),
                   (unsigned long)g_imu_app.attitude.tick_ms);

    if (len < 0)
    {
        CommandService_SetNack(resp, req->cmd, PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    if (len >= (int)sizeof(text))
    {
        len = (int)(sizeof(text) - 1);
        text[len] = '\0';
    }

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)len);

    return COMMAND_SERVICE_OK;
}

static int ImuApp_HandleClearStats(const ProtocolFrame_t *req, CommandManagerResponse_t *resp, void *ctx)
{
    static const char text[] = "cleared=1";

    (void)ctx;

    if ((req == NULL) || (resp == NULL))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    ImuApp_ResetStats();

    CommandService_SetResp(resp, req->cmd, (const uint8_t *)text, (uint16_t)strlen(text));

    return COMMAND_SERVICE_OK;
}

/* ========================= Public getters / stats ========================= */

const ImuAppStats_t *ImuApp_GetStats(void)
{
    ImuApp_UpdateStatsState();
    return &g_imu_app.stats;
}

const ImuRawSample_t *ImuApp_GetRawSample(void)
{
    return &g_imu_app.raw;
}

const ImuScaledSample_t *ImuApp_GetScaledSample(void)
{
    return &g_imu_app.scaled;
}

const ImuAttitude_t *ImuApp_GetAttitude(void)
{
    ImuApp_UpdateAttitudeCache();
    return &g_imu_app.attitude;
}

void ImuApp_ResetStats(void)
{
    uint8_t initialized;
    uint8_t started;
    uint8_t stream;
    uint32_t sample_period_ms;
    uint32_t event_period_ms;
    uint8_t state;
    uint8_t previous_state;

    initialized = g_imu_app.stats.initialized;
    started = g_imu_app.started;
    stream = g_imu_app.event_stream_enabled;
    sample_period_ms = g_imu_app.sample_period_ms;
    event_period_ms = g_imu_app.event_period_ms;
    state = (uint8_t)StateMachine_GetState(&g_imu_app.sm);
    previous_state = (uint8_t)StateMachine_GetPreviousState(&g_imu_app.sm);

    memset(&g_imu_app.stats, 0, sizeof(g_imu_app.stats));

    g_imu_app.stats.initialized = initialized;
    g_imu_app.stats.started = started;
    g_imu_app.stats.event_stream_enabled = stream;
    g_imu_app.stats.sample_period_ms = sample_period_ms;
    g_imu_app.stats.event_period_ms = event_period_ms;
    g_imu_app.stats.state = state;
    g_imu_app.stats.previous_state = previous_state;

    AttitudeEstimator_Reset();
}

void ImuApp_PrintStats(void)
{
    const AttitudeEstimatorState_t *att;

    att = AttitudeEstimator_GetState();

    ImuApp_UpdateStatsState();

    BoardLog_PrintSeparator();
    BoardLog_Info("ImuApp Stats:\r\n");
    BoardLog_Info("  initialized          = %u\r\n", g_imu_app.stats.initialized);
    BoardLog_Info("  state                = %s\r\n", ImuApp_StateToString(g_imu_app.stats.state));
    BoardLog_Info("  started              = %u\r\n", g_imu_app.stats.started);
    BoardLog_Info("  stream               = %u\r\n", g_imu_app.stats.event_stream_enabled);
    BoardLog_Info("  init_count           = %lu\r\n", g_imu_app.stats.init_count);
    BoardLog_Info("  run_count            = %lu\r\n", g_imu_app.stats.run_count);
    BoardLog_Info("  sample_count         = %lu\r\n", g_imu_app.stats.sample_count);
    BoardLog_Info("  read_error_count     = %lu\r\n", g_imu_app.stats.read_error_count);
    BoardLog_Info("  attitude_update      = %lu\r\n", g_imu_app.stats.attitude_update_count);
    BoardLog_Info("  attitude_error       = %lu\r\n", g_imu_app.stats.attitude_error_count);
    BoardLog_Info("  event_send           = %lu\r\n", g_imu_app.stats.event_stream_send_count);
    BoardLog_Info("  event_drop           = %lu\r\n", g_imu_app.stats.event_stream_drop_count);
    BoardLog_Info("  sample_period_ms     = %lu\r\n", g_imu_app.stats.sample_period_ms);
    BoardLog_Info("  event_period_ms      = %lu\r\n", g_imu_app.stats.event_period_ms);
    BoardLog_Info("  last_read_us         = %lu\r\n", g_imu_app.stats.last_read_us);
    BoardLog_Info("  max_read_us          = %lu\r\n", g_imu_app.stats.max_read_us);
    BoardLog_Info("  last_algo_us         = %lu\r\n", g_imu_app.stats.last_algo_us);
    BoardLog_Info("  max_algo_us          = %lu\r\n", g_imu_app.stats.max_algo_us);
    BoardLog_Info("  last_run_us          = %lu\r\n", g_imu_app.stats.last_run_us);
    BoardLog_Info("  max_run_us           = %lu\r\n", g_imu_app.stats.max_run_us);
    BoardLog_Info("  att_update_count     = %lu\r\n", att->update_count);
    BoardLog_Info("  att_last_dt_us       = %lu\r\n", att->last_dt_us);
    BoardLog_Info("  att_max_dt_us        = %lu\r\n", att->max_dt_us);
    BoardLog_Info("  last_error           = %d\r\n", (int)g_imu_app.stats.last_error);
}
