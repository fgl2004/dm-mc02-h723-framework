#include "bsp_bmi088.h"

#include "platform_spi.h"
#include "platform_time.h"
#include "board_log.h"

#include <string.h>

/* BMI088 chip IDs. */
#define BMI088_ACC_CHIP_ID_VALUE             0x1EU
#define BMI088_GYRO_CHIP_ID_VALUE            0x0FU

#define BMI088_SPI_READ_BIT                  0x80U
#define BMI088_SPI_WRITE_MASK                0x7FU

/* Accelerometer registers. */
#define BMI088_ACC_CHIP_ID_REG               0x00U
#define BMI088_ACC_ERR_REG                   0x02U
#define BMI088_ACC_STATUS_REG                0x03U
#define BMI088_ACC_X_LSB_REG                 0x12U
#define BMI088_ACC_X_MSB_REG                 0x13U
#define BMI088_ACC_Y_LSB_REG                 0x14U
#define BMI088_ACC_Y_MSB_REG                 0x15U
#define BMI088_ACC_Z_LSB_REG                 0x16U
#define BMI088_ACC_Z_MSB_REG                 0x17U
#define BMI088_ACC_TEMP_MSB_REG              0x22U
#define BMI088_ACC_TEMP_LSB_REG              0x23U
#define BMI088_ACC_CONF_REG                  0x40U
#define BMI088_ACC_RANGE_REG                 0x41U
#define BMI088_ACC_PWR_CONF_REG              0x7CU
#define BMI088_ACC_PWR_CTRL_REG              0x7DU
#define BMI088_ACC_SOFTRESET_REG             0x7EU

/* Gyroscope registers. */
#define BMI088_GYRO_CHIP_ID_REG              0x00U
#define BMI088_GYRO_X_LSB_REG                0x02U
#define BMI088_GYRO_X_MSB_REG                0x03U
#define BMI088_GYRO_Y_LSB_REG                0x04U
#define BMI088_GYRO_Y_MSB_REG                0x05U
#define BMI088_GYRO_Z_LSB_REG                0x06U
#define BMI088_GYRO_Z_MSB_REG                0x07U
#define BMI088_GYRO_RANGE_REG                0x0FU
#define BMI088_GYRO_BANDWIDTH_REG            0x10U
#define BMI088_GYRO_LPM1_REG                 0x11U
#define BMI088_GYRO_SOFTRESET_REG            0x14U

/* Basic configuration values. */
#define BMI088_ACC_SOFTRESET_CMD             0xB6U
#define BMI088_GYRO_SOFTRESET_CMD            0xB6U

#define BMI088_ACC_PWR_CONF_ACTIVE           0x00U
#define BMI088_ACC_PWR_CTRL_ON               0x04U
#define BMI088_ACC_CONF_OSR_NORMAL_ODR_100HZ 0xA8U
#define BMI088_ACC_RANGE_6G                  0x01U

#define BMI088_GYRO_RANGE_2000_DPS           0x00U
#define BMI088_GYRO_BW_100HZ_32HZ            0x07U
#define BMI088_GYRO_LPM1_NORMAL              0x00U

#define BMI088_SPI_TIMEOUT_MS                10U

/*
 * BMI088 accelerometer SPI read has one dummy byte after register address.
 * BMI088 gyroscope SPI read has no additional dummy byte.
 */
#define BMI088_ACC_READ_DUMMY_BYTES          1U
#define BMI088_GYRO_READ_DUMMY_BYTES         0U

typedef enum
{
    BMI088_PART_ACC = 0,
    BMI088_PART_GYRO = 1
} BspBmi088Part_t;

static BspBmi088Stats_t g_bmi088_stats;

static PlatformSpiDevice_t BspBmi088_GetSpiDevice(BspBmi088Part_t part)
{
    if (part == BMI088_PART_ACC)
    {
        return PLATFORM_SPI_DEVICE_0;
    }

    return PLATFORM_SPI_DEVICE_1;
}

