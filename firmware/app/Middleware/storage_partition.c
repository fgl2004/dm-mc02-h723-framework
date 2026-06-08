#include "storage_partition.h"

#include "board_log.h"

#include <string.h>

/*
 * Current 8MB partition plan:
 *
 * 0x000000 - 0x00FFFF   64KB     Storage metadata / superblock
 * 0x010000 - 0x01FFFF   64KB     Parameter Slot A
 * 0x020000 - 0x02FFFF   64KB     Parameter Slot B
 * 0x030000 - 0x07FFFF   320KB    Fault / Reset / Trace records
 * 0x080000 - 0x0FFFFF   512KB    Persistent diagnostic logs
 * 0x100000 - 0x2FFFFF   2MB      IMU recorder
 * 0x300000 - 0x3FFFFF   1MB      LittleFS user files
 * 0x400000 - 0x7DFFFF   3.875MB  Firmware staging area
 * 0x7E0000 - 0x7FFFFF   128KB    Factory / reserved
 */

static const StoragePartition_t g_storage_partitions[STORAGE_PARTITION_COUNT] =
{
    {
        STORAGE_PARTITION_METADATA,
        "metadata",
        0x000000UL,
        0x010000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE
    },
    {
        STORAGE_PARTITION_PARAMETER_A,
        "parameter_a",
        0x010000UL,
        0x010000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE
    },
    {
        STORAGE_PARTITION_PARAMETER_B,
        "parameter_b",
        0x020000UL,
        0x010000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE
    },
    {
        STORAGE_PARTITION_FAULT_TRACE,
        "fault_trace",
        0x030000UL,
        0x050000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE
    },
    {
        STORAGE_PARTITION_DIAGNOSTIC_LOG,
        "diagnostic_log",
        0x080000UL,
        0x080000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE
    },
    {
        STORAGE_PARTITION_IMU_RECORDER,
        "imu_recorder",
        0x100000UL,
        0x200000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE
    },
    {
        STORAGE_PARTITION_LITTLEFS_USER,
        "littlefs_user",
        0x300000UL,
        0x100000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE |
        STORAGE_PARTITION_FLAG_FILESYSTEM
    },
    {
        STORAGE_PARTITION_FIRMWARE_STAGING,
        "firmware_staging",
        0x400000UL,
        0x3E0000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE |
        STORAGE_PARTITION_FLAG_BOOT_RELATED
    },
    {
        STORAGE_PARTITION_FACTORY_RESERVED,
        "factory_reserved",
        0x7E0000UL,
        0x020000UL,
        STORAGE_PARTITION_ERASE_SIZE,
        STORAGE_PARTITION_FLAG_READABLE |
        STORAGE_PARTITION_FLAG_WRITABLE |
        STORAGE_PARTITION_FLAG_ERASABLE |
        STORAGE_PARTITION_FLAG_FACTORY
    }
};

static uint8_t StoragePartition_IsRangeInFlash(uint32_t start_addr, uint32_t size_bytes)
{
    if (size_bytes == 0UL)
    {
        return 0U;
    }

    if (start_addr >= STORAGE_FLASH_END_ADDR)
    {
        return 0U;
    }

    if (size_bytes > (STORAGE_FLASH_END_ADDR - start_addr))
    {
        return 0U;
    }

    return 1U;
}

const StoragePartition_t *StoragePartition_Get(StoragePartitionId_t id)
{
    if ((uint32_t)id >= (uint32_t)STORAGE_PARTITION_COUNT)
    {
        return 0;
    }

    return &g_storage_partitions[(uint32_t)id];
}

const StoragePartition_t *StoragePartition_FindByName(const char *name)
{
    uint32_t i;

    if (name == 0)
    {
        return 0;
    }

    for (i = 0UL; i < (uint32_t)STORAGE_PARTITION_COUNT; i++)
    {
        if (strcmp(name, g_storage_partitions[i].name) == 0)
        {
            return &g_storage_partitions[i];
        }
    }

    return 0;
}

const StoragePartition_t *StoragePartition_FindByAddress(uint32_t addr)
{
    uint32_t i;

    for (i = 0UL; i < (uint32_t)STORAGE_PARTITION_COUNT; i++)
    {
        if (StoragePartition_Contains(&g_storage_partitions[i], addr, 1UL) == 1)
        {
            return &g_storage_partitions[i];
        }
    }

    return 0;
}

uint32_t StoragePartition_GetCount(void)
{
    return (uint32_t)STORAGE_PARTITION_COUNT;
}

int StoragePartition_IsValid(const StoragePartition_t *partition)
{
    if (partition == 0)
    {
        return 0;
    }

    if (partition->name == 0)
    {
        return 0;
    }

    if (StoragePartition_IsRangeInFlash(partition->start_addr, partition->size_bytes) == 0U)
    {
        return 0;
    }

    if ((partition->start_addr % partition->erase_size) != 0UL)
    {
        return 0;
    }

    if ((partition->size_bytes % partition->erase_size) != 0UL)
    {
        return 0;
    }

    return 1;
}

int StoragePartition_Contains(const StoragePartition_t *partition, uint32_t addr, uint32_t len)
{
    if (StoragePartition_IsValid(partition) == 0)
    {
        return 0;
    }

    if (len == 0UL)
    {
        return 0;
    }

    if (addr < partition->start_addr)
    {
        return 0;
    }

    if (addr >= (partition->start_addr + partition->size_bytes))
    {
        return 0;
    }

    if (len > ((partition->start_addr + partition->size_bytes) - addr))
    {
        return 0;
    }

    return 1;
}

int StoragePartition_Translate(const StoragePartition_t *partition,
                               uint32_t offset,
                               uint32_t len,
                               uint32_t *abs_addr)
{
    if ((StoragePartition_IsValid(partition) == 0) || (abs_addr == 0))
    {
        return -1;
    }

    if (len == 0UL)
    {
        return -1;
    }

    if (offset >= partition->size_bytes)
    {
        return -1;
    }

    if (len > (partition->size_bytes - offset))
    {
        return -1;
    }

    *abs_addr = partition->start_addr + offset;

    return 0;
}

void StoragePartition_PrintTable(void)
{
    uint32_t i;

    BoardLog_PrintSeparator();
    BoardLog_Info("Storage Partition Table:\r\n");

    for (i = 0UL; i < (uint32_t)STORAGE_PARTITION_COUNT; i++)
    {
        const StoragePartition_t *p = &g_storage_partitions[i];

        BoardLog_Info("  [%lu] %-18s start=0x%06lX size=%luKB end=0x%06lX flags=0x%08lX valid=%d\r\n",
                      (unsigned long)i,
                      p->name,
                      (unsigned long)p->start_addr,
                      (unsigned long)(p->size_bytes / 1024UL),
                      (unsigned long)(p->start_addr + p->size_bytes - 1UL),
                      (unsigned long)p->flags,
                      StoragePartition_IsValid(p));
    }
}
