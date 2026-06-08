#ifndef PLATFORM_QSPI_H
#define PLATFORM_QSPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Generic OCTOSPI/QSPI platform layer.
 *
 * Design rule:
 *   - This layer only wraps STM32 HAL OSPI access.
 *   - This layer must not contain W25Q64JV command/register logic.
 *   - BSP layer decides command/address/data line mode and instruction code.
 */

typedef enum
{
    PLATFORM_QSPI_OK = 0,
    PLATFORM_QSPI_ERROR = -1,
    PLATFORM_QSPI_INVALID_PARAM = -2,
    PLATFORM_QSPI_HAL_ERROR = -3,
    PLATFORM_QSPI_TIMEOUT = -4,
    PLATFORM_QSPI_BUSY = -5,
    PLATFORM_QSPI_NOT_INITIALIZED = -6
} PlatformQspiResult_t;

typedef enum
{
    PLATFORM_QSPI_LINE_NONE = 0,
    PLATFORM_QSPI_LINE_1    = 1,
    PLATFORM_QSPI_LINE_2    = 2,
    PLATFORM_QSPI_LINE_4    = 4,
    PLATFORM_QSPI_LINE_8    = 8
} PlatformQspiLineMode_t;

typedef enum
{
    PLATFORM_QSPI_DTR_DISABLE = 0,
    PLATFORM_QSPI_DTR_ENABLE  = 1
} PlatformQspiDtrMode_t;

typedef enum
{
    PLATFORM_QSPI_SIOO_EVERY_CMD = 0,
    PLATFORM_QSPI_SIOO_INST_ONLY_FIRST_CMD = 1
} PlatformQspiSiooMode_t;

typedef struct
{
    uint8_t instruction;
    PlatformQspiLineMode_t instruction_lines;

    uint32_t address;
    PlatformQspiLineMode_t address_lines;
    uint32_t address_size;

    uint32_t alternate_bytes;
    PlatformQspiLineMode_t alternate_bytes_lines;
    uint32_t alternate_bytes_size;

    PlatformQspiLineMode_t data_lines;
    uint32_t dummy_cycles;
    uint32_t data_len;

    PlatformQspiDtrMode_t dtr_mode;
    PlatformQspiSiooMode_t sioo_mode;
} PlatformQspiCommand_t;

typedef struct
{
    uint8_t match;
    uint8_t mask;
    uint32_t interval;
    uint32_t automatic_stop;
    uint32_t timeout_ms;
} PlatformQspiPolling_t;

typedef struct
{
    uint32_t init_count;

    uint32_t command_count;
    uint32_t transmit_count;
    uint32_t receive_count;
    uint32_t polling_count;

    uint32_t error_count;
    uint32_t invalid_param_count;
    uint32_t timeout_count;
    uint32_t busy_count;
    uint32_t not_initialized_count;

    uint32_t tx_bytes;
    uint32_t rx_bytes;

    uint32_t last_cmd;
    uint32_t last_addr;
    uint32_t last_len;
    uint32_t last_timeout_ms;

    uint32_t last_command_us;
    uint32_t max_command_us;
    uint32_t last_transmit_us;
    uint32_t max_transmit_us;
    uint32_t last_receive_us;
    uint32_t max_receive_us;
    uint32_t last_polling_us;
    uint32_t max_polling_us;

    uint8_t initialized;
    int last_error;
} PlatformQspiStats_t;

void PlatformQspi_Init(void);

int PlatformQspi_Command(const PlatformQspiCommand_t *cmd, uint32_t timeout_ms);
int PlatformQspi_Transmit(const uint8_t *buf, uint32_t len, uint32_t timeout_ms);
int PlatformQspi_Receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms);
int PlatformQspi_AutoPolling(const PlatformQspiCommand_t *cmd,
                             const PlatformQspiPolling_t *polling);

const PlatformQspiStats_t *PlatformQspi_GetStats(void);
void PlatformQspi_ResetStats(void);
void PlatformQspi_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_QSPI_H */
