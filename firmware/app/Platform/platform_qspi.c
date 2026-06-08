#include "platform_qspi.h"

#include "octospi.h"
#include "board_log.h"
#include "platform_time.h"

#include <string.h>

/*
 * This file wraps STM32 HAL OSPI APIs.
 *
 * Current CubeMX generated handle:
 *   OSPI_HandleTypeDef hospi1;
 *
 * Current CubeMX configuration is in octospi.c:
 *   OCTOSPI1
 *   Quad SPI mode
 *   OCTOSPIM Port1
 *   Clock prescaler = 4
 *   GPIO:
 *     PA1  -> IO3
 *     PA3  -> IO2
 *     PB0  -> IO1
 *     PB2  -> CLK
 *     PE11 -> NCS
 *     PD11 -> IO0
 */

static PlatformQspiStats_t g_platform_qspi_stats;

static int PlatformQspi_ConvertHalStatus(HAL_StatusTypeDef status)
{
    switch (status)
    {
        case HAL_OK:
            return PLATFORM_QSPI_OK;

        case HAL_BUSY:
            return PLATFORM_QSPI_BUSY;

        case HAL_TIMEOUT:
            return PLATFORM_QSPI_TIMEOUT;

        case HAL_ERROR:
        default:
            return PLATFORM_QSPI_HAL_ERROR;
    }
}

static uint32_t PlatformQspi_ConvertInstructionLineMode(PlatformQspiLineMode_t line_mode)
{
    switch (line_mode)
    {
        case PLATFORM_QSPI_LINE_NONE:
            return HAL_OSPI_INSTRUCTION_NONE;
        case PLATFORM_QSPI_LINE_1:
            return HAL_OSPI_INSTRUCTION_1_LINE;
        case PLATFORM_QSPI_LINE_2:
            return HAL_OSPI_INSTRUCTION_2_LINES;
        case PLATFORM_QSPI_LINE_4:
            return HAL_OSPI_INSTRUCTION_4_LINES;
        case PLATFORM_QSPI_LINE_8:
            return HAL_OSPI_INSTRUCTION_8_LINES;
        default:
            return HAL_OSPI_INSTRUCTION_NONE;
    }
}

static uint32_t PlatformQspi_ConvertAddressLineMode(PlatformQspiLineMode_t line_mode)
{
    switch (line_mode)
    {
        case PLATFORM_QSPI_LINE_NONE:
            return HAL_OSPI_ADDRESS_NONE;
        case PLATFORM_QSPI_LINE_1:
            return HAL_OSPI_ADDRESS_1_LINE;
        case PLATFORM_QSPI_LINE_2:
            return HAL_OSPI_ADDRESS_2_LINES;
        case PLATFORM_QSPI_LINE_4:
            return HAL_OSPI_ADDRESS_4_LINES;
        case PLATFORM_QSPI_LINE_8:
            return HAL_OSPI_ADDRESS_8_LINES;
        default:
            return HAL_OSPI_ADDRESS_NONE;
    }
}

static uint32_t PlatformQspi_ConvertAlternateLineMode(PlatformQspiLineMode_t line_mode)
{
    switch (line_mode)
    {
        case PLATFORM_QSPI_LINE_NONE:
            return HAL_OSPI_ALTERNATE_BYTES_NONE;
        case PLATFORM_QSPI_LINE_1:
            return HAL_OSPI_ALTERNATE_BYTES_1_LINE;
        case PLATFORM_QSPI_LINE_2:
            return HAL_OSPI_ALTERNATE_BYTES_2_LINES;
        case PLATFORM_QSPI_LINE_4:
            return HAL_OSPI_ALTERNATE_BYTES_4_LINES;
        case PLATFORM_QSPI_LINE_8:
            return HAL_OSPI_ALTERNATE_BYTES_8_LINES;
        default:
            return HAL_OSPI_ALTERNATE_BYTES_NONE;
    }
}

