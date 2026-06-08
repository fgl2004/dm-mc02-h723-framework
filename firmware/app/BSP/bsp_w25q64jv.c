#include "bsp_w25q64jv.h"

#include "platform_qspi.h"
#include "platform_time.h"
#include "board_log.h"

#include <string.h>

/*
 * W25Q64JV first-version BSP.
 *
 * Bring-up strategy:
 *   1. Use 1-1-1 standard SPI command mode through OCTOSPI.
 *   2. Verify JEDEC ID first.
 *   3. Verify read/write/erase on a dedicated test sector.
 *   4. Quad read/write and memory-mapped mode can be enabled later.
 */

/* W25Q command set. */
#define W25Q_CMD_WRITE_ENABLE              0x06U
#define W25Q_CMD_WRITE_DISABLE             0x04U
#define W25Q_CMD_READ_STATUS_REG1          0x05U
#define W25Q_CMD_READ_STATUS_REG2          0x35U
#define W25Q_CMD_READ_STATUS_REG3          0x15U
#define W25Q_CMD_WRITE_STATUS_REG1         0x01U
#define W25Q_CMD_PAGE_PROGRAM              0x02U
#define W25Q_CMD_READ_DATA                 0x03U
#define W25Q_CMD_FAST_READ                 0x0BU
#define W25Q_CMD_SECTOR_ERASE_4K           0x20U
#define W25Q_CMD_BLOCK_ERASE_32K           0x52U
#define W25Q_CMD_BLOCK_ERASE_64K           0xD8U
#define W25Q_CMD_CHIP_ERASE                0xC7U
#define W25Q_CMD_READ_JEDEC_ID             0x9FU
#define W25Q_CMD_READ_UNIQUE_ID            0x4BU
#define W25Q_CMD_ENABLE_RESET              0x66U
#define W25Q_CMD_RESET_DEVICE              0x99U

/* Status register bits. */
#define W25Q_SR1_BUSY_MASK                 0x01U

/* Expected JEDEC for Winbond W25Q64. */
#define W25Q_EXPECTED_MANUFACTURER_ID      0xEFU
#define W25Q_EXPECTED_MEMORY_TYPE          0x40U
#define W25Q_EXPECTED_CAPACITY_ID          0x17U

#define W25Q_DEFAULT_TIMEOUT_MS            100U
#define W25Q_ERASE_TIMEOUT_MS              5000U
#define W25Q_CHIP_ERASE_TIMEOUT_MS         120000U

#define W25Q_ADDRESS_BITS                  24U

static BspW25q64Stats_t g_w25q_stats;

static const BspW25q64Info_t g_w25q_info =
{
    BSP_W25Q64_CAPACITY_BYTES,
    BSP_W25Q64_PAGE_SIZE,
    BSP_W25Q64_SECTOR_SIZE,
    BSP_W25Q64_BLOCK_32K_SIZE,
    BSP_W25Q64_BLOCK_64K_SIZE
};

static void BspW25q64_RecordError(int error)
{
    g_w25q_stats.last_error = error;

    if (error == BSP_W25Q64_OK)
    {
        return;
    }

    if (error == BSP_W25Q64_INVALID_PARAM)
    {
        g_w25q_stats.invalid_param_count++;
    }
    else if (error == BSP_W25Q64_QSPI_ERROR)
    {
        g_w25q_stats.qspi_error_count++;
    }
    else if (error == BSP_W25Q64_TIMEOUT)
    {
        g_w25q_stats.timeout_count++;
    }
    else if (error == BSP_W25Q64_ID_ERROR)
    {
        g_w25q_stats.id_error_count++;
    }
    else if (error == BSP_W25Q64_ADDR_ERROR)
    {
        g_w25q_stats.addr_error_count++;
    }
    else if (error == BSP_W25Q64_PAGE_BOUNDARY_ERROR)
    {
        g_w25q_stats.page_boundary_error_count++;
    }
    else if (error == BSP_W25Q64_VERIFY_ERROR)
    {
        g_w25q_stats.verify_error_count++;
    }
}

