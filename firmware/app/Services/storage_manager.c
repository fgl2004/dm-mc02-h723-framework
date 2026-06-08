#include "storage_manager.h"

#include "flash_block_device.h"
#include "board_log.h"

#include <string.h>

#define STORAGE_MANAGER_TEST_BUFFER_SIZE       512U

static StorageManagerStats_t g_storage_manager_stats;

static void StorageManager_RecordError(int error)
{
    g_storage_manager_stats.last_error = error;

    if (error == STORAGE_MANAGER_OK)
    {
        return;
    }

    if (error == STORAGE_MANAGER_INVALID_PARAM)
    {
        g_storage_manager_stats.invalid_param_count++;
    }
    else if (error == STORAGE_MANAGER_NOT_INITIALIZED)
    {
        g_storage_manager_stats.not_initialized_count++;
    }
    else if (error == STORAGE_MANAGER_PARTITION_ERROR)
    {
        g_storage_manager_stats.partition_error_count++;
    }
    else if (error == STORAGE_MANAGER_RANGE_ERROR)
    {
        g_storage_manager_stats.range_error_count++;
    }
    else if (error == STORAGE_MANAGER_ALIGN_ERROR)
    {
        g_storage_manager_stats.align_error_count++;
    }
    else if (error == STORAGE_MANAGER_BLOCK_ERROR)
    {
        g_storage_manager_stats.block_error_count++;
    }
    else if (error == STORAGE_MANAGER_PERMISSION_ERROR)
    {
        g_storage_manager_stats.permission_error_count++;
    }
    else if (error == STORAGE_MANAGER_VERIFY_ERROR)
    {
        g_storage_manager_stats.verify_error_count++;
    }
}

static uint8_t StorageManager_IsInitialized(void)
{
    return g_storage_manager_stats.initialized;
}

static int StorageManager_CheckPartitionPermission(const StoragePartition_t *partition,
                                                  uint32_t required_flag)
{
    if (partition == 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_PARTITION_ERROR);
        return STORAGE_MANAGER_PARTITION_ERROR;
    }

    if ((partition->flags & required_flag) == 0UL)
    {
        StorageManager_RecordError(STORAGE_MANAGER_PERMISSION_ERROR);
        return STORAGE_MANAGER_PERMISSION_ERROR;
    }

    return STORAGE_MANAGER_OK;
}

static int StorageManager_Translate(StoragePartitionId_t partition_id,
                                    uint32_t offset,
                                    uint32_t len,
                                    uint32_t required_flag,
                                    const StoragePartition_t **partition_out,
                                    uint32_t *abs_addr_out)
{
    const StoragePartition_t *partition;
    uint32_t abs_addr;
    int ret;

    if (StorageManager_IsInitialized() == 0U)
    {
        StorageManager_RecordError(STORAGE_MANAGER_NOT_INITIALIZED);
        return STORAGE_MANAGER_NOT_INITIALIZED;
    }

    if ((partition_out == 0) || (abs_addr_out == 0) || (len == 0UL))
    {
        StorageManager_RecordError(STORAGE_MANAGER_INVALID_PARAM);
        return STORAGE_MANAGER_INVALID_PARAM;
    }

    partition = StoragePartition_Get(partition_id);
    if ((partition == 0) || (StoragePartition_IsValid(partition) == 0))
    {
        StorageManager_RecordError(STORAGE_MANAGER_PARTITION_ERROR);
        return STORAGE_MANAGER_PARTITION_ERROR;
    }

    ret = StorageManager_CheckPartitionPermission(partition, required_flag);
    if (ret != STORAGE_MANAGER_OK)
    {
        return ret;
    }

    if (StoragePartition_Translate(partition, offset, len, &abs_addr) != 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_RANGE_ERROR);
        return STORAGE_MANAGER_RANGE_ERROR;
    }

    *partition_out = partition;
    *abs_addr_out = abs_addr;

    return STORAGE_MANAGER_OK;
}

