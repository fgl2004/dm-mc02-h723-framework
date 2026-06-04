#include "attitude_estimator.h"

#include "platform_time.h"

#include <math.h>
#include <string.h>

#ifndef ATTITUDE_ESTIMATOR_DEFAULT_ALPHA
#define ATTITUDE_ESTIMATOR_DEFAULT_ALPHA    0.98f
#endif

#define ATTITUDE_ESTIMATOR_RAD_TO_DEG       57.29577951308232f
#define ATTITUDE_ESTIMATOR_DEG_TO_RAD       0.017453292519943295f
#define ATTITUDE_ESTIMATOR_MIN_DT_S         0.000001f
#define ATTITUDE_ESTIMATOR_MAX_DT_S         1.000000f
#define ATTITUDE_ESTIMATOR_MIN_ACC_NORM     0.05f

static AttitudeEstimatorState_t g_attitude_estimator;

static float AttitudeEstimator_WrapAngle180(float angle_deg)
{
    while (angle_deg > 180.0f)
    {
        angle_deg -= 360.0f;
    }

    while (angle_deg < -180.0f)
    {
        angle_deg += 360.0f;
    }

    return angle_deg;
}

static void AttitudeEstimator_UpdateQuaternionFromEuler(void)
{
    float roll_rad;
    float pitch_rad;
    float yaw_rad;

    float cr;
    float sr;
    float cp;
    float sp;
    float cy;
    float sy;

    roll_rad = g_attitude_estimator.roll_deg * ATTITUDE_ESTIMATOR_DEG_TO_RAD;
    pitch_rad = g_attitude_estimator.pitch_deg * ATTITUDE_ESTIMATOR_DEG_TO_RAD;
    yaw_rad = g_attitude_estimator.yaw_deg * ATTITUDE_ESTIMATOR_DEG_TO_RAD;

    cr = cosf(roll_rad * 0.5f);
    sr = sinf(roll_rad * 0.5f);
    cp = cosf(pitch_rad * 0.5f);
    sp = sinf(pitch_rad * 0.5f);
    cy = cosf(yaw_rad * 0.5f);
    sy = sinf(yaw_rad * 0.5f);

    g_attitude_estimator.q0 = (cr * cp * cy) + (sr * sp * sy);
    g_attitude_estimator.q1 = (sr * cp * cy) - (cr * sp * sy);
    g_attitude_estimator.q2 = (cr * sp * cy) + (sr * cp * sy);
    g_attitude_estimator.q3 = (cr * cp * sy) - (sr * sp * cy);
}

void AttitudeEstimator_Init(void)
{
    memset(&g_attitude_estimator, 0, sizeof(g_attitude_estimator));

    g_attitude_estimator.alpha = ATTITUDE_ESTIMATOR_DEFAULT_ALPHA;
    g_attitude_estimator.q0 = 1.0f;
    g_attitude_estimator.initialized = 1U;
    g_attitude_estimator.last_error = ATTITUDE_ESTIMATOR_OK;
}

void AttitudeEstimator_Reset(void)
{
    float alpha;

    alpha = g_attitude_estimator.alpha;

    memset(&g_attitude_estimator, 0, sizeof(g_attitude_estimator));

    g_attitude_estimator.alpha = alpha;
    if (g_attitude_estimator.alpha <= 0.0f)
    {
        g_attitude_estimator.alpha = ATTITUDE_ESTIMATOR_DEFAULT_ALPHA;
    }

    g_attitude_estimator.q0 = 1.0f;
    g_attitude_estimator.initialized = 1U;
    g_attitude_estimator.last_error = ATTITUDE_ESTIMATOR_OK;
}

int AttitudeEstimator_Update6Axis(float ax_g,
                                  float ay_g,
                                  float az_g,
                                  float gx_dps,
                                  float gy_dps,
                                  float gz_dps,
                                  float dt_s,
                                  uint32_t tick_ms)
{
    uint32_t start_cycle;
    uint32_t elapsed_us;

    float acc_norm;
    float roll_acc;
    float pitch_acc;

    float gyro_roll;
    float gyro_pitch;
    float gyro_yaw;

    float alpha;

    if (g_attitude_estimator.initialized == 0U)
    {
        g_attitude_estimator.last_error = ATTITUDE_ESTIMATOR_NOT_INITIALIZED;
        return ATTITUDE_ESTIMATOR_NOT_INITIALIZED;
    }

    if ((dt_s < ATTITUDE_ESTIMATOR_MIN_DT_S) || (dt_s > ATTITUDE_ESTIMATOR_MAX_DT_S))
    {
        g_attitude_estimator.invalid_sample_count++;
        g_attitude_estimator.last_error = ATTITUDE_ESTIMATOR_INVALID_PARAM;
        return ATTITUDE_ESTIMATOR_INVALID_PARAM;
    }

    acc_norm = sqrtf((ax_g * ax_g) + (ay_g * ay_g) + (az_g * az_g));
    if (acc_norm < ATTITUDE_ESTIMATOR_MIN_ACC_NORM)
    {
        g_attitude_estimator.invalid_sample_count++;
        g_attitude_estimator.last_error = ATTITUDE_ESTIMATOR_INVALID_PARAM;
        return ATTITUDE_ESTIMATOR_INVALID_PARAM;
    }

    start_cycle = PlatformTime_ProfileStart();

    roll_acc = atan2f(ay_g, az_g) * ATTITUDE_ESTIMATOR_RAD_TO_DEG;
    pitch_acc = atan2f(-ax_g, sqrtf((ay_g * ay_g) + (az_g * az_g))) * ATTITUDE_ESTIMATOR_RAD_TO_DEG;

    if (g_attitude_estimator.first_update_done == 0U)
    {
        g_attitude_estimator.roll_deg = roll_acc;
        g_attitude_estimator.pitch_deg = pitch_acc;
        g_attitude_estimator.yaw_deg = 0.0f;
        g_attitude_estimator.first_update_done = 1U;
    }
    else
    {
        gyro_roll = g_attitude_estimator.roll_deg + (gx_dps * dt_s);
        gyro_pitch = g_attitude_estimator.pitch_deg + (gy_dps * dt_s);
        gyro_yaw = g_attitude_estimator.yaw_deg + (gz_dps * dt_s);

        alpha = g_attitude_estimator.alpha;

        g_attitude_estimator.roll_deg = (alpha * gyro_roll) + ((1.0f - alpha) * roll_acc);
        g_attitude_estimator.pitch_deg = (alpha * gyro_pitch) + ((1.0f - alpha) * pitch_acc);
        g_attitude_estimator.yaw_deg = AttitudeEstimator_WrapAngle180(gyro_yaw);
    }

    AttitudeEstimator_UpdateQuaternionFromEuler();

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);

    g_attitude_estimator.update_count++;
    g_attitude_estimator.last_update_tick_ms = tick_ms;
    g_attitude_estimator.last_dt_us = (uint32_t)(dt_s * 1000000.0f);
    if (g_attitude_estimator.last_dt_us > g_attitude_estimator.max_dt_us)
    {
        g_attitude_estimator.max_dt_us = g_attitude_estimator.last_dt_us;
    }

    g_attitude_estimator.last_update_us = elapsed_us;
    if (elapsed_us > g_attitude_estimator.max_update_us)
    {
        g_attitude_estimator.max_update_us = elapsed_us;
    }

    g_attitude_estimator.last_error = ATTITUDE_ESTIMATOR_OK;

    return ATTITUDE_ESTIMATOR_OK;
}

const AttitudeEstimatorState_t *AttitudeEstimator_GetState(void)
{
    return &g_attitude_estimator;
}
