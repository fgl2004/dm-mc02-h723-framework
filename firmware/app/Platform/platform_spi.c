#include "platform_spi.h"

#include "main.h"
#include "spi.h"
#include "board_log.h"

#include <string.h>

/*
 * Current board mapping:
 *   PLATFORM_SPI_DEVICE_0 -> SPI2 + ACC_CS
 *   PLATFORM_SPI_DEVICE_1 -> SPI2 + GYRO_CS
 *
 * API remains generic. Sensor register logic stays in BSP.
 */

typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
} PlatformSpiDeviceConfig_t;

static const PlatformSpiDeviceConfig_t g_spi_device_config[PLATFORM_SPI_DEVICE_COUNT] =
{
    { &hspi2, ACC_CS_GPIO_Port,  ACC_CS_Pin  },
    { &hspi2, GYRO_CS_GPIO_Port, GYRO_CS_Pin }
};

static PlatformSpiStats_t g_platform_spi_stats;

static uint8_t PlatformSpi_IsValidDevice(PlatformSpiDevice_t dev)
{
    return ((uint32_t)dev < (uint32_t)PLATFORM_SPI_DEVICE_COUNT) ? 1U : 0U;
}

static int PlatformSpi_ConvertHalStatus(HAL_StatusTypeDef status)
{
    switch (status)
    {
        case HAL_OK:
            return PLATFORM_SPI_OK;
        case HAL_BUSY:
            return PLATFORM_SPI_BUSY;
        case HAL_TIMEOUT:
            return PLATFORM_SPI_TIMEOUT;
        case HAL_ERROR:
        default:
            return PLATFORM_SPI_HAL_ERROR;
    }
}

void PlatformSpi_Init(void)
{
    memset(&g_platform_spi_stats, 0, sizeof(g_platform_spi_stats));

    /*
     * CubeMX usually calls MX_SPI2_Init() before App_Init().
     * Do not call HAL_SPI_Init() again here unless you intentionally re-init.
     */
    PlatformSpi_DeselectAll();

    g_platform_spi_stats.init_count++;

    BoardLog_Info("PlatformSpi init OK\r\n");
}

void PlatformSpi_Select(PlatformSpiDevice_t dev)
{
    if (PlatformSpi_IsValidDevice(dev) == 0U)
    {
        g_platform_spi_stats.invalid_param_count++;
        g_platform_spi_stats.last_error = PLATFORM_SPI_INVALID_PARAM;
        return;
    }

    PlatformSpi_DeselectAll();

    HAL_GPIO_WritePin(g_spi_device_config[dev].cs_port,
                      g_spi_device_config[dev].cs_pin,
                      GPIO_PIN_RESET);
}

void PlatformSpi_Deselect(PlatformSpiDevice_t dev)
{
    if (PlatformSpi_IsValidDevice(dev) == 0U)
    {
        g_platform_spi_stats.invalid_param_count++;
        g_platform_spi_stats.last_error = PLATFORM_SPI_INVALID_PARAM;
        return;
    }

    HAL_GPIO_WritePin(g_spi_device_config[dev].cs_port,
                      g_spi_device_config[dev].cs_pin,
                      GPIO_PIN_SET);
}

void PlatformSpi_DeselectAll(void)
{
    uint32_t i;

    for (i = 0U; i < (uint32_t)PLATFORM_SPI_DEVICE_COUNT; i++)
    {
        HAL_GPIO_WritePin(g_spi_device_config[i].cs_port,
                          g_spi_device_config[i].cs_pin,
                          GPIO_PIN_SET);
    }
}

int PlatformSpi_Transfer(PlatformSpiDevice_t dev,
                         const uint8_t *tx_buf,
                         uint8_t *rx_buf,
                         uint16_t len,
                         uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_status;
    int ret;

    if ((PlatformSpi_IsValidDevice(dev) == 0U) ||
        (tx_buf == NULL) ||
        (rx_buf == NULL) ||
        (len == 0U))
    {
        g_platform_spi_stats.invalid_param_count++;
        g_platform_spi_stats.last_device = (uint8_t)dev;
        g_platform_spi_stats.last_error = PLATFORM_SPI_INVALID_PARAM;
        return PLATFORM_SPI_INVALID_PARAM;
    }

    g_platform_spi_stats.transfer_count++;
    g_platform_spi_stats.last_device = (uint8_t)dev;
    g_platform_spi_stats.last_len = len;
    g_platform_spi_stats.last_timeout_ms = timeout_ms;

    PlatformSpi_Select(dev);

    hal_status = HAL_SPI_TransmitReceive(g_spi_device_config[dev].hspi,
                                         (uint8_t *)tx_buf,
                                         rx_buf,
                                         len,
                                         timeout_ms);

    PlatformSpi_Deselect(dev);

    ret = PlatformSpi_ConvertHalStatus(hal_status);

    if (ret == PLATFORM_SPI_OK)
    {
        g_platform_spi_stats.bytes_total += len;
        g_platform_spi_stats.last_error = PLATFORM_SPI_OK;
        return PLATFORM_SPI_OK;
    }

    g_platform_spi_stats.error_count++;
    g_platform_spi_stats.last_error = ret;

    if (ret == PLATFORM_SPI_TIMEOUT)
    {
        g_platform_spi_stats.timeout_count++;
    }
    else if (ret == PLATFORM_SPI_BUSY)
    {
        g_platform_spi_stats.busy_count++;
    }

    return ret;
}

const PlatformSpiStats_t *PlatformSpi_GetStats(void)
{
    return &g_platform_spi_stats;
}

void PlatformSpi_ResetStats(void)
{
    memset(&g_platform_spi_stats, 0, sizeof(g_platform_spi_stats));
}

void PlatformSpi_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("PlatformSpi Stats:\r\n");
    BoardLog_Info("  init_count          = %lu\r\n", g_platform_spi_stats.init_count);
    BoardLog_Info("  transfer_count      = %lu\r\n", g_platform_spi_stats.transfer_count);
    BoardLog_Info("  error_count         = %lu\r\n", g_platform_spi_stats.error_count);
    BoardLog_Info("  invalid_param_count = %lu\r\n", g_platform_spi_stats.invalid_param_count);
    BoardLog_Info("  timeout_count       = %lu\r\n", g_platform_spi_stats.timeout_count);
    BoardLog_Info("  busy_count          = %lu\r\n", g_platform_spi_stats.busy_count);
    BoardLog_Info("  bytes_total         = %lu\r\n", g_platform_spi_stats.bytes_total);
    BoardLog_Info("  last_device         = %u\r\n", g_platform_spi_stats.last_device);
    BoardLog_Info("  last_len            = %lu\r\n", g_platform_spi_stats.last_len);
    BoardLog_Info("  last_timeout_ms     = %lu\r\n", g_platform_spi_stats.last_timeout_ms);
    BoardLog_Info("  last_error          = %d\r\n", g_platform_spi_stats.last_error);
}