static int16_t BspBmi088_MakeInt16(uint8_t lsb, uint8_t msb)
{
    return (int16_t)((uint16_t)lsb | ((uint16_t)msb << 8U));
}

static void BspBmi088_RecordError(int error)
{
    g_bmi088_stats.last_error = error;
}

static int BspBmi088_ReadRegs(BspBmi088Part_t part,
                              uint8_t reg,
                              uint8_t *data,
                              uint16_t len)
{
    uint8_t tx[18];
    uint8_t rx[18];
    uint16_t dummy_len;
    uint16_t transfer_len;
    uint16_t i;
    int ret;

    if ((data == NULL) || (len == 0U) || (len > 16U))
    {
        g_bmi088_stats.invalid_param_count++;
        BspBmi088_RecordError(BSP_BMI088_INVALID_PARAM);
        return BSP_BMI088_INVALID_PARAM;
    }

    dummy_len = (part == BMI088_PART_ACC) ? BMI088_ACC_READ_DUMMY_BYTES : BMI088_GYRO_READ_DUMMY_BYTES;
    transfer_len = (uint16_t)(1U + dummy_len + len);

    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));

    tx[0] = (uint8_t)(reg | BMI088_SPI_READ_BIT);

    ret = PlatformSpi_Transfer(BspBmi088_GetSpiDevice(part),
                               tx,
                               rx,
                               transfer_len,
                               BMI088_SPI_TIMEOUT_MS);

    if (ret != PLATFORM_SPI_OK)
    {
        g_bmi088_stats.spi_error_count++;
        BspBmi088_RecordError(BSP_BMI088_SPI_ERROR);
        return BSP_BMI088_SPI_ERROR;
    }

    for (i = 0U; i < len; i++)
    {
        data[i] = rx[1U + dummy_len + i];
    }

    g_bmi088_stats.last_error = BSP_BMI088_OK;

    return BSP_BMI088_OK;
}

static int BspBmi088_ReadReg(BspBmi088Part_t part,
                             uint8_t reg,
                             uint8_t *value)
{
    return BspBmi088_ReadRegs(part, reg, value, 1U);
}

static int BspBmi088_WriteReg(BspBmi088Part_t part,
                              uint8_t reg,
                              uint8_t value)
{
    uint8_t tx[2];
    uint8_t rx[2];
    int ret;

    tx[0] = (uint8_t)(reg & BMI088_SPI_WRITE_MASK);
    tx[1] = value;

    memset(rx, 0, sizeof(rx));

    ret = PlatformSpi_Transfer(BspBmi088_GetSpiDevice(part),
                               tx,
                               rx,
                               (uint16_t)sizeof(tx),
                               BMI088_SPI_TIMEOUT_MS);

    if (ret != PLATFORM_SPI_OK)
    {
        g_bmi088_stats.spi_error_count++;
        BspBmi088_RecordError(BSP_BMI088_SPI_ERROR);
        return BSP_BMI088_SPI_ERROR;
    }

    g_bmi088_stats.last_error = BSP_BMI088_OK;

    return BSP_BMI088_OK;
}

