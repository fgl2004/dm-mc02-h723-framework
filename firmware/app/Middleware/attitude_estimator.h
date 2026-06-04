#ifndef ATTITUDE_ESTIMATOR_H
#define ATTITUDE_ESTIMATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Stage 4 V1 attitude estimator.
 *
 * Algorithm:
 *   - 6-axis complementary filter
 *   - roll / pitch corrected by accelerometer gravity vector
 *   - yaw integrated from gyroscope only
 *
 * Important:
 *   BMI088 is a 6-axis IMU in this project. Without magnetometer, yaw is not
 *   absolute and will drift over time.
 */

typedef enum
{
    ATTITUDE_ESTIMATOR_OK = 0,
    ATTITUDE_ESTIMATOR_ERROR = -1,
    ATTITUDE_ESTIMATOR_INVALID_PARAM = -2,
    ATTITUDE_ESTIMATOR_NOT_INITIALIZED = -3
} AttitudeEstimatorResult_t;

typedef struct
{
    float roll_deg;
    float pitch_deg;
    float yaw_deg;

    float q0;
    float q1;
    float q2;
    float q3;

    float alpha;

    uint32_t update_count;
    uint32_t invalid_sample_count;

    uint32_t last_update_tick_ms;
    uint32_t last_dt_us;
    uint32_t max_dt_us;

    uint32_t last_update_us;
    uint32_t max_update_us;

    uint8_t initialized;
    uint8_t first_update_done;
    int last_error;
} AttitudeEstimatorState_t;

void AttitudeEstimator_Init(void);
void AttitudeEstimator_Reset(void);

int AttitudeEstimator_Update6Axis(float ax_g,
                                  float ay_g,
                                  float az_g,
                                  float gx_dps,
                                  float gy_dps,
                                  float gz_dps,
                                  float dt_s,
                                  uint32_t tick_ms);

const AttitudeEstimatorState_t *AttitudeEstimator_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* ATTITUDE_ESTIMATOR_H */
