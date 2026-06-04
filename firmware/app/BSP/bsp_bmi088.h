#ifndef BSP_BMI088_H
#define BSP_BMI088_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    BSP_BMI088_OK = 0,
    BSP_BMI088_ERROR = -1,
    BSP_BMI088_INVALID_PARAM = -2,
    BSP_BMI088_SPI_ERROR = -3,
    BSP_BMI088_CHIP_ID_ERROR = -4,
    BSP_BMI088_NOT_INITIALIZED = -5
} BspBmi088Result_t;

typedef struct
{
    uint8_t acc_id;
    uint8_t gyro_id;
} BspBmi088ChipId_t;

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
} BspBmi088RawData_t;

typedef struct
{
    uint32_t init_count;
    uint32_t read_chip_id_count;
    uint32_t read_raw_count;
    uint32_t read_accel_count;
    uint32_t read_gyro_count;
    uint32_t read_temp_count;

    uint32_t spi_error_count;
    uint32_t chip_id_error_count;
    uint32_t invalid_param_count;
    uint32_t not_initialized_count;

    uint8_t initialized;
    uint8_t last_acc_id;
    uint8_t last_gyro_id;
    int last_error;
} BspBmi088Stats_t;

int BspBmi088_Init(void);

int BspBmi088_ReadChipId(BspBmi088ChipId_t *chip_id);
int BspBmi088_ReadRaw(BspBmi088RawData_t *raw);
int BspBmi088_ReadAccelRaw(int16_t *ax, int16_t *ay, int16_t *az);
int BspBmi088_ReadGyroRaw(int16_t *gx, int16_t *gy, int16_t *gz);
int BspBmi088_ReadTemperatureRaw(int16_t *temp);

const BspBmi088Stats_t *BspBmi088_GetStats(void);
void BspBmi088_ResetStats(void);
void BspBmi088_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BMI088_H */