int BspBmi088_Init(void)
{
    BspBmi088ChipId_t chip_id;

    memset(&g_bmi088_stats, 0, sizeof(g_bmi088_stats));

    g_bmi088_stats.init_count++;

    PlatformSpi_DeselectAll();

    (void)BspBmi088_WriteReg(BMI088_PART_ACC,
                             BMI088_ACC_SOFTRESET_REG,
                             BMI088_ACC_SOFTRESET_CMD);
    PlatformTime_DelayMs(50U);

    (void)BspBmi088_WriteReg(BMI088_PART_GYRO,
                             BMI088_GYRO_SOFTRESET_REG,
                             BMI088_GYRO_SOFTRESET_CMD);
    PlatformTime_DelayMs(50U);

    if (BspBmi088_WriteReg(BMI088_PART_ACC,
                           BMI088_ACC_PWR_CONF_REG,
                           BMI088_ACC_PWR_CONF_ACTIVE) != BSP_BMI088_OK)
    {
        return BSP_BMI088_SPI_ERROR;
    }

    PlatformTime_DelayMs(5U);

    if (BspBmi088_WriteReg(BMI088_PART_ACC,
                           BMI088_ACC_PWR_CTRL_REG,
                           BMI088_ACC_PWR_CTRL_ON) != BSP_BMI088_OK)
    {
        return BSP_BMI088_SPI_ERROR;
    }

    PlatformTime_DelayMs(50U);

    (void)BspBmi088_WriteReg(BMI088_PART_ACC,
                             BMI088_ACC_CONF_REG,
                             BMI088_ACC_CONF_OSR_NORMAL_ODR_100HZ);
    (void)BspBmi088_WriteReg(BMI088_PART_ACC,
                             BMI088_ACC_RANGE_REG,
                             BMI088_ACC_RANGE_6G);

    (void)BspBmi088_WriteReg(BMI088_PART_GYRO,
                             BMI088_GYRO_LPM1_REG,
                             BMI088_GYRO_LPM1_NORMAL);
    PlatformTime_DelayMs(30U);

    (void)BspBmi088_WriteReg(BMI088_PART_GYRO,
                             BMI088_GYRO_RANGE_REG,
                             BMI088_GYRO_RANGE_2000_DPS);
    (void)BspBmi088_WriteReg(BMI088_PART_GYRO,
                             BMI088_GYRO_BANDWIDTH_REG,
                             BMI088_GYRO_BW_100HZ_32HZ);

    if (BspBmi088_ReadChipId(&chip_id) != BSP_BMI088_OK)
    {
        g_bmi088_stats.chip_id_error_count++;
        BspBmi088_RecordError(BSP_BMI088_CHIP_ID_ERROR);
        return BSP_BMI088_CHIP_ID_ERROR;
    }

    if ((chip_id.acc_id != BMI088_ACC_CHIP_ID_VALUE) ||
        (chip_id.gyro_id != BMI088_GYRO_CHIP_ID_VALUE))
    {
        g_bmi088_stats.chip_id_error_count++;
        BspBmi088_RecordError(BSP_BMI088_CHIP_ID_ERROR);
        return BSP_BMI088_CHIP_ID_ERROR;
    }

    g_bmi088_stats.initialized = 1U;
    g_bmi088_stats.last_acc_id = chip_id.acc_id;
    g_bmi088_stats.last_gyro_id = chip_id.gyro_id;
    g_bmi088_stats.last_error = BSP_BMI088_OK;

    BoardLog_Info("BspBmi088 init OK, acc_id=0x%02X, gyro_id=0x%02X\r\n",
                  chip_id.acc_id,
                  chip_id.gyro_id);

    return BSP_BMI088_OK;
}

int BspBmi088_ReadChipId(BspBmi088ChipId_t *chip_id)
{
    uint8_t acc_id;
    uint8_t gyro_id;

    if (chip_id == NULL)
    {
        g_bmi088_stats.invalid_param_count++;
        BspBmi088_RecordError(BSP_BMI088_INVALID_PARAM);
        return BSP_BMI088_INVALID_PARAM;
    }

    if (BspBmi088_ReadReg(BMI088_PART_ACC,
                          BMI088_ACC_CHIP_ID_REG,
                          &acc_id) != BSP_BMI088_OK)
    {
        return BSP_BMI088_SPI_ERROR;
    }

    if (BspBmi088_ReadReg(BMI088_PART_GYRO,
                          BMI088_GYRO_CHIP_ID_REG,
                          &gyro_id) != BSP_BMI088_OK)
    {
        return BSP_BMI088_SPI_ERROR;
    }

    chip_id->acc_id = acc_id;
    chip_id->gyro_id = gyro_id;

    g_bmi088_stats.read_chip_id_count++;
    g_bmi088_stats.last_acc_id = acc_id;
    g_bmi088_stats.last_gyro_id = gyro_id;
    g_bmi088_stats.last_error = BSP_BMI088_OK;

    return BSP_BMI088_OK;
}

