#ifndef FLASH_BLOCK_DEVICE_H
#define FLASH_BLOCK_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Flash Block Device middleware layer.
 *
 * Position in Stage 5:
 *
 *   qspi_platform
 *     ↓
 *   bsp_w25q64jv
 *     ↓
 *   flash_block_device
 *     ↓
 *   storage_manager / littlefs adapter / storage_app
 *
 * Design rules:
 *   - This layer exposes a generic block-device style API.
 *   - This layer hides W25Q64JV page-program splitting.
 *   - This layer hides erase-size iteration.
 *   - This layer does not know file system semantics.
 *   - This layer does not register protocol commands.
 */

typedef enum
{
    FLASH_BLOCK_DEVICE_OK = 0,
    FLASH_BLOCK_DEVICE_ERROR = -1,
    FLASH_BLOCK_DEVICE_INVALID_PARAM = -2,
    FLASH_BLOCK_DEVICE_NOT_INITIALIZED = -3,
    FLASH_BLOCK_DEVICE_ADDR_ERROR = -4,
    FLASH_BLOCK_DEVICE_ALIGN_ERROR = -5,
    FLASH_BLOCK_DEVICE_BSP_ERROR = -6,
    FLASH_BLOCK_DEVICE_VERIFY_ERROR = -7
} FlashBlockDeviceResult_t;

typedef struct
{
    uint32_t capacity_bytes;
    uint32_t read_size;
    uint32_t program_size;
    uint32_t erase_size;
    uint32_t block_32k_size;
    uint32_t block_64k_size;
} FlashBlockDeviceInfo_t;

typedef struct
{
    uint32_t init_count;

    uint32_t read_count;
    uint32_t program_count;
    uint32_t erase_count;
    uint32_t sync_count;
    uint32_t verify_count;

    uint32_t read_bytes;
    uint32_t program_bytes;
    uint32_t erase_bytes;

    uint32_t split_program_count;
    uint32_t sector_erase_count;
    uint32_t block_32k_erase_count;
    uint32_t block_64k_erase_count;

    uint32_t invalid_param_count;
    uint32_t not_initialized_count;
    uint32_t addr_error_count;
    uint32_t align_error_count;
    uint32_t bsp_error_count;
    uint32_t verify_error_count;

    uint32_t last_addr;
    uint32_t last_len;
    int last_error;

    uint8_t initialized;
} FlashBlockDeviceStats_t;

int FlashBlockDevice_Init(void);

int FlashBlockDevice_Read(uint32_t addr, void *buf, uint32_t len);
int FlashBlockDevice_Program(uint32_t addr, const void *buf, uint32_t len);
int FlashBlockDevice_Erase(uint32_t addr, uint32_t len);
int FlashBlockDevice_Sync(void);

/*
 * Test helper.
 *
 * This function erases one sector at test_addr, writes test_len bytes,
 * reads back, and verifies the content.
 *
 * Constraints:
 *   - test_addr must be 4KB-sector aligned.
 *   - test_len must be > 0 and <= 4096 for the first version.
 *   - The selected sector will be erased.
 */
int FlashBlockDevice_WriteReadTest(uint32_t test_addr, uint32_t test_len);

const FlashBlockDeviceInfo_t *FlashBlockDevice_GetInfo(void);
const FlashBlockDeviceStats_t *FlashBlockDevice_GetStats(void);
void FlashBlockDevice_ResetStats(void);
void FlashBlockDevice_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_BLOCK_DEVICE_H */