static uint8_t BspW25q64_IsAddressValid(uint32_t addr, uint32_t len)
{
    if (len == 0U)
    {
        return 0U;
    }

    if (addr >= BSP_W25Q64_CAPACITY_BYTES)
    {
        return 0U;
    }

    if (len > (BSP_W25Q64_CAPACITY_BYTES - addr))
    {
        return 0U;
    }

    return 1U;
}

static void BspW25q64_MakeCommand(PlatformQspiCommand_t *cmd,
                                  uint8_t instruction,
                                  uint32_t addr,
                                  uint8_t has_addr,
                                  PlatformQspiLineMode_t data_lines,
                                  uint32_t data_len,
                                  uint32_t dummy_cycles)
{
    memset(cmd, 0, sizeof(*cmd));

    cmd->instruction = instruction;
    cmd->instruction_lines = PLATFORM_QSPI_LINE_1;

    cmd->address = addr;
    cmd->address_lines = has_addr ? PLATFORM_QSPI_LINE_1 : PLATFORM_QSPI_LINE_NONE;
    cmd->address_size = W25Q_ADDRESS_BITS;

    cmd->alternate_bytes = 0U;
    cmd->alternate_bytes_lines = PLATFORM_QSPI_LINE_NONE;
    cmd->alternate_bytes_size = 8U;

    cmd->data_lines = data_lines;
    cmd->dummy_cycles = dummy_cycles;
    cmd->data_len = data_len;

    cmd->dtr_mode = PLATFORM_QSPI_DTR_DISABLE;
    cmd->sioo_mode = PLATFORM_QSPI_SIOO_EVERY_CMD;
}