int BspBmi088_ReadAccelRaw(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];

    if ((ax == NULL) || (ay == NULL) || (az == NULL))
    {
        g_bmi088_stats.invalid_param_count++;
        BspBmi088_RecordError(BSP_BMI088_INVALID_PARAM);
        return BSP_BMI088_INVALID_PARAM;
    }

    if (g_bmi088_stats.initialized == 0U)
    {
        g_bmi088_stats.not_initialized_count++;
        BspBmi088_RecordError(BSP_BMI088_NOT_INITIALIZED);
        return BSP_BMI088_NOT_INITIALIZED;
    }

    if (BspBmi088_ReadRegs(BMI088_PART_ACC,
                           BMI088_ACC_X_LSB_REG,
                           buf,
                           (uint16_t)sizeof(buf)) != BSP_BMI088_OK)
    {
        return BSP_BMI088_SPI_ERROR;
    }

    *ax = BspBmi088_MakeInt16(buf[0], buf[1]);
    *ay = BspBmi088_MakeInt16(buf[2], buf[3]);
    *az = BspBmi088_MakeInt16(buf[4], buf[5]);

    g_bmi088_stats.read_accel_count++;
    g_bmi088_stats.last_error = BSP_BMI088_OK;

    return BSP_BMI088_OK;
}

int BspBmi088_ReadGyroRaw(int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[6];

    if ((gx == NULL) || (gy == NULL) || (gz == NULL))
    {
        g_bmi088_stats.invalid_param_count++;
        BspBmi088_RecordError(BSP_BMI088_INVALID_PARAM);
        return BSP_BMI088_INVALID_PARAM;
    }

    if (g_bmi088_stats.initialized == 0U)
    {
        g_bmi088_stats.not_initialized_count++;
        BspBmi088_RecordError(BSP_BMI088_NOT_INITIALIZED);
        return BSP_BMI088_NOT_INITIALIZED;
    }

    if (BspBmi088_ReadRegs(BMI088_PART_GYRO,
                           BMI088_GYRO_X_LSB_REG,
                           buf,
                           (uint16_t)sizeof(buf)) != BSP_BMI088_OK)
    {
        return BSP_BMI088_SPI_ERROR;
    }

    *gx = BspBmi088_MakeInt16(buf[0], buf[1]);
    *gy = BspBmi088_MakeInt16(buf[2], buf[3]);
    *gz = BspBmi088_MakeInt16(buf[4], buf[5]);

    g_bmi088_stats.read_gyro_count++;
    g_bmi088_stats.last_error = BSP_BMI088_OK;

    return BSP_BMI088_OK;
}

int BspBmi088_ReadTemperatureRaw(int16_t *temp)
{
    uint8_t buf[2];
    int16_t raw_temp;

    if (temp == NULL)
    {
        g_bmi088_stats.invalid_param_count++;
        BspBmi088_RecordError(BSP_BMI088_INVALID_PARAM);
        return BSP_BMI088_INVALID_PARAM;
    }

    if (g_bmi088_stats.initialized == 0U)
    {
        g_bmi088_stats.not_initialized_count++;
        BspBmi088_RecordError(BSP_BMI088_NOT_INITIALIZED);
        return BSP_BMI088_NOT_INITIALIZED;
    }

    if (BspBmi088_ReadRegs(BMI088_PART_ACC,
                           BMI088_ACC_TEMP_MSB_REG,
                           buf,
                           (uint16_t)sizeof(buf)) != BSP_BMI088_OK)
    {
        return BSP_BMI088_SPI_ERROR;
    }

    raw_temp = (int16_t)(((int16_t)buf[0] << 3) | ((int16_t)buf[1] >> 5));

    if ((raw_temp & 0x0400) != 0)
    {
        raw_temp |= (int16_t)0xF800;
    }

    *temp = raw_temp;

    g_bmi088_stats.read_temp_count++;
    g_bmi088_stats.last_error = BSP_BMI088_OK;

    return BSP_BMI088_OK;
}

