#include "flash_block_device.h"

#include "bsp_w25q64jv.h"
#include "board_log.h"

#include <string.h>

static const FlashBlockDeviceInfo_t g_flash_block_device_info =
{
    BSP_W25Q64_CAPACITY_BYTES,
    1U,
    BSP_W25Q64_PAGE_SIZE,
    BSP_W25Q64_SECTOR_SIZE,
    BSP_W25Q64_BLOCK_32K_SIZE,
    BSP_W25Q64_BLOCK_64K_SIZE
};

static FlashBlockDeviceStats_t g_flash_block_device_stats;

static void FlashBlockDevice_RecordError(int error)
{
    g_flash_block_device_stats.last_error = error;

    if (error == FLASH_BLOCK_DEVICE_OK)
    {
        return;
    }

    if (error == FLASH_BLOCK_DEVICE_INVALID_PARAM)
    {
        g_flash_block_device_stats.invalid_param_count++;
    }
    else if (error == FLASH_BLOCK_DEVICE_NOT_INITIALIZED)
    {
        g_flash_block_device_stats.not_initialized_count++;
    }
    else if (error == FLASH_BLOCK_DEVICE_ADDR_ERROR)
    {
        g_flash_block_device_stats.addr_error_count++;
    }
    else if (error == FLASH_BLOCK_DEVICE_ALIGN_ERROR)
    {
        g_flash_block_device_stats.align_error_count++;
    }
    else if (error == FLASH_BLOCK_DEVICE_BSP_ERROR)
    {
        g_flash_block_device_stats.bsp_error_count++;
    }
    else if (error == FLASH_BLOCK_DEVICE_VERIFY_ERROR)
    {
        g_flash_block_device_stats.verify_error_count++;
    }
}

static uint8_t FlashBlockDevice_IsInitialized(void)
{
    return g_flash_block_device_stats.initialized;
}

static uint8_t FlashBlockDevice_IsRangeValid(uint32_t addr, uint32_t len)
{
    if (len == 0U)
    {
        return 0U;
    }

    if (addr >= g_flash_block_device_info.capacity_bytes)
    {
        return 0U;
    }

    if (len > (g_flash_block_device_info.capacity_bytes - addr))
    {
        return 0U;
    }

    return 1U;
}

int FlashBlockDevice_Init(void)
{
    const BspW25q64Info_t *bsp_info;
    int ret;

    memset(&g_flash_block_device_stats, 0, sizeof(g_flash_block_device_stats));
    g_flash_block_device_stats.init_count++;

    /*
     * If BspW25q64_Init() has already been called by app_main test code, calling
     * it again is acceptable for bring-up, but later StorageApp should own init
     * order more strictly.
     */
    ret = BspW25q64_Init();
    if (ret != BSP_W25Q64_OK)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_BSP_ERROR);
        return FLASH_BLOCK_DEVICE_BSP_ERROR;
    }

    bsp_info = BspW25q64_GetInfo();
    if ((bsp_info == 0) ||
        (bsp_info->capacity_bytes != g_flash_block_device_info.capacity_bytes) ||
        (bsp_info->page_size != g_flash_block_device_info.program_size) ||
        (bsp_info->sector_size != g_flash_block_device_info.erase_size))
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_ERROR);
        return FLASH_BLOCK_DEVICE_ERROR;
    }

    g_flash_block_device_stats.initialized = 1U;
    FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_OK);

    BoardLog_Info("FlashBlockDevice init OK: capacity=%lu, read=%lu, program=%lu, erase=%lu\r\n",
                  (unsigned long)g_flash_block_device_info.capacity_bytes,
                  (unsigned long)g_flash_block_device_info.read_size,
                  (unsigned long)g_flash_block_device_info.program_size,
                  (unsigned long)g_flash_block_device_info.erase_size);

    return FLASH_BLOCK_DEVICE_OK;
}

int FlashBlockDevice_Read(uint32_t addr, void *buf, uint32_t len)
{
    int ret;

    if (FlashBlockDevice_IsInitialized() == 0U)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_NOT_INITIALIZED);
        return FLASH_BLOCK_DEVICE_NOT_INITIALIZED;
    }

    if ((buf == 0) || (FlashBlockDevice_IsRangeValid(addr, len) == 0U))
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_INVALID_PARAM);
        return FLASH_BLOCK_DEVICE_INVALID_PARAM;
    }

    ret = BspW25q64_Read(addr, (uint8_t *)buf, len);
    if (ret != BSP_W25Q64_OK)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_BSP_ERROR);
        return FLASH_BLOCK_DEVICE_BSP_ERROR;
    }

    g_flash_block_device_stats.read_count++;
    g_flash_block_device_stats.read_bytes += len;
    g_flash_block_device_stats.last_addr = addr;
    g_flash_block_device_stats.last_len = len;
    FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_OK);

    return FLASH_BLOCK_DEVICE_OK;
}