static uint32_t PlatformQspi_ConvertDataLineMode(PlatformQspiLineMode_t line_mode)
{
    switch (line_mode)
    {
        case PLATFORM_QSPI_LINE_NONE:
            return HAL_OSPI_DATA_NONE;
        case PLATFORM_QSPI_LINE_1:
            return HAL_OSPI_DATA_1_LINE;
        case PLATFORM_QSPI_LINE_2:
            return HAL_OSPI_DATA_2_LINES;
        case PLATFORM_QSPI_LINE_4:
            return HAL_OSPI_DATA_4_LINES;
        case PLATFORM_QSPI_LINE_8:
            return HAL_OSPI_DATA_8_LINES;
        default:
            return HAL_OSPI_DATA_NONE;
    }
}

static uint32_t PlatformQspi_ConvertDtrMode(PlatformQspiDtrMode_t dtr)
{
    return 0;
}

static uint32_t PlatformQspi_ConvertSiooMode(PlatformQspiSiooMode_t sioo)
{
    return (sioo == PLATFORM_QSPI_SIOO_INST_ONLY_FIRST_CMD) ?
           HAL_OSPI_SIOO_INST_ONLY_FIRST_CMD :
           HAL_OSPI_SIOO_INST_EVERY_CMD;
}

static uint32_t PlatformQspi_NormalizeAddressSize(uint32_t address_size)
{
    switch (address_size)
    {
        case 8U:
            return HAL_OSPI_ADDRESS_8_BITS;
        case 16U:
            return HAL_OSPI_ADDRESS_16_BITS;
        case 24U:
            return HAL_OSPI_ADDRESS_24_BITS;
        case 32U:
            return HAL_OSPI_ADDRESS_32_BITS;
        default:
            return HAL_OSPI_ADDRESS_24_BITS;
    }
}

static uint32_t PlatformQspi_NormalizeAlternateSize(uint32_t alternate_size)
{
    switch (alternate_size)
    {
        case 8U:
            return HAL_OSPI_ALTERNATE_BYTES_8_BITS;
        case 16U:
            return HAL_OSPI_ALTERNATE_BYTES_16_BITS;
        case 24U:
            return HAL_OSPI_ALTERNATE_BYTES_24_BITS;
        case 32U:
            return HAL_OSPI_ALTERNATE_BYTES_32_BITS;
        default:
            return HAL_OSPI_ALTERNATE_BYTES_8_BITS;
    }
}

static void PlatformQspi_RecordError(int ret)
{
    g_platform_qspi_stats.last_error = ret;

    if (ret == PLATFORM_QSPI_OK)
    {
        return;
    }

    g_platform_qspi_stats.error_count++;

    if (ret == PLATFORM_QSPI_INVALID_PARAM)
    {
        g_platform_qspi_stats.invalid_param_count++;
    }
    else if (ret == PLATFORM_QSPI_TIMEOUT)
    {
        g_platform_qspi_stats.timeout_count++;
    }
    else if (ret == PLATFORM_QSPI_BUSY)
    {
        g_platform_qspi_stats.busy_count++;
    }
    else if (ret == PLATFORM_QSPI_NOT_INITIALIZED)
    {
        g_platform_qspi_stats.not_initialized_count++;
    }
}