int StorageManager_Init(void)
{
    const FlashBlockDeviceInfo_t *block_info;
    uint32_t i;
    int ret;

    memset(&g_storage_manager_stats, 0, sizeof(g_storage_manager_stats));
    g_storage_manager_stats.init_count++;
    g_storage_manager_stats.last_partition = STORAGE_PARTITION_COUNT;

    ret = FlashBlockDevice_Init();
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        StorageManager_RecordError(STORAGE_MANAGER_BLOCK_ERROR);
        return STORAGE_MANAGER_BLOCK_ERROR;
    }

    block_info = FlashBlockDevice_GetInfo();
    if (block_info == 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_BLOCK_ERROR);
        return STORAGE_MANAGER_BLOCK_ERROR;
    }

    for (i = 0UL; i < StoragePartition_GetCount(); i++)
    {
        const StoragePartition_t *partition = StoragePartition_Get((StoragePartitionId_t)i);

        if (StoragePartition_IsValid(partition) == 0)
        {
            StorageManager_RecordError(STORAGE_MANAGER_PARTITION_ERROR);
            return STORAGE_MANAGER_PARTITION_ERROR;
        }

        if (partition->erase_size != block_info->erase_size)
        {
            StorageManager_RecordError(STORAGE_MANAGER_PARTITION_ERROR);
            return STORAGE_MANAGER_PARTITION_ERROR;
        }

        if ((partition->start_addr + partition->size_bytes) > block_info->capacity_bytes)
        {
            StorageManager_RecordError(STORAGE_MANAGER_PARTITION_ERROR);
            return STORAGE_MANAGER_PARTITION_ERROR;
        }
    }

    g_storage_manager_stats.initialized = 1U;
    StorageManager_RecordError(STORAGE_MANAGER_OK);

    BoardLog_Info("StorageManager init OK: capacity=%lu, partitions=%lu\r\n",
                  (unsigned long)block_info->capacity_bytes,
                  (unsigned long)StoragePartition_GetCount());

    return STORAGE_MANAGER_OK;
}

int StorageManager_GetInfo(StorageManagerInfo_t *info)
{
    const FlashBlockDeviceInfo_t *block_info;

    if (StorageManager_IsInitialized() == 0U)
    {
        StorageManager_RecordError(STORAGE_MANAGER_NOT_INITIALIZED);
        return STORAGE_MANAGER_NOT_INITIALIZED;
    }

    if (info == 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_INVALID_PARAM);
        return STORAGE_MANAGER_INVALID_PARAM;
    }

    block_info = FlashBlockDevice_GetInfo();
    if (block_info == 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_BLOCK_ERROR);
        return STORAGE_MANAGER_BLOCK_ERROR;
    }

    info->flash_capacity_bytes = block_info->capacity_bytes;
    info->flash_read_size = block_info->read_size;
    info->flash_program_size = block_info->program_size;
    info->flash_erase_size = block_info->erase_size;
    info->partition_count = StoragePartition_GetCount();

    StorageManager_RecordError(STORAGE_MANAGER_OK);

    return STORAGE_MANAGER_OK;
}

int StorageManager_Read(StoragePartitionId_t partition_id,
                        uint32_t offset,
                        void *buf,
                        uint32_t len)
{
    const StoragePartition_t *partition;
    uint32_t abs_addr;
    int ret;

    if (buf == 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_INVALID_PARAM);
        return STORAGE_MANAGER_INVALID_PARAM;
    }

    ret = StorageManager_Translate(partition_id,
                                   offset,
                                   len,
                                   STORAGE_PARTITION_FLAG_READABLE,
                                   &partition,
                                   &abs_addr);
    if (ret != STORAGE_MANAGER_OK)
    {
        return ret;
    }

    ret = FlashBlockDevice_Read(abs_addr, buf, len);
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        StorageManager_RecordError(STORAGE_MANAGER_BLOCK_ERROR);
        return STORAGE_MANAGER_BLOCK_ERROR;
    }

    g_storage_manager_stats.read_count++;
    g_storage_manager_stats.read_bytes += len;
    g_storage_manager_stats.last_partition = partition->id;
    g_storage_manager_stats.last_offset = offset;
    g_storage_manager_stats.last_abs_addr = abs_addr;
    g_storage_manager_stats.last_len = len;
    StorageManager_RecordError(STORAGE_MANAGER_OK);

    return STORAGE_MANAGER_OK;
}