int BspBmi088_ReadRaw(BspBmi088RawData_t *raw)
{
    int ret;

    if (raw == NULL)
    {
        g_bmi088_stats.invalid_param_count++;
        BspBmi088_RecordError(BSP_BMI088_INVALID_PARAM);
        return BSP_BMI088_INVALID_PARAM;
    }

    memset(raw, 0, sizeof(*raw));

    ret = BspBmi088_ReadAccelRaw(&raw->ax, &raw->ay, &raw->az);
    if (ret != BSP_BMI088_OK)
    {
        return ret;
    }

    ret = BspBmi088_ReadGyroRaw(&raw->gx, &raw->gy, &raw->gz);
    if (ret != BSP_BMI088_OK)
    {
        return ret;
    }

    ret = BspBmi088_ReadTemperatureRaw(&raw->temp);
    if (ret != BSP_BMI088_OK)
    {
        raw->temp = 0;
    }

    raw->tick_ms = PlatformTime_GetMs();

    g_bmi088_stats.read_raw_count++;
    g_bmi088_stats.last_error = BSP_BMI088_OK;

    return BSP_BMI088_OK;
}

const BspBmi088Stats_t *BspBmi088_GetStats(void)
{
    return &g_bmi088_stats;
}

void BspBmi088_ResetStats(void)
{
    uint8_t initialized = g_bmi088_stats.initialized;
    uint8_t last_acc_id = g_bmi088_stats.last_acc_id;
    uint8_t last_gyro_id = g_bmi088_stats.last_gyro_id;

    memset(&g_bmi088_stats, 0, sizeof(g_bmi088_stats));

    g_bmi088_stats.initialized = initialized;
    g_bmi088_stats.last_acc_id = last_acc_id;
    g_bmi088_stats.last_gyro_id = last_gyro_id;
}

void BspBmi088_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("BspBmi088 Stats:\r\n");
    BoardLog_Info("  initialized         = %u\r\n", g_bmi088_stats.initialized);
    BoardLog_Info("  init_count          = %lu\r\n", g_bmi088_stats.init_count);
    BoardLog_Info("  read_chip_id_count  = %lu\r\n", g_bmi088_stats.read_chip_id_count);
    BoardLog_Info("  read_raw_count      = %lu\r\n", g_bmi088_stats.read_raw_count);
    BoardLog_Info("  read_accel_count    = %lu\r\n", g_bmi088_stats.read_accel_count);
    BoardLog_Info("  read_gyro_count     = %lu\r\n", g_bmi088_stats.read_gyro_count);
    BoardLog_Info("  read_temp_count     = %lu\r\n", g_bmi088_stats.read_temp_count);
    BoardLog_Info("  spi_error_count     = %lu\r\n", g_bmi088_stats.spi_error_count);
    BoardLog_Info("  chip_id_error_count = %lu\r\n", g_bmi088_stats.chip_id_error_count);
    BoardLog_Info("  invalid_param_count = %lu\r\n", g_bmi088_stats.invalid_param_count);
    BoardLog_Info("  not_initialized     = %lu\r\n", g_bmi088_stats.not_initialized_count);
    BoardLog_Info("  last_acc_id         = 0x%02X\r\n", g_bmi088_stats.last_acc_id);
    BoardLog_Info("  last_gyro_id        = 0x%02X\r\n", g_bmi088_stats.last_gyro_id);
    BoardLog_Info("  last_error          = %d\r\n", g_bmi088_stats.last_error);
}
