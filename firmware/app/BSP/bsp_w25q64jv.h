#ifndef BSP_W25Q64JV_H
#define BSP_W25Q64JV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * W25Q64JV BSP layer.
 *
 * This layer owns W25Q64JV command/register/page/sector semantics.
 * It must not know CommandService, StorageApp, FileSystem, or PC protocol.
 */

#define BSP_W25Q64_CAPACITY_BYTES        (8U * 1024U * 1024U)
#define BSP_W25Q64_PAGE_SIZE             256U
#define BSP_W25Q64_SECTOR_SIZE           4096U
#define BSP_W25Q64_BLOCK_32K_SIZE        (32U * 1024U)
#define BSP_W25Q64_BLOCK_64K_SIZE        (64U * 1024U)

typedef enum
{
    BSP_W25Q64_OK = 0,
    BSP_W25Q64_ERROR = -1,
    BSP_W25Q64_INVALID_PARAM = -2,
    BSP_W25Q64_QSPI_ERROR = -3,
    BSP_W25Q64_TIMEOUT = -4,
    BSP_W25Q64_ID_ERROR = -5,
    BSP_W25Q64_NOT_INITIALIZED = -6,
    BSP_W25Q64_ADDR_ERROR = -7,
    BSP_W25Q64_PAGE_BOUNDARY_ERROR = -8,
    BSP_W25Q64_VERIFY_ERROR = -9
} BspW25q64Result_t;

typedef struct
{
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
} BspW25q64JedecId_t;

typedef struct
{
    uint8_t bytes[8];
} BspW25q64UniqueId_t;

typedef struct
{
    uint32_t capacity_bytes;
    uint32_t page_size;
    uint32_t sector_size;
    uint32_t block_32k_size;
    uint32_t block_64k_size;
} BspW25q64Info_t;

typedef struct
{
    uint32_t init_count;
    uint32_t read_id_count;
    uint32_t read_uid_count;
    uint32_t read_status_count;
    uint32_t write_enable_count;
    uint32_t wait_ready_count;
    uint32_t read_count;
    uint32_t page_program_count;
    uint32_t sector_erase_count;
    uint32_t block_erase_count;
    uint32_t chip_erase_count;

    uint32_t qspi_error_count;
    uint32_t timeout_count;
    uint32_t id_error_count;
    uint32_t invalid_param_count;
    uint32_t addr_error_count;
    uint32_t page_boundary_error_count;
    uint32_t verify_error_count;

    uint32_t read_bytes;
    uint32_t program_bytes;
    uint32_t erase_bytes;

    uint32_t last_addr;
    uint32_t last_len;
    uint8_t last_status1;
    uint8_t last_status2;
    uint8_t last_status3;

    uint8_t initialized;
    uint8_t last_manufacturer_id;
    uint8_t last_memory_type;
    uint8_t last_capacity_id;

    int last_error;
} BspW25q64Stats_t;

int BspW25q64_Init(void);

int BspW25q64_ReadJedecId(BspW25q64JedecId_t *id);
int BspW25q64_ReadUniqueId(BspW25q64UniqueId_t *uid);

int BspW25q64_ReadStatus1(uint8_t *status);
int BspW25q64_ReadStatus2(uint8_t *status);
int BspW25q64_ReadStatus3(uint8_t *status);

int BspW25q64_WriteEnable(void);
int BspW25q64_IsBusy(uint8_t *busy);
int BspW25q64_WaitReady(uint32_t timeout_ms);

int BspW25q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/*
 * PageProgram must not cross 256-byte page boundary.
 * Higher block-device layer can split larger writes.
 */
int BspW25q64_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len);

int BspW25q64_SectorErase(uint32_t addr);
int BspW25q64_BlockErase32K(uint32_t addr);
int BspW25q64_BlockErase64K(uint32_t addr);
int BspW25q64_ChipErase(void);

int BspW25q64_WriteReadTest(uint32_t test_addr);

const BspW25q64Info_t *BspW25q64_GetInfo(void);
const BspW25q64Stats_t *BspW25q64_GetStats(void);
void BspW25q64_ResetStats(void);
void BspW25q64_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_W25Q64JV_H */