int StorageManager_Write(StoragePartitionId_t partition_id,
                         uint32_t offset,
                         const void *buf,
                         uint32_t len)
{
    const StoragePartition_t *partition;
    uint32_t abs_addr;
    int ret;

    if (buf == 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_INVALID_PARAM);
        return STORAGE_MANAGER_INVALID_PARAM;
    }

    ret = StorageManager_Translate(partition_id,
                                   offset,
                                   len,
                                   STORAGE_PARTITION_FLAG_WRITABLE,
                                   &partition,
                                   &abs_addr);
    if (ret != STORAGE_MANAGER_OK)
    {
        return ret;
    }

    ret = FlashBlockDevice_Program(abs_addr, buf, len);
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        StorageManager_RecordError(STORAGE_MANAGER_BLOCK_ERROR);
        return STORAGE_MANAGER_BLOCK_ERROR;
    }

    g_storage_manager_stats.write_count++;
    g_storage_manager_stats.write_bytes += len;
    g_storage_manager_stats.last_partition = partition->id;
    g_storage_manager_stats.last_offset = offset;
    g_storage_manager_stats.last_abs_addr = abs_addr;
    g_storage_manager_stats.last_len = len;
    StorageManager_RecordError(STORAGE_MANAGER_OK);

    return STORAGE_MANAGER_OK;
}

int StorageManager_Erase(StoragePartitionId_t partition_id,
                         uint32_t offset,
                         uint32_t len)
{
    const StoragePartition_t *partition;
    uint32_t abs_addr;
    int ret;

    ret = StorageManager_Translate(partition_id,
                                   offset,
                                   len,
                                   STORAGE_PARTITION_FLAG_ERASABLE,
                                   &partition,
                                   &abs_addr);
    if (ret != STORAGE_MANAGER_OK)
    {
        return ret;
    }

    if (((offset % partition->erase_size) != 0UL) ||
        ((len % partition->erase_size) != 0UL))
    {
        StorageManager_RecordError(STORAGE_MANAGER_ALIGN_ERROR);
        return STORAGE_MANAGER_ALIGN_ERROR;
    }

    ret = FlashBlockDevice_Erase(abs_addr, len);
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        StorageManager_RecordError(STORAGE_MANAGER_BLOCK_ERROR);
        return STORAGE_MANAGER_BLOCK_ERROR;
    }

    g_storage_manager_stats.erase_count++;
    g_storage_manager_stats.erase_bytes += len;
    g_storage_manager_stats.last_partition = partition->id;
    g_storage_manager_stats.last_offset = offset;
    g_storage_manager_stats.last_abs_addr = abs_addr;
    g_storage_manager_stats.last_len = len;
    StorageManager_RecordError(STORAGE_MANAGER_OK);

    return STORAGE_MANAGER_OK;
}

int StorageManager_ErasePartition(StoragePartitionId_t partition_id)
{
    const StoragePartition_t *partition;

    partition = StoragePartition_Get(partition_id);
    if ((partition == 0) || (StoragePartition_IsValid(partition) == 0))
    {
        StorageManager_RecordError(STORAGE_MANAGER_PARTITION_ERROR);
        return STORAGE_MANAGER_PARTITION_ERROR;
    }

    return StorageManager_Erase(partition_id, 0UL, partition->size_bytes);
}

