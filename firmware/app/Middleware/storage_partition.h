#ifndef STORAGE_PARTITION_H
#define STORAGE_PARTITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Storage partition table for W25Q64JV 8MB external Flash.
 *
 * Position:
 *   middleware/storage
 *
 * This file does not access hardware.
 * It only defines logical regions used later by StorageManager,
 * PersistentLog, IMU Recorder, LittleFS, and Bootloader staging.
 */

typedef enum
{
    STORAGE_PARTITION_METADATA = 0,
    STORAGE_PARTITION_PARAMETER_A,
    STORAGE_PARTITION_PARAMETER_B,
    STORAGE_PARTITION_FAULT_TRACE,
    STORAGE_PARTITION_DIAGNOSTIC_LOG,
    STORAGE_PARTITION_IMU_RECORDER,
    STORAGE_PARTITION_LITTLEFS_USER,
    STORAGE_PARTITION_FIRMWARE_STAGING,
    STORAGE_PARTITION_FACTORY_RESERVED,
    STORAGE_PARTITION_COUNT
} StoragePartitionId_t;

typedef struct
{
    StoragePartitionId_t id;
    const char *name;
    uint32_t start_addr;
    uint32_t size_bytes;
    uint32_t erase_size;
    uint32_t flags;
} StoragePartition_t;

#define STORAGE_PARTITION_FLAG_READABLE      (1UL << 0)
#define STORAGE_PARTITION_FLAG_WRITABLE      (1UL << 1)
#define STORAGE_PARTITION_FLAG_ERASABLE      (1UL << 2)
#define STORAGE_PARTITION_FLAG_FILESYSTEM    (1UL << 3)
#define STORAGE_PARTITION_FLAG_BOOT_RELATED  (1UL << 4)
#define STORAGE_PARTITION_FLAG_FACTORY       (1UL << 5)

#define STORAGE_FLASH_BASE_ADDR              0x000000UL
#define STORAGE_FLASH_TOTAL_SIZE             (8UL * 1024UL * 1024UL)
#define STORAGE_FLASH_END_ADDR               (STORAGE_FLASH_BASE_ADDR + STORAGE_FLASH_TOTAL_SIZE)

#define STORAGE_PARTITION_ERASE_SIZE         4096UL

const StoragePartition_t *StoragePartition_Get(StoragePartitionId_t id);
const StoragePartition_t *StoragePartition_FindByName(const char *name);
const StoragePartition_t *StoragePartition_FindByAddress(uint32_t addr);
uint32_t StoragePartition_GetCount(void);

int StoragePartition_IsValid(const StoragePartition_t *partition);
int StoragePartition_Contains(const StoragePartition_t *partition, uint32_t addr, uint32_t len);
int StoragePartition_Translate(const StoragePartition_t *partition,
                               uint32_t offset,
                               uint32_t len,
                               uint32_t *abs_addr);

void StoragePartition_PrintTable(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_PARTITION_H */