int FlashBlockDevice_Program(uint32_t addr, const void *buf, uint32_t len)
{
    const uint8_t *src;
    uint32_t remaining;
    uint32_t current_addr;
    uint32_t page_offset;
    uint32_t chunk_len;
    int ret;

    if (FlashBlockDevice_IsInitialized() == 0U)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_NOT_INITIALIZED);
        return FLASH_BLOCK_DEVICE_NOT_INITIALIZED;
    }

    if ((buf == 0) || (FlashBlockDevice_IsRangeValid(addr, len) == 0U))
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_INVALID_PARAM);
        return FLASH_BLOCK_DEVICE_INVALID_PARAM;
    }

    src = (const uint8_t *)buf;
    remaining = len;
    current_addr = addr;

    while (remaining > 0U)
    {
        page_offset = current_addr % g_flash_block_device_info.program_size;
        chunk_len = g_flash_block_device_info.program_size - page_offset;

        if (chunk_len > remaining)
        {
            chunk_len = remaining;
        }

        ret = BspW25q64_PageProgram(current_addr, src, chunk_len);
        if (ret != BSP_W25Q64_OK)
        {
            FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_BSP_ERROR);
            return FLASH_BLOCK_DEVICE_BSP_ERROR;
        }

        g_flash_block_device_stats.split_program_count++;

        current_addr += chunk_len;
        src += chunk_len;
        remaining -= chunk_len;
    }

    g_flash_block_device_stats.program_count++;
    g_flash_block_device_stats.program_bytes += len;
    g_flash_block_device_stats.last_addr = addr;
    g_flash_block_device_stats.last_len = len;
    FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_OK);

    return FLASH_BLOCK_DEVICE_OK;
}

int FlashBlockDevice_Erase(uint32_t addr, uint32_t len)
{
    uint32_t remaining;
    uint32_t current_addr;
    int ret;

    if (FlashBlockDevice_IsInitialized() == 0U)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_NOT_INITIALIZED);
        return FLASH_BLOCK_DEVICE_NOT_INITIALIZED;
    }

    if (FlashBlockDevice_IsRangeValid(addr, len) == 0U)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_INVALID_PARAM);
        return FLASH_BLOCK_DEVICE_INVALID_PARAM;
    }

    if (((addr % g_flash_block_device_info.erase_size) != 0U) ||
        ((len % g_flash_block_device_info.erase_size) != 0U))
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_ALIGN_ERROR);
        return FLASH_BLOCK_DEVICE_ALIGN_ERROR;
    }

    current_addr = addr;
    remaining = len;

    while (remaining > 0U)
    {
        /*
         * Prefer larger erase commands when naturally aligned. This reduces
         * erase command count while keeping the external API simple.
         */
        if (((current_addr % g_flash_block_device_info.block_64k_size) == 0U) &&
            (remaining >= g_flash_block_device_info.block_64k_size))
        {
            ret = BspW25q64_BlockErase64K(current_addr);
            if (ret != BSP_W25Q64_OK)
            {
                FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_BSP_ERROR);
                return FLASH_BLOCK_DEVICE_BSP_ERROR;
            }

            g_flash_block_device_stats.block_64k_erase_count++;
            current_addr += g_flash_block_device_info.block_64k_size;
            remaining -= g_flash_block_device_info.block_64k_size;
        }
        else if (((current_addr % g_flash_block_device_info.block_32k_size) == 0U) &&
                 (remaining >= g_flash_block_device_info.block_32k_size))
        {
            ret = BspW25q64_BlockErase32K(current_addr);
            if (ret != BSP_W25Q64_OK)
            {
                FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_BSP_ERROR);
                return FLASH_BLOCK_DEVICE_BSP_ERROR;
            }

            g_flash_block_device_stats.block_32k_erase_count++;
            current_addr += g_flash_block_device_info.block_32k_size;
            remaining -= g_flash_block_device_info.block_32k_size;
        }
        else
        {
            ret = BspW25q64_SectorErase(current_addr);
            if (ret != BSP_W25Q64_OK)
            {
                FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_BSP_ERROR);
                return FLASH_BLOCK_DEVICE_BSP_ERROR;
            }

            g_flash_block_device_stats.sector_erase_count++;
            current_addr += g_flash_block_device_info.erase_size;
            remaining -= g_flash_block_device_info.erase_size;
        }
    }

    g_flash_block_device_stats.erase_count++;
    g_flash_block_device_stats.erase_bytes += len;
    g_flash_block_device_stats.last_addr = addr;
    g_flash_block_device_stats.last_len = len;
    FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_OK);

    return FLASH_BLOCK_DEVICE_OK;
}

int FlashBlockDevice_Sync(void)
{
    if (FlashBlockDevice_IsInitialized() == 0U)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_NOT_INITIALIZED);
        return FLASH_BLOCK_DEVICE_NOT_INITIALIZED;
    }

    /*
     * BSP waits until ready after program/erase.
     * There is no additional cache flush in this first version.
     */
    g_flash_block_device_stats.sync_count++;
    FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_OK);

    return FLASH_BLOCK_DEVICE_OK;
}