int StorageManager_Sync(void)
{
    int ret;

    if (StorageManager_IsInitialized() == 0U)
    {
        StorageManager_RecordError(STORAGE_MANAGER_NOT_INITIALIZED);
        return STORAGE_MANAGER_NOT_INITIALIZED;
    }

    ret = FlashBlockDevice_Sync();
    if (ret != FLASH_BLOCK_DEVICE_OK)
    {
        StorageManager_RecordError(STORAGE_MANAGER_BLOCK_ERROR);
        return STORAGE_MANAGER_BLOCK_ERROR;
    }

    g_storage_manager_stats.sync_count++;
    StorageManager_RecordError(STORAGE_MANAGER_OK);

    return STORAGE_MANAGER_OK;
}


int StorageManager_IsErased(StoragePartitionId_t partition_id,
                            uint32_t offset,
                            uint32_t len,
                            uint8_t *is_erased)
{
    uint8_t buf[64];
    uint32_t pos = 0UL;
    int ret;

    if (is_erased == 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_INVALID_PARAM);
        return STORAGE_MANAGER_INVALID_PARAM;
    }

    if (len == 0UL)
    {
        StorageManager_RecordError(STORAGE_MANAGER_INVALID_PARAM);
        return STORAGE_MANAGER_INVALID_PARAM;
    }

    *is_erased = 0U;

    while (pos < len)
    {
        uint32_t chunk = len - pos;
        uint32_t i;

        if (chunk > (uint32_t)sizeof(buf))
        {
            chunk = (uint32_t)sizeof(buf);
        }

        ret = StorageManager_Read(partition_id, offset + pos, buf, chunk);
        if (ret != STORAGE_MANAGER_OK)
        {
            return ret;
        }

        for (i = 0UL; i < chunk; i++)
        {
            if (buf[i] != 0xFFU)
            {
                *is_erased = 0U;
                StorageManager_RecordError(STORAGE_MANAGER_OK);
                return STORAGE_MANAGER_OK;
            }
        }

        pos += chunk;
    }

    *is_erased = 1U;
    StorageManager_RecordError(STORAGE_MANAGER_OK);
    return STORAGE_MANAGER_OK;
}

int StorageManager_IsHostReadable(StoragePartitionId_t partition_id)
{
    const StoragePartition_t *partition = StoragePartition_Get(partition_id);

    if ((partition == 0) || (StoragePartition_IsValid(partition) == 0))
    {
        return 0;
    }

    return ((partition->flags & STORAGE_PARTITION_FLAG_READABLE) != 0UL) ? 1 : 0;
}

int StorageManager_IsHostWritable(StoragePartitionId_t partition_id)
{
    const StoragePartition_t *partition = StoragePartition_Get(partition_id);

    if ((partition == 0) || (StoragePartition_IsValid(partition) == 0))
    {
        return 0;
    }

    if ((partition->flags & STORAGE_PARTITION_FLAG_WRITABLE) == 0UL)
    {
        return 0;
    }

    /* Keep raw host writes away from boot / factory / metadata / parameter slots. */
    if ((partition->flags & (STORAGE_PARTITION_FLAG_BOOT_RELATED | STORAGE_PARTITION_FLAG_FACTORY)) != 0UL)
    {
        return 0;
    }

    if ((partition_id == STORAGE_PARTITION_METADATA) ||
        (partition_id == STORAGE_PARTITION_PARAMETER_A) ||
        (partition_id == STORAGE_PARTITION_PARAMETER_B))
    {
        return 0;
    }

    return 1;
}

int StorageManager_IsHostErasable(StoragePartitionId_t partition_id)
{
    const StoragePartition_t *partition = StoragePartition_Get(partition_id);

    if ((partition == 0) || (StoragePartition_IsValid(partition) == 0))
    {
        return 0;
    }

    if ((partition->flags & STORAGE_PARTITION_FLAG_ERASABLE) == 0UL)
    {
        return 0;
    }

    return StorageManager_IsHostWritable(partition_id);
}