static int PlatformQspi_BuildHalCommand(const PlatformQspiCommand_t *cmd,
                                        OSPI_RegularCmdTypeDef *hal_cmd)
{
    if ((cmd == NULL) || (hal_cmd == NULL))
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_INVALID_PARAM);
        return PLATFORM_QSPI_INVALID_PARAM;
    }

    memset(hal_cmd, 0, sizeof(*hal_cmd));

    hal_cmd->OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;

    hal_cmd->Instruction = cmd->instruction;
    hal_cmd->InstructionMode = PlatformQspi_ConvertInstructionLineMode(cmd->instruction_lines);
    hal_cmd->InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    hal_cmd->InstructionDtrMode = PlatformQspi_ConvertDtrMode(cmd->dtr_mode);

    hal_cmd->Address = cmd->address;
    hal_cmd->AddressMode = PlatformQspi_ConvertAddressLineMode(cmd->address_lines);
    hal_cmd->AddressSize = PlatformQspi_NormalizeAddressSize(cmd->address_size);
    hal_cmd->AddressDtrMode = PlatformQspi_ConvertDtrMode(cmd->dtr_mode);

    hal_cmd->AlternateBytes = cmd->alternate_bytes;
    hal_cmd->AlternateBytesMode = PlatformQspi_ConvertAlternateLineMode(cmd->alternate_bytes_lines);
    hal_cmd->AlternateBytesSize = PlatformQspi_NormalizeAlternateSize(cmd->alternate_bytes_size);
    hal_cmd->AlternateBytesDtrMode = PlatformQspi_ConvertDtrMode(cmd->dtr_mode);

    hal_cmd->DataMode = PlatformQspi_ConvertDataLineMode(cmd->data_lines);
    hal_cmd->NbData = cmd->data_len;
    hal_cmd->DataDtrMode = PlatformQspi_ConvertDtrMode(cmd->dtr_mode);

    hal_cmd->DummyCycles = cmd->dummy_cycles;
    hal_cmd->DQSMode = HAL_OSPI_DQS_DISABLE;
    hal_cmd->SIOOMode = PlatformQspi_ConvertSiooMode(cmd->sioo_mode);

    return PLATFORM_QSPI_OK;
}

void PlatformQspi_Init(void)
{
    memset(&g_platform_qspi_stats, 0, sizeof(g_platform_qspi_stats));

    /*
     * CubeMX normally calls MX_OCTOSPI1_Init() before App_Init().
     * Do not call HAL_OSPI_Init() repeatedly here.
     */
    g_platform_qspi_stats.initialized = 1U;
    g_platform_qspi_stats.init_count++;
    g_platform_qspi_stats.last_error = PLATFORM_QSPI_OK;

    BoardLog_Info("PlatformQspi init OK\r\n");
}

int PlatformQspi_Command(const PlatformQspiCommand_t *cmd, uint32_t timeout_ms)
{
    OSPI_RegularCmdTypeDef hal_cmd;
    HAL_StatusTypeDef hal_status;
    uint32_t start_cycle;
    uint32_t elapsed_us;
    int ret;

    if (g_platform_qspi_stats.initialized == 0U)
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_NOT_INITIALIZED);
        return PLATFORM_QSPI_NOT_INITIALIZED;
    }

    ret = PlatformQspi_BuildHalCommand(cmd, &hal_cmd);
    if (ret != PLATFORM_QSPI_OK)
    {
        return ret;
    }

    g_platform_qspi_stats.command_count++;
    g_platform_qspi_stats.last_cmd = cmd->instruction;
    g_platform_qspi_stats.last_addr = cmd->address;
    g_platform_qspi_stats.last_len = cmd->data_len;
    g_platform_qspi_stats.last_timeout_ms = timeout_ms;

    start_cycle = PlatformTime_ProfileStart();
    hal_status = HAL_OSPI_Command(&hospi1, &hal_cmd, timeout_ms);
    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);

    g_platform_qspi_stats.last_command_us = elapsed_us;
    if (elapsed_us > g_platform_qspi_stats.max_command_us)
    {
        g_platform_qspi_stats.max_command_us = elapsed_us;
    }

    ret = PlatformQspi_ConvertHalStatus(hal_status);
    PlatformQspi_RecordError(ret);

    return ret;
}