int FlashBlockDevice_WriteReadTest(uint32_t test_addr, uint32_t test_len)
{
    uint8_t write_buf[512];
    uint8_t read_buf[512];
    uint32_t i;
    int ret;

    if ((test_len == 0U) || (test_len > (uint32_t)sizeof(write_buf)))
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_INVALID_PARAM);
        return FLASH_BLOCK_DEVICE_INVALID_PARAM;
    }

    if ((test_addr % g_flash_block_device_info.erase_size) != 0U)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_ALIGN_ERROR);
        return FLASH_BLOCK_DEVICE_ALIGN_ERROR;
    }

    if (FlashBlockDevice_IsRangeValid(test_addr, g_flash_block_device_info.erase_size) == 0U)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_ADDR_ERROR);
        return FLASH_BLOCK_DEVICE_ADDR_ERROR;
    }

    for (i = 0U; i < test_len; i++)
    {
        write_buf[i] = (uint8_t)(0x5AU ^ (uint8_t)i);
        read_buf[i] = 0U;
    }

    ret = FlashBlockDevice_Erase(test_addr, g_flash_block_device_info.erase_size);
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        return ret;
    }

    ret = FlashBlockDevice_Program(test_addr, write_buf, test_len);
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        return ret;
    }

    ret = FlashBlockDevice_Read(test_addr, read_buf, test_len);
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        return ret;
    }

    g_flash_block_device_stats.verify_count++;

    if (memcmp(write_buf, read_buf, test_len) != 0)
    {
        FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_VERIFY_ERROR);
        return FLASH_BLOCK_DEVICE_VERIFY_ERROR;
    }

    FlashBlockDevice_RecordError(FLASH_BLOCK_DEVICE_OK);

    return FLASH_BLOCK_DEVICE_OK;
}

const FlashBlockDeviceInfo_t *FlashBlockDevice_GetInfo(void)
{
    return &g_flash_block_device_info;
}

const FlashBlockDeviceStats_t *FlashBlockDevice_GetStats(void)
{
    return &g_flash_block_device_stats;
}

void FlashBlockDevice_ResetStats(void)
{
    uint8_t initialized = g_flash_block_device_stats.initialized;
    uint32_t init_count = g_flash_block_device_stats.init_count;

    memset(&g_flash_block_device_stats, 0, sizeof(g_flash_block_device_stats));

    g_flash_block_device_stats.initialized = initialized;
    g_flash_block_device_stats.init_count = init_count;
}

void FlashBlockDevice_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("FlashBlockDevice Stats:\r\n");
    BoardLog_Info("  initialized             = %u\r\n", g_flash_block_device_stats.initialized);
    BoardLog_Info("  init_count              = %lu\r\n", g_flash_block_device_stats.init_count);
    BoardLog_Info("  read_count              = %lu\r\n", g_flash_block_device_stats.read_count);
    BoardLog_Info("  program_count           = %lu\r\n", g_flash_block_device_stats.program_count);
    BoardLog_Info("  erase_count             = %lu\r\n", g_flash_block_device_stats.erase_count);
    BoardLog_Info("  sync_count              = %lu\r\n", g_flash_block_device_stats.sync_count);
    BoardLog_Info("  verify_count            = %lu\r\n", g_flash_block_device_stats.verify_count);
    BoardLog_Info("  read_bytes              = %lu\r\n", g_flash_block_device_stats.read_bytes);
    BoardLog_Info("  program_bytes           = %lu\r\n", g_flash_block_device_stats.program_bytes);
    BoardLog_Info("  erase_bytes             = %lu\r\n", g_flash_block_device_stats.erase_bytes);
    BoardLog_Info("  split_program_count     = %lu\r\n", g_flash_block_device_stats.split_program_count);
    BoardLog_Info("  sector_erase_count      = %lu\r\n", g_flash_block_device_stats.sector_erase_count);
    BoardLog_Info("  block_32k_erase_count   = %lu\r\n", g_flash_block_device_stats.block_32k_erase_count);
    BoardLog_Info("  block_64k_erase_count   = %lu\r\n", g_flash_block_device_stats.block_64k_erase_count);
    BoardLog_Info("  invalid_param_count     = %lu\r\n", g_flash_block_device_stats.invalid_param_count);
    BoardLog_Info("  not_initialized_count   = %lu\r\n", g_flash_block_device_stats.not_initialized_count);
    BoardLog_Info("  addr_error_count        = %lu\r\n", g_flash_block_device_stats.addr_error_count);
    BoardLog_Info("  align_error_count       = %lu\r\n", g_flash_block_device_stats.align_error_count);
    BoardLog_Info("  bsp_error_count         = %lu\r\n", g_flash_block_device_stats.bsp_error_count);
    BoardLog_Info("  verify_error_count      = %lu\r\n", g_flash_block_device_stats.verify_error_count);
    BoardLog_Info("  last_addr               = 0x%06lX\r\n", g_flash_block_device_stats.last_addr);
    BoardLog_Info("  last_len                = %lu\r\n", g_flash_block_device_stats.last_len);
    BoardLog_Info("  last_error              = %d\r\n", g_flash_block_device_stats.last_error);
}