int StorageManager_TestPartition(StoragePartitionId_t partition_id,
                                 uint32_t test_offset,
                                 uint32_t test_len)
{
    static uint8_t write_buf[STORAGE_MANAGER_TEST_BUFFER_SIZE];
    static uint8_t read_buf[STORAGE_MANAGER_TEST_BUFFER_SIZE];

    const StoragePartition_t *partition;
    uint32_t i;
    int ret;

    partition = StoragePartition_Get(partition_id);
    if ((partition == 0) || (StoragePartition_IsValid(partition) == 0))
    {
        StorageManager_RecordError(STORAGE_MANAGER_PARTITION_ERROR);
        return STORAGE_MANAGER_PARTITION_ERROR;
    }

    if ((test_len == 0UL) || (test_len > STORAGE_MANAGER_TEST_BUFFER_SIZE))
    {
        StorageManager_RecordError(STORAGE_MANAGER_INVALID_PARAM);
        return STORAGE_MANAGER_INVALID_PARAM;
    }

    if ((test_offset % partition->erase_size) != 0UL)
    {
        StorageManager_RecordError(STORAGE_MANAGER_ALIGN_ERROR);
        return STORAGE_MANAGER_ALIGN_ERROR;
    }

    if (test_len > partition->erase_size)
    {
        StorageManager_RecordError(STORAGE_MANAGER_RANGE_ERROR);
        return STORAGE_MANAGER_RANGE_ERROR;
    }

    if ((test_offset + partition->erase_size) > partition->size_bytes)
    {
        StorageManager_RecordError(STORAGE_MANAGER_RANGE_ERROR);
        return STORAGE_MANAGER_RANGE_ERROR;
    }

    for (i = 0UL; i < test_len; i++)
    {
        write_buf[i] = (uint8_t)(0xC3U ^ (uint8_t)i ^ (uint8_t)partition_id);
        read_buf[i] = 0U;
    }

    BoardLog_Info("[STORAGE_MANAGER_TEST] erase partition=%s offset=0x%06lX len=%lu\r\n",
                  partition->name,
                  (unsigned long)test_offset,
                  (unsigned long)partition->erase_size);

    ret = StorageManager_Erase(partition_id, test_offset, partition->erase_size);
    if (ret != STORAGE_MANAGER_OK)
    {
        return ret;
    }

    BoardLog_Info("[STORAGE_MANAGER_TEST] write partition=%s offset=0x%06lX len=%lu\r\n",
                  partition->name,
                  (unsigned long)test_offset,
                  (unsigned long)test_len);

    ret = StorageManager_Write(partition_id, test_offset, write_buf, test_len);
    if (ret != STORAGE_MANAGER_OK)
    {
        return ret;
    }

    BoardLog_Info("[STORAGE_MANAGER_TEST] read partition=%s offset=0x%06lX len=%lu\r\n",
                  partition->name,
                  (unsigned long)test_offset,
                  (unsigned long)test_len);

    ret = StorageManager_Read(partition_id, test_offset, read_buf, test_len);
    if (ret != STORAGE_MANAGER_OK)
    {
        return ret;
    }

    g_storage_manager_stats.verify_count++;

    if (memcmp(write_buf, read_buf, test_len) != 0)
    {
        StorageManager_RecordError(STORAGE_MANAGER_VERIFY_ERROR);
        return STORAGE_MANAGER_VERIFY_ERROR;
    }

    g_storage_manager_stats.test_count++;
    StorageManager_RecordError(STORAGE_MANAGER_OK);

    BoardLog_Info("[STORAGE_MANAGER_TEST] PASS partition=%s offset=0x%06lX len=%lu\r\n",
                  partition->name,
                  (unsigned long)test_offset,
                  (unsigned long)test_len);

    return STORAGE_MANAGER_OK;
}

const StorageManagerStats_t *StorageManager_GetStats(void)
{
    return &g_storage_manager_stats;
}

void StorageManager_ResetStats(void)
{
    uint8_t initialized = g_storage_manager_stats.initialized;
    uint32_t init_count = g_storage_manager_stats.init_count;

    memset(&g_storage_manager_stats, 0, sizeof(g_storage_manager_stats));

    g_storage_manager_stats.initialized = initialized;
    g_storage_manager_stats.init_count = init_count;
    g_storage_manager_stats.last_partition = STORAGE_PARTITION_COUNT;
}