static int BspW25q64_CommandOnly(uint8_t instruction)
{
    PlatformQspiCommand_t cmd;
    int ret;

    BspW25q64_MakeCommand(&cmd,
                          instruction,
                          0U,
                          0U,
                          PLATFORM_QSPI_LINE_NONE,
                          0U,
                          0U);

    ret = PlatformQspi_Command(&cmd, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    BspW25q64_RecordError(BSP_W25Q64_OK);
    return BSP_W25Q64_OK;
}

int BspW25q64_Init(void)
{
    BspW25q64JedecId_t id;
    int ret;

    memset(&g_w25q_stats, 0, sizeof(g_w25q_stats));

    g_w25q_stats.init_count++;

    /*
     * Reset sequence is safe for W25Q devices.
     * If a debugger reset left the flash in an odd state, this improves bring-up.
     */
    (void)BspW25q64_CommandOnly(W25Q_CMD_ENABLE_RESET);
    (void)BspW25q64_CommandOnly(W25Q_CMD_RESET_DEVICE);
    PlatformTime_DelayMs(10U);

    ret = BspW25q64_ReadJedecId(&id);
    if (ret != BSP_W25Q64_OK)
    {
        BspW25q64_RecordError(ret);
        return ret;
    }

    if ((id.manufacturer_id != W25Q_EXPECTED_MANUFACTURER_ID) ||
        (id.memory_type != W25Q_EXPECTED_MEMORY_TYPE) ||
        (id.capacity_id != W25Q_EXPECTED_CAPACITY_ID))
    {
        BspW25q64_RecordError(BSP_W25Q64_ID_ERROR);
        return BSP_W25Q64_ID_ERROR;
    }

    g_w25q_stats.initialized = 1U;
    g_w25q_stats.last_manufacturer_id = id.manufacturer_id;
    g_w25q_stats.last_memory_type = id.memory_type;
    g_w25q_stats.last_capacity_id = id.capacity_id;
    BspW25q64_RecordError(BSP_W25Q64_OK);

    BoardLog_Info("BspW25q64 init OK, JEDEC=%02X %02X %02X\r\n",
                  id.manufacturer_id,
                  id.memory_type,
                  id.capacity_id);

    return BSP_W25Q64_OK;
}

int BspW25q64_ReadJedecId(BspW25q64JedecId_t *id)
{
    PlatformQspiCommand_t cmd;
    uint8_t buf[3];
    int ret;

    if (id == NULL)
    {
        BspW25q64_RecordError(BSP_W25Q64_INVALID_PARAM);
        return BSP_W25Q64_INVALID_PARAM;
    }

    BspW25q64_MakeCommand(&cmd,
                          W25Q_CMD_READ_JEDEC_ID,
                          0U,
                          0U,
                          PLATFORM_QSPI_LINE_1,
                          (uint32_t)sizeof(buf),
                          0U);

    ret = PlatformQspi_Command(&cmd, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    ret = PlatformQspi_Receive(buf, (uint32_t)sizeof(buf), W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    id->manufacturer_id = buf[0];
    id->memory_type = buf[1];
    id->capacity_id = buf[2];

    g_w25q_stats.read_id_count++;
    g_w25q_stats.last_manufacturer_id = id->manufacturer_id;
    g_w25q_stats.last_memory_type = id->memory_type;
    g_w25q_stats.last_capacity_id = id->capacity_id;

    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

int BspW25q64_ReadUniqueId(BspW25q64UniqueId_t *uid)
{
    PlatformQspiCommand_t cmd;
    uint8_t buf[8];
    int ret;

    if (uid == NULL)
    {
        BspW25q64_RecordError(BSP_W25Q64_INVALID_PARAM);
        return BSP_W25Q64_INVALID_PARAM;
    }

    /*
     * Read Unique ID:
     *   0x4B + 24-bit dummy/address field + 8 dummy cycles + 64-bit UID.
     */
    BspW25q64_MakeCommand(&cmd,
                          W25Q_CMD_READ_UNIQUE_ID,
                          0U,
                          1U,
                          PLATFORM_QSPI_LINE_1,
                          (uint32_t)sizeof(buf),
                          8U);

    ret = PlatformQspi_Command(&cmd, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    ret = PlatformQspi_Receive(buf, (uint32_t)sizeof(buf), W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    memcpy(uid->bytes, buf, sizeof(uid->bytes));

    g_w25q_stats.read_uid_count++;
    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

static int BspW25q64_ReadStatusByCmd(uint8_t instruction, uint8_t *status)
{
    PlatformQspiCommand_t cmd;
    int ret;

    if (status == NULL)
    {
        BspW25q64_RecordError(BSP_W25Q64_INVALID_PARAM);
        return BSP_W25Q64_INVALID_PARAM;
    }

    BspW25q64_MakeCommand(&cmd,
                          instruction,
                          0U,
                          0U,
                          PLATFORM_QSPI_LINE_1,
                          1U,
                          0U);

    ret = PlatformQspi_Command(&cmd, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    ret = PlatformQspi_Receive(status, 1U, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    g_w25q_stats.read_status_count++;
    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

int BspW25q64_ReadStatus1(uint8_t *status)
{
    int ret = BspW25q64_ReadStatusByCmd(W25Q_CMD_READ_STATUS_REG1, status);
    if ((ret == BSP_W25Q64_OK) && (status != NULL))
    {
        g_w25q_stats.last_status1 = *status;
    }
    return ret;
}

int BspW25q64_ReadStatus2(uint8_t *status)
{
    int ret = BspW25q64_ReadStatusByCmd(W25Q_CMD_READ_STATUS_REG2, status);
    if ((ret == BSP_W25Q64_OK) && (status != NULL))
    {
        g_w25q_stats.last_status2 = *status;
    }
    return ret;
}

int BspW25q64_ReadStatus3(uint8_t *status)
{
    int ret = BspW25q64_ReadStatusByCmd(W25Q_CMD_READ_STATUS_REG3, status);
    if ((ret == BSP_W25Q64_OK) && (status != NULL))
    {
        g_w25q_stats.last_status3 = *status;
    }
    return ret;
}

int BspW25q64_WriteEnable(void)
{
    int ret = BspW25q64_CommandOnly(W25Q_CMD_WRITE_ENABLE);
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    g_w25q_stats.write_enable_count++;
    return BSP_W25Q64_OK;
}

int BspW25q64_IsBusy(uint8_t *busy)
{
    uint8_t status;
    int ret;

    if (busy == NULL)
    {
        BspW25q64_RecordError(BSP_W25Q64_INVALID_PARAM);
        return BSP_W25Q64_INVALID_PARAM;
    }

    ret = BspW25q64_ReadStatus1(&status);
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    *busy = ((status & W25Q_SR1_BUSY_MASK) != 0U) ? 1U : 0U;

    return BSP_W25Q64_OK;
}

int BspW25q64_WaitReady(uint32_t timeout_ms)
{
    uint32_t start_ms;
    uint8_t busy;
    int ret;

    start_ms = PlatformTime_GetMs();

    while (1)
    {
        ret = BspW25q64_IsBusy(&busy);
        if (ret != BSP_W25Q64_OK)
        {
            return ret;
        }

        if (busy == 0U)
        {
            g_w25q_stats.wait_ready_count++;
            BspW25q64_RecordError(BSP_W25Q64_OK);
            return BSP_W25Q64_OK;
        }

        if ((PlatformTime_GetMs() - start_ms) >= timeout_ms)
        {
            BspW25q64_RecordError(BSP_W25Q64_TIMEOUT);
            return BSP_W25Q64_TIMEOUT;
        }
    }
}

int BspW25q64_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    PlatformQspiCommand_t cmd;
    int ret;

    if ((buf == NULL) || (BspW25q64_IsAddressValid(addr, len) == 0U))
    {
        BspW25q64_RecordError(BSP_W25Q64_INVALID_PARAM);
        return BSP_W25Q64_INVALID_PARAM;
    }

    BspW25q64_MakeCommand(&cmd,
                          W25Q_CMD_READ_DATA,
                          addr,
                          1U,
                          PLATFORM_QSPI_LINE_1,
                          len,
                          0U);

    ret = PlatformQspi_Command(&cmd, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    ret = PlatformQspi_Receive(buf, len, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    g_w25q_stats.read_count++;
    g_w25q_stats.read_bytes += len;
    g_w25q_stats.last_addr = addr;
    g_w25q_stats.last_len = len;
    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

int BspW25q64_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    PlatformQspiCommand_t cmd;
    uint32_t page_offset;
    int ret;

    if ((buf == NULL) || (BspW25q64_IsAddressValid(addr, len) == 0U))
    {
        BspW25q64_RecordError(BSP_W25Q64_INVALID_PARAM);
        return BSP_W25Q64_INVALID_PARAM;
    }

    if (len > BSP_W25Q64_PAGE_SIZE)
    {
        BspW25q64_RecordError(BSP_W25Q64_PAGE_BOUNDARY_ERROR);
        return BSP_W25Q64_PAGE_BOUNDARY_ERROR;
    }

    page_offset = addr % BSP_W25Q64_PAGE_SIZE;
    if ((page_offset + len) > BSP_W25Q64_PAGE_SIZE)
    {
        BspW25q64_RecordError(BSP_W25Q64_PAGE_BOUNDARY_ERROR);
        return BSP_W25Q64_PAGE_BOUNDARY_ERROR;
    }

    ret = BspW25q64_WriteEnable();
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    BspW25q64_MakeCommand(&cmd,
                          W25Q_CMD_PAGE_PROGRAM,
                          addr,
                          1U,
                          PLATFORM_QSPI_LINE_1,
                          len,
                          0U);

    ret = PlatformQspi_Command(&cmd, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    ret = PlatformQspi_Transmit(buf, len, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    ret = BspW25q64_WaitReady(W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    g_w25q_stats.page_program_count++;
    g_w25q_stats.program_bytes += len;
    g_w25q_stats.last_addr = addr;
    g_w25q_stats.last_len = len;
    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

static int BspW25q64_EraseByCmd(uint8_t instruction,
                                uint32_t addr,
                                uint32_t erase_size,
                                uint32_t timeout_ms)
{
    PlatformQspiCommand_t cmd;
    int ret;

    if ((addr >= BSP_W25Q64_CAPACITY_BYTES) || ((addr % erase_size) != 0U))
    {
        BspW25q64_RecordError(BSP_W25Q64_ADDR_ERROR);
        return BSP_W25Q64_ADDR_ERROR;
    }

    ret = BspW25q64_WriteEnable();
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    BspW25q64_MakeCommand(&cmd,
                          instruction,
                          addr,
                          1U,
                          PLATFORM_QSPI_LINE_NONE,
                          0U,
                          0U);

    ret = PlatformQspi_Command(&cmd, W25Q_DEFAULT_TIMEOUT_MS);
    if (ret != PLATFORM_QSPI_OK)
    {
        BspW25q64_RecordError(BSP_W25Q64_QSPI_ERROR);
        return BSP_W25Q64_QSPI_ERROR;
    }

    ret = BspW25q64_WaitReady(timeout_ms);
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    g_w25q_stats.erase_bytes += erase_size;
    g_w25q_stats.last_addr = addr;
    g_w25q_stats.last_len = erase_size;
    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

int BspW25q64_SectorErase(uint32_t addr)
{
    int ret = BspW25q64_EraseByCmd(W25Q_CMD_SECTOR_ERASE_4K,
                                   addr,
                                   BSP_W25Q64_SECTOR_SIZE,
                                   W25Q_ERASE_TIMEOUT_MS);
    if (ret == BSP_W25Q64_OK)
    {
        g_w25q_stats.sector_erase_count++;
    }

    return ret;
}

int BspW25q64_BlockErase32K(uint32_t addr)
{
    int ret = BspW25q64_EraseByCmd(W25Q_CMD_BLOCK_ERASE_32K,
                                   addr,
                                   BSP_W25Q64_BLOCK_32K_SIZE,
                                   W25Q_ERASE_TIMEOUT_MS);
    if (ret == BSP_W25Q64_OK)
    {
        g_w25q_stats.block_erase_count++;
    }

    return ret;
}

int BspW25q64_BlockErase64K(uint32_t addr)
{
    int ret = BspW25q64_EraseByCmd(W25Q_CMD_BLOCK_ERASE_64K,
                                   addr,
                                   BSP_W25Q64_BLOCK_64K_SIZE,
                                   W25Q_ERASE_TIMEOUT_MS);
    if (ret == BSP_W25Q64_OK)
    {
        g_w25q_stats.block_erase_count++;
    }

    return ret;
}

int BspW25q64_ChipErase(void)
{
    int ret;

    ret = BspW25q64_WriteEnable();
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    ret = BspW25q64_CommandOnly(W25Q_CMD_CHIP_ERASE);
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    ret = BspW25q64_WaitReady(W25Q_CHIP_ERASE_TIMEOUT_MS);
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    g_w25q_stats.chip_erase_count++;
    g_w25q_stats.erase_bytes += BSP_W25Q64_CAPACITY_BYTES;
    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

int BspW25q64_WriteReadTest(uint32_t test_addr)
{
    uint8_t write_buf[64];
    uint8_t read_buf[64];
    uint32_t i;
    int ret;

    if ((test_addr % BSP_W25Q64_SECTOR_SIZE) != 0U)
    {
        BspW25q64_RecordError(BSP_W25Q64_ADDR_ERROR);
        return BSP_W25Q64_ADDR_ERROR;
    }

    if (BspW25q64_IsAddressValid(test_addr, BSP_W25Q64_SECTOR_SIZE) == 0U)
    {
        BspW25q64_RecordError(BSP_W25Q64_ADDR_ERROR);
        return BSP_W25Q64_ADDR_ERROR;
    }

    for (i = 0U; i < (uint32_t)sizeof(write_buf); i++)
    {
        write_buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
        read_buf[i] = 0U;
    }

    ret = BspW25q64_SectorErase(test_addr);
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    ret = BspW25q64_PageProgram(test_addr, write_buf, (uint32_t)sizeof(write_buf));
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    ret = BspW25q64_Read(test_addr, read_buf, (uint32_t)sizeof(read_buf));
    if (ret != BSP_W25Q64_OK)
    {
        return ret;
    }

    if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0)
    {
        BspW25q64_RecordError(BSP_W25Q64_VERIFY_ERROR);
        return BSP_W25Q64_VERIFY_ERROR;
    }

    BspW25q64_RecordError(BSP_W25Q64_OK);

    return BSP_W25Q64_OK;
}

const BspW25q64Info_t *BspW25q64_GetInfo(void)
{
    return &g_w25q_info;
}

const BspW25q64Stats_t *BspW25q64_GetStats(void)
{
    return &g_w25q_stats;
}

void BspW25q64_ResetStats(void)
{
    uint8_t initialized = g_w25q_stats.initialized;
    uint8_t mid = g_w25q_stats.last_manufacturer_id;
    uint8_t type = g_w25q_stats.last_memory_type;
    uint8_t cap = g_w25q_stats.last_capacity_id;

    memset(&g_w25q_stats, 0, sizeof(g_w25q_stats));

    g_w25q_stats.initialized = initialized;
    g_w25q_stats.last_manufacturer_id = mid;
    g_w25q_stats.last_memory_type = type;
    g_w25q_stats.last_capacity_id = cap;
}

void BspW25q64_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("BspW25q64 Stats:\r\n");
    BoardLog_Info("  initialized          = %u\r\n", g_w25q_stats.initialized);
    BoardLog_Info("  init_count           = %lu\r\n", g_w25q_stats.init_count);
    BoardLog_Info("  read_id_count        = %lu\r\n", g_w25q_stats.read_id_count);
    BoardLog_Info("  read_uid_count       = %lu\r\n", g_w25q_stats.read_uid_count);
    BoardLog_Info("  read_status_count    = %lu\r\n", g_w25q_stats.read_status_count);
    BoardLog_Info("  write_enable_count   = %lu\r\n", g_w25q_stats.write_enable_count);
    BoardLog_Info("  wait_ready_count     = %lu\r\n", g_w25q_stats.wait_ready_count);
    BoardLog_Info("  read_count           = %lu\r\n", g_w25q_stats.read_count);
    BoardLog_Info("  page_program_count   = %lu\r\n", g_w25q_stats.page_program_count);
    BoardLog_Info("  sector_erase_count   = %lu\r\n", g_w25q_stats.sector_erase_count);
    BoardLog_Info("  block_erase_count    = %lu\r\n", g_w25q_stats.block_erase_count);
    BoardLog_Info("  chip_erase_count     = %lu\r\n", g_w25q_stats.chip_erase_count);
    BoardLog_Info("  qspi_error_count     = %lu\r\n", g_w25q_stats.qspi_error_count);
    BoardLog_Info("  timeout_count        = %lu\r\n", g_w25q_stats.timeout_count);
    BoardLog_Info("  id_error_count       = %lu\r\n", g_w25q_stats.id_error_count);
    BoardLog_Info("  verify_error_count   = %lu\r\n", g_w25q_stats.verify_error_count);
    BoardLog_Info("  read_bytes           = %lu\r\n", g_w25q_stats.read_bytes);
    BoardLog_Info("  program_bytes        = %lu\r\n", g_w25q_stats.program_bytes);
    BoardLog_Info("  erase_bytes          = %lu\r\n", g_w25q_stats.erase_bytes);
    BoardLog_Info("  last_jedec           = %02X %02X %02X\r\n",
                  g_w25q_stats.last_manufacturer_id,
                  g_w25q_stats.last_memory_type,
                  g_w25q_stats.last_capacity_id);
    BoardLog_Info("  last_status          = S1=0x%02X S2=0x%02X S3=0x%02X\r\n",
                  g_w25q_stats.last_status1,
                  g_w25q_stats.last_status2,
                  g_w25q_stats.last_status3);
    BoardLog_Info("  last_addr            = 0x%06lX\r\n", g_w25q_stats.last_addr);
    BoardLog_Info("  last_len             = %lu\r\n", g_w25q_stats.last_len);
    BoardLog_Info("  last_error           = %d\r\n", g_w25q_stats.last_error);
}