int PlatformQspi_Transmit(const uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_status;
    uint32_t start_cycle;
    uint32_t elapsed_us;
    int ret;

    if (g_platform_qspi_stats.initialized == 0U)
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_NOT_INITIALIZED);
        return PLATFORM_QSPI_NOT_INITIALIZED;
    }

    if ((buf == NULL) || (len == 0U))
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_INVALID_PARAM);
        return PLATFORM_QSPI_INVALID_PARAM;
    }

    g_platform_qspi_stats.transmit_count++;
    g_platform_qspi_stats.last_len = len;
    g_platform_qspi_stats.last_timeout_ms = timeout_ms;

    start_cycle = PlatformTime_ProfileStart();
    hal_status = HAL_OSPI_Transmit(&hospi1, (uint8_t *)buf, timeout_ms);
    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);

    g_platform_qspi_stats.last_transmit_us = elapsed_us;
    if (elapsed_us > g_platform_qspi_stats.max_transmit_us)
    {
        g_platform_qspi_stats.max_transmit_us = elapsed_us;
    }

    ret = PlatformQspi_ConvertHalStatus(hal_status);
    PlatformQspi_RecordError(ret);

    if (ret == PLATFORM_QSPI_OK)
    {
        g_platform_qspi_stats.tx_bytes += len;
    }

    return ret;
}

int PlatformQspi_Receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_status;
    uint32_t start_cycle;
    uint32_t elapsed_us;
    int ret;

    if (g_platform_qspi_stats.initialized == 0U)
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_NOT_INITIALIZED);
        return PLATFORM_QSPI_NOT_INITIALIZED;
    }

    if ((buf == NULL) || (len == 0U))
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_INVALID_PARAM);
        return PLATFORM_QSPI_INVALID_PARAM;
    }

    g_platform_qspi_stats.receive_count++;
    g_platform_qspi_stats.last_len = len;
    g_platform_qspi_stats.last_timeout_ms = timeout_ms;

    start_cycle = PlatformTime_ProfileStart();
    hal_status = HAL_OSPI_Receive(&hospi1, buf, timeout_ms);
    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);

    g_platform_qspi_stats.last_receive_us = elapsed_us;
    if (elapsed_us > g_platform_qspi_stats.max_receive_us)
    {
        g_platform_qspi_stats.max_receive_us = elapsed_us;
    }

    ret = PlatformQspi_ConvertHalStatus(hal_status);
    PlatformQspi_RecordError(ret);

    if (ret == PLATFORM_QSPI_OK)
    {
        g_platform_qspi_stats.rx_bytes += len;
    }

    return ret;
}

int PlatformQspi_AutoPolling(const PlatformQspiCommand_t *cmd,
                             const PlatformQspiPolling_t *polling)
{
    OSPI_RegularCmdTypeDef hal_cmd;
    OSPI_AutoPollingTypeDef hal_polling;
    HAL_StatusTypeDef hal_status;
    uint32_t start_cycle;
    uint32_t elapsed_us;
    int ret;

    if (g_platform_qspi_stats.initialized == 0U)
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_NOT_INITIALIZED);
        return PLATFORM_QSPI_NOT_INITIALIZED;
    }

    if ((cmd == NULL) || (polling == NULL))
    {
        PlatformQspi_RecordError(PLATFORM_QSPI_INVALID_PARAM);
        return PLATFORM_QSPI_INVALID_PARAM;
    }

    ret = PlatformQspi_BuildHalCommand(cmd, &hal_cmd);
    if (ret != PLATFORM_QSPI_OK)
    {
        return ret;
    }

    memset(&hal_polling, 0, sizeof(hal_polling));
    hal_polling.Match = polling->match;
    hal_polling.Mask = polling->mask;
    hal_polling.MatchMode = HAL_OSPI_MATCH_MODE_AND;
    hal_polling.Interval = polling->interval;
    hal_polling.AutomaticStop = polling->automatic_stop;

    g_platform_qspi_stats.polling_count++;
    g_platform_qspi_stats.last_cmd = cmd->instruction;
    g_platform_qspi_stats.last_len = cmd->data_len;
    g_platform_qspi_stats.last_timeout_ms = polling->timeout_ms;

    start_cycle = PlatformTime_ProfileStart();

    hal_status = HAL_OSPI_Command(&hospi1, &hal_cmd, polling->timeout_ms);
    if (hal_status == HAL_OK)
    {
        hal_status = HAL_OSPI_AutoPolling(&hospi1, &hal_polling, polling->timeout_ms);
    }

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);

    g_platform_qspi_stats.last_polling_us = elapsed_us;
    if (elapsed_us > g_platform_qspi_stats.max_polling_us)
    {
        g_platform_qspi_stats.max_polling_us = elapsed_us;
    }

    ret = PlatformQspi_ConvertHalStatus(hal_status);
    PlatformQspi_RecordError(ret);

    return ret;
}