void StorageManager_PrintInfo(void)
{
    StorageManagerInfo_t info;
    int ret;

    ret = StorageManager_GetInfo(&info);
    if (ret != STORAGE_MANAGER_OK)
    {
        BoardLog_Error("StorageManager_GetInfo failed, ret=%d\r\n", ret);
        return;
    }

    BoardLog_PrintSeparator();
    BoardLog_Info("StorageManager Info:\r\n");
    BoardLog_Info("  flash_capacity_bytes = %lu\r\n", (unsigned long)info.flash_capacity_bytes);
    BoardLog_Info("  flash_read_size      = %lu\r\n", (unsigned long)info.flash_read_size);
    BoardLog_Info("  flash_program_size   = %lu\r\n", (unsigned long)info.flash_program_size);
    BoardLog_Info("  flash_erase_size     = %lu\r\n", (unsigned long)info.flash_erase_size);
    BoardLog_Info("  partition_count      = %lu\r\n", (unsigned long)info.partition_count);
}

void StorageManager_PrintStats(void)
{
    BoardLog_PrintSeparator();
    BoardLog_Info("StorageManager Stats:\r\n");
    BoardLog_Info("  initialized              = %u\r\n", g_storage_manager_stats.initialized);
    BoardLog_Info("  init_count               = %lu\r\n", g_storage_manager_stats.init_count);
    BoardLog_Info("  read_count               = %lu\r\n", g_storage_manager_stats.read_count);
    BoardLog_Info("  write_count              = %lu\r\n", g_storage_manager_stats.write_count);
    BoardLog_Info("  erase_count              = %lu\r\n", g_storage_manager_stats.erase_count);
    BoardLog_Info("  sync_count               = %lu\r\n", g_storage_manager_stats.sync_count);
    BoardLog_Info("  test_count               = %lu\r\n", g_storage_manager_stats.test_count);
    BoardLog_Info("  verify_count             = %lu\r\n", g_storage_manager_stats.verify_count);
    BoardLog_Info("  read_bytes               = %lu\r\n", g_storage_manager_stats.read_bytes);
    BoardLog_Info("  write_bytes              = %lu\r\n", g_storage_manager_stats.write_bytes);
    BoardLog_Info("  erase_bytes              = %lu\r\n", g_storage_manager_stats.erase_bytes);
    BoardLog_Info("  invalid_param_count      = %lu\r\n", g_storage_manager_stats.invalid_param_count);
    BoardLog_Info("  not_initialized_count    = %lu\r\n", g_storage_manager_stats.not_initialized_count);
    BoardLog_Info("  partition_error_count    = %lu\r\n", g_storage_manager_stats.partition_error_count);
    BoardLog_Info("  range_error_count        = %lu\r\n", g_storage_manager_stats.range_error_count);
    BoardLog_Info("  align_error_count        = %lu\r\n", g_storage_manager_stats.align_error_count);
    BoardLog_Info("  block_error_count        = %lu\r\n", g_storage_manager_stats.block_error_count);
    BoardLog_Info("  permission_error_count   = %lu\r\n", g_storage_manager_stats.permission_error_count);
    BoardLog_Info("  verify_error_count       = %lu\r\n", g_storage_manager_stats.verify_error_count);
    BoardLog_Info("  last_partition           = %d\r\n", (int)g_storage_manager_stats.last_partition);
    BoardLog_Info("  last_offset              = 0x%06lX\r\n", g_storage_manager_stats.last_offset);
    BoardLog_Info("  last_abs_addr            = 0x%06lX\r\n", g_storage_manager_stats.last_abs_addr);
    BoardLog_Info("  last_len                 = %lu\r\n", g_storage_manager_stats.last_len);
    BoardLog_Info("  last_error               = %d\r\n", g_storage_manager_stats.last_error);
}
