#ifndef PLATFORM_SPI_H
#define PLATFORM_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Generic board-level SPI access layer.
 * This layer must not contain sensor-specific register logic.
 */

typedef enum
{
    PLATFORM_SPI_OK = 0,
    PLATFORM_SPI_ERROR = -1,
    PLATFORM_SPI_INVALID_PARAM = -2,
    PLATFORM_SPI_HAL_ERROR = -3,
    PLATFORM_SPI_TIMEOUT = -4,
    PLATFORM_SPI_BUSY = -5
} PlatformSpiResult_t;

typedef enum
{
    PLATFORM_SPI_DEVICE_0 = 0,
    PLATFORM_SPI_DEVICE_1 = 1,
    PLATFORM_SPI_DEVICE_COUNT
} PlatformSpiDevice_t;

typedef struct
{
    uint32_t init_count;
    uint32_t transfer_count;
    uint32_t error_count;
    uint32_t invalid_param_count;
    uint32_t timeout_count;
    uint32_t busy_count;
    uint32_t bytes_total;
    uint32_t last_len;
    uint32_t last_timeout_ms;
    uint8_t last_device;
    int last_error;
} PlatformSpiStats_t;

void PlatformSpi_Init(void);

int PlatformSpi_Transfer(PlatformSpiDevice_t dev,
                         const uint8_t *tx_buf,
                         uint8_t *rx_buf,
                         uint16_t len,
                         uint32_t timeout_ms);

void PlatformSpi_Select(PlatformSpiDevice_t dev);
void PlatformSpi_Deselect(PlatformSpiDevice_t dev);
void PlatformSpi_DeselectAll(void);

const PlatformSpiStats_t *PlatformSpi_GetStats(void);
void PlatformSpi_ResetStats(void);
void PlatformSpi_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_SPI_H */