const PlatformQspiStats_t *PlatformQspi_GetStats(void)
{
    return &g_platform_qspi_stats;
}

void PlatformQspi_ResetStats(void)
{
    uint8_t initialized = g_platform_qspi_stats.initialized;
    uint32_t init_count = g_platform_qspi_stats.init_count;

    memset(&g_platform_qspi_stats, 0, sizeof(g_platform_qspi_stats));

    g_platform_qspi_stats.initialized = initialized;
    g_platform_qspi_stats.init_count = init_count;
}

void PlatformQspi_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("PlatformQspi Stats:\r\n");
    BoardLog_Info("  initialized          = %u\r\n", g_platform_qspi_stats.initialized);
    BoardLog_Info("  init_count           = %lu\r\n", g_platform_qspi_stats.init_count);
    BoardLog_Info("  command_count        = %lu\r\n", g_platform_qspi_stats.command_count);
    BoardLog_Info("  transmit_count       = %lu\r\n", g_platform_qspi_stats.transmit_count);
    BoardLog_Info("  receive_count        = %lu\r\n", g_platform_qspi_stats.receive_count);
    BoardLog_Info("  polling_count        = %lu\r\n", g_platform_qspi_stats.polling_count);
    BoardLog_Info("  error_count          = %lu\r\n", g_platform_qspi_stats.error_count);
    BoardLog_Info("  timeout_count        = %lu\r\n", g_platform_qspi_stats.timeout_count);
    BoardLog_Info("  busy_count           = %lu\r\n", g_platform_qspi_stats.busy_count);
    BoardLog_Info("  invalid_param_count  = %lu\r\n", g_platform_qspi_stats.invalid_param_count);
    BoardLog_Info("  tx_bytes             = %lu\r\n", g_platform_qspi_stats.tx_bytes);
    BoardLog_Info("  rx_bytes             = %lu\r\n", g_platform_qspi_stats.rx_bytes);
    BoardLog_Info("  last_cmd             = 0x%02lX\r\n", g_platform_qspi_stats.last_cmd);
    BoardLog_Info("  last_addr            = 0x%06lX\r\n", g_platform_qspi_stats.last_addr);
    BoardLog_Info("  last_len             = %lu\r\n", g_platform_qspi_stats.last_len);
    BoardLog_Info("  last_error           = %d\r\n", g_platform_qspi_stats.last_error);
    BoardLog_Info("  last_command_us      = %lu\r\n", g_platform_qspi_stats.last_command_us);
    BoardLog_Info("  max_command_us       = %lu\r\n", g_platform_qspi_stats.max_command_us);
    BoardLog_Info("  last_transmit_us     = %lu\r\n", g_platform_qspi_stats.last_transmit_us);
    BoardLog_Info("  max_transmit_us      = %lu\r\n", g_platform_qspi_stats.max_transmit_us);
    BoardLog_Info("  last_receive_us      = %lu\r\n", g_platform_qspi_stats.last_receive_us);
    BoardLog_Info("  max_receive_us       = %lu\r\n", g_platform_qspi_stats.max_receive_us);
    BoardLog_Info("  last_polling_us      = %lu\r\n", g_platform_qspi_stats.last_polling_us);
    BoardLog_Info("  max_polling_us       = %lu\r\n", g_platform_qspi_stats.max_polling_us);
}
