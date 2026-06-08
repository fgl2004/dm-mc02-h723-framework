#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "storage_partition.h"

/*
 * StorageManager middleware/service layer.
 *
 * Position in Stage 5:
 *
 *   qspi_platform
 *     ↓
 *   bsp_w25q64jv
 *     ↓
 *   flash_block_device
 *     ↓
 *   storage_partition
 *     ↓
 *   storage_manager
 *     ↓
 *   storage_app / parameter_manager / persistent_log / littlefs_adapter
 *
 * Design rules:
 *   - This layer manages partition-based storage access.
 *   - Upper layers should use partition + offset instead of absolute address.
 *   - This layer does not register protocol commands.
 *   - This layer does not implement file-system semantics yet.
 */

typedef enum
{
    STORAGE_MANAGER_OK = 0,
    STORAGE_MANAGER_ERROR = -1,
    STORAGE_MANAGER_INVALID_PARAM = -2,
    STORAGE_MANAGER_NOT_INITIALIZED = -3,
    STORAGE_MANAGER_PARTITION_ERROR = -4,
    STORAGE_MANAGER_RANGE_ERROR = -5,
    STORAGE_MANAGER_ALIGN_ERROR = -6,
    STORAGE_MANAGER_BLOCK_ERROR = -7,
    STORAGE_MANAGER_PERMISSION_ERROR = -8,
    STORAGE_MANAGER_VERIFY_ERROR = -9
} StorageManagerResult_t;

typedef struct
{
    uint32_t flash_capacity_bytes;
    uint32_t flash_read_size;
    uint32_t flash_program_size;
    uint32_t flash_erase_size;
    uint32_t partition_count;
} StorageManagerInfo_t;

typedef struct
{
    uint32_t init_count;

    uint32_t read_count;
    uint32_t write_count;
    uint32_t erase_count;
    uint32_t sync_count;
    uint32_t test_count;
    uint32_t verify_count;

    uint32_t read_bytes;
    uint32_t write_bytes;
    uint32_t erase_bytes;

    uint32_t invalid_param_count;
    uint32_t not_initialized_count;
    uint32_t partition_error_count;
    uint32_t range_error_count;
    uint32_t align_error_count;
    uint32_t block_error_count;
    uint32_t permission_error_count;
    uint32_t verify_error_count;

    StoragePartitionId_t last_partition;
    uint32_t last_offset;
    uint32_t last_abs_addr;
    uint32_t last_len;
    int last_error;

    uint8_t initialized;
} StorageManagerStats_t;

int StorageManager_Init(void);

int StorageManager_GetInfo(StorageManagerInfo_t *info);

int StorageManager_Read(StoragePartitionId_t partition_id,
                        uint32_t offset,
                        void *buf,
                        uint32_t len);

int StorageManager_Write(StoragePartitionId_t partition_id,
                         uint32_t offset,
                         const void *buf,
                         uint32_t len);

int StorageManager_Erase(StoragePartitionId_t partition_id,
                         uint32_t offset,
                         uint32_t len);

int StorageManager_ErasePartition(StoragePartitionId_t partition_id);

int StorageManager_Sync(void);

/*
 * Check whether a partition range is erased to 0xFF.
 * This is useful before raw program / OTA chunk write because NOR Flash can
 * program bits from 1 to 0 only.
 */
int StorageManager_IsErased(StoragePartitionId_t partition_id,
                            uint32_t offset,
                            uint32_t len,
                            uint8_t *is_erased);

/*
 * Host access policy helpers. These are stricter than low-level partition
 * READABLE / WRITABLE / ERASABLE flags. A partition may be writable by
 * firmware internally, but not safe for arbitrary PC raw access.
 */
int StorageManager_IsHostReadable(StoragePartitionId_t partition_id);
int StorageManager_IsHostWritable(StoragePartitionId_t partition_id);
int StorageManager_IsHostErasable(StoragePartitionId_t partition_id);

/*
 * Safe test helper.
 *
 * It erases the sector containing test_offset, writes test_len bytes,
 * reads back, and verifies.
 *
 * Constraints:
 *   - test_offset must be erase-size aligned.
 *   - test_len must be > 0 and <= 512 in this first version.
 *   - The selected sector will be erased.
 */
int StorageManager_TestPartition(StoragePartitionId_t partition_id,
                                 uint32_t test_offset,
                                 uint32_t test_len);

const StorageManagerStats_t *StorageManager_GetStats(void);
void StorageManager_ResetStats(void);
void StorageManager_PrintInfo(void);
void StorageManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_MANAGER_H */
