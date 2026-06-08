#include "storage_app.h"

#include "storage_manager.h"
#include "storage_partition.h"
#include "block_transfer_session.h"
#include "board_log.h"
#include "command_service.h"
#include "protocol_frame.h"

#include <string.h>

#define STORAGE_APP_DEFAULT_HANDLE          0x31U
#define STORAGE_APP_MAX_NAME_LEN            31U

static int StorageApp_CommandHandler(const ProtocolFrame_t *req_frame,
                                     CommandManagerResponse_t *resp,
                                     void *ctx);

typedef struct
{
    uint8_t cmd;
    const char *name;
    uint32_t flags;
} StorageAppCommandEntry_t;

static const StorageAppCommandEntry_t g_storage_app_commands[] =
{
    { STORAGE_CMD_GET_INFO,              "STORAGE_GET_INFO",              CMD_FLAG_READ_ONLY },
    { STORAGE_CMD_GET_PARTITION_COUNT,   "STORAGE_GET_PARTITION_COUNT",   CMD_FLAG_READ_ONLY },
    { STORAGE_CMD_GET_PARTITION_INFO,    "STORAGE_GET_PARTITION_INFO",    CMD_FLAG_READ_ONLY },
    { STORAGE_CMD_ERASE_PARTITION,       "STORAGE_ERASE_PARTITION",       CMD_FLAG_WRITE | CMD_FLAG_DANGEROUS },
    { STORAGE_CMD_WRITE_PARTITION_BEGIN, "STORAGE_WRITE_PARTITION_BEGIN", CMD_FLAG_WRITE | CMD_FLAG_BULK_TRANSFER },
    { STORAGE_CMD_READ_PARTITION_BEGIN,  "STORAGE_READ_PARTITION_BEGIN",  CMD_FLAG_READ_ONLY | CMD_FLAG_BULK_TRANSFER },
    { STORAGE_CMD_GET_STATS,             "STORAGE_GET_STATS",             CMD_FLAG_READ_ONLY },
    { STORAGE_CMD_ABORT_TRANSFER,        "STORAGE_ABORT_TRANSFER",        CMD_FLAG_WRITE | CMD_FLAG_BULK_TRANSFER }
};


static uint32_t StorageApp_ReadLe32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void StorageApp_WriteLe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint8_t StorageApp_ErrorFromRet(int ret)
{
    if (ret == 0)
    {
        return STORAGE_APP_STATUS_OK;
    }

    if ((ret == STORAGE_MANAGER_INVALID_PARAM) ||
        (ret == STORAGE_MANAGER_RANGE_ERROR) ||
        (ret == STORAGE_MANAGER_ALIGN_ERROR))
    {
        return STORAGE_APP_STATUS_INVALID_PARAM;
    }

    if (ret == STORAGE_MANAGER_PERMISSION_ERROR)
    {
        return STORAGE_APP_STATUS_DENIED;
    }

    return STORAGE_APP_STATUS_STORAGE_ERROR;
}


int StorageApp_RegisterCommands(void)
{
    uint32_t i;
    int final_ret = 0;

    for (i = 0UL; i < (uint32_t)(sizeof(g_storage_app_commands) / sizeof(g_storage_app_commands[0])); i++)
    {
        int ret = CommandService_Register(g_storage_app_commands[i].cmd,
                                          CommandService_GetCategoryByCmd(g_storage_app_commands[i].cmd),
                                          g_storage_app_commands[i].flags,
                                          StorageApp_CommandHandler,
                                          0,
                                          g_storage_app_commands[i].name);
        if (ret != COMMAND_SERVICE_OK)
        {
            final_ret = -1;
            BoardLog_Error("StorageApp command register failed: cmd=0x%02X ret=%d\r\n",
                           g_storage_app_commands[i].cmd,
                           ret);
        }
    }

    return final_ret;
}

void StorageApp_Init(void)
{
    int ret;

    ret = StorageManager_Init();
    if (ret != STORAGE_MANAGER_OK)
    {
        BoardLog_Error("StorageApp: StorageManager_Init failed ret=%d\r\n", ret);
    }

    BlockTransferSession_Init();
    (void)StorageApp_RegisterCommands();
    BoardLog_Info("StorageApp init OK\r\n");
}

void StorageApp_Process(void)
{
    BlockTransferSession_Process();
}

static int StorageApp_HandleGetInfo(uint8_t *resp, uint16_t *resp_len, uint16_t resp_cap)
{
    StorageManagerInfo_t info;
    int ret;

    if ((resp == 0) || (resp_len == 0) || (resp_cap < 21U))
    {
        return -1;
    }

    ret = StorageManager_GetInfo(&info);
    resp[0] = StorageApp_ErrorFromRet(ret);

    StorageApp_WriteLe32(&resp[1], info.flash_capacity_bytes);
    StorageApp_WriteLe32(&resp[5], info.flash_read_size);
    StorageApp_WriteLe32(&resp[9], info.flash_program_size);
    StorageApp_WriteLe32(&resp[13], info.flash_erase_size);
    StorageApp_WriteLe32(&resp[17], info.partition_count);

    *resp_len = 21U;
    return 0;
}

static int StorageApp_HandleGetPartitionCount(uint8_t *resp, uint16_t *resp_len, uint16_t resp_cap)
{
    if ((resp == 0) || (resp_len == 0) || (resp_cap < 5U))
    {
        return -1;
    }

    resp[0] = STORAGE_APP_STATUS_OK;
    StorageApp_WriteLe32(&resp[1], StoragePartition_GetCount());
    *resp_len = 5U;
    return 0;
}

static int StorageApp_HandleGetPartitionInfo(const uint8_t *payload, uint16_t payload_len,
                                             uint8_t *resp, uint16_t *resp_len, uint16_t resp_cap)
{
    const StoragePartition_t *p;
    uint8_t id;
    uint8_t name_len;
    uint8_t access = 0U;

    if ((payload == 0) || (payload_len < 1U) || (resp == 0) || (resp_len == 0) || (resp_cap < 24U))
    {
        return -1;
    }

    id = payload[0];
    p = StoragePartition_Get((StoragePartitionId_t)id);
    if ((p == 0) || (StoragePartition_IsValid(p) == 0))
    {
        resp[0] = STORAGE_APP_STATUS_NOT_FOUND;
        *resp_len = 1U;
        return 0;
    }

    name_len = (uint8_t)strlen(p->name);
    if (name_len > STORAGE_APP_MAX_NAME_LEN)
    {
        name_len = STORAGE_APP_MAX_NAME_LEN;
    }

    if (resp_cap < (uint16_t)(24U + name_len))
    {
        return -1;
    }

    if (StorageManager_IsHostReadable((StoragePartitionId_t)id)) { access |= 0x01U; }
    if (StorageManager_IsHostWritable((StoragePartitionId_t)id)) { access |= 0x02U; }
    if (StorageManager_IsHostErasable((StoragePartitionId_t)id)) { access |= 0x04U; }

    resp[0] = STORAGE_APP_STATUS_OK;
    resp[1] = id;
    resp[2] = access;
    resp[3] = name_len;
    StorageApp_WriteLe32(&resp[4], p->start_addr);
    StorageApp_WriteLe32(&resp[8], p->size_bytes);
    StorageApp_WriteLe32(&resp[12], p->erase_size);
    StorageApp_WriteLe32(&resp[16], p->flags);
    StorageApp_WriteLe32(&resp[20], (uint32_t)StoragePartition_IsValid(p));
    memcpy(&resp[24], p->name, name_len);

    *resp_len = (uint16_t)(24U + name_len);
    return 0;
}

static int StorageApp_HandleErasePartition(const uint8_t *payload, uint16_t payload_len,
                                           uint8_t *resp, uint16_t *resp_len, uint16_t resp_cap)
{
    StoragePartitionId_t id;
    int ret;

    if ((payload == 0) || (payload_len < 1U) || (resp == 0) || (resp_len == 0) || (resp_cap < 2U))
    {
        return -1;
    }

    id = (StoragePartitionId_t)payload[0];
    if (StorageManager_IsHostErasable(id) == 0)
    {
        resp[0] = STORAGE_APP_STATUS_DENIED;
        resp[1] = payload[0];
        *resp_len = 2U;
        return 0;
    }

    ret = StorageManager_ErasePartition(id);
    resp[0] = StorageApp_ErrorFromRet(ret);
    resp[1] = payload[0];
    *resp_len = 2U;
    return 0;
}

static int StorageApp_HandleWriteBegin(const uint8_t *payload, uint16_t payload_len,
                                       uint8_t *resp, uint16_t *resp_len, uint16_t resp_cap)
{
    StoragePartitionId_t id;
    uint32_t offset;
    uint32_t total_size;
    uint32_t crc32;
    uint8_t handle;
    int ret;

    if ((payload == 0) || (payload_len < 14U) || (resp == 0) || (resp_len == 0) || (resp_cap < 3U))
    {
        return -1;
    }

    id = (StoragePartitionId_t)payload[0];
    offset = StorageApp_ReadLe32(&payload[1]);
    total_size = StorageApp_ReadLe32(&payload[5]);
    crc32 = StorageApp_ReadLe32(&payload[9]);
    handle = payload[13];
    if (handle == 0U)
    {
        handle = STORAGE_APP_DEFAULT_HANDLE;
    }

    ret = BlockTransferSession_StartUpload(id, offset, total_size, crc32, handle);
    resp[0] = (ret == BLOCK_TRANSFER_OK) ? STORAGE_APP_STATUS_OK : STORAGE_APP_STATUS_STORAGE_ERROR;
    resp[1] = (uint8_t)id;
    resp[2] = handle;
    *resp_len = 3U;
    return 0;
}

static int StorageApp_HandleReadBegin(const uint8_t *payload, uint16_t payload_len,
                                      uint8_t *resp, uint16_t *resp_len, uint16_t resp_cap)
{
    StoragePartitionId_t id;
    uint32_t offset;
    uint32_t total_size;
    uint8_t handle;
    int ret;

    if ((payload == 0) || (payload_len < 10U) || (resp == 0) || (resp_len == 0) || (resp_cap < 3U))
    {
        return -1;
    }

    id = (StoragePartitionId_t)payload[0];
    offset = StorageApp_ReadLe32(&payload[1]);
    total_size = StorageApp_ReadLe32(&payload[5]);
    handle = payload[9];
    if (handle == 0U)
    {
        handle = STORAGE_APP_DEFAULT_HANDLE;
    }

    ret = BlockTransferSession_StartDownload(id, offset, total_size, 0UL, handle);
    resp[0] = (ret == BLOCK_TRANSFER_OK) ? STORAGE_APP_STATUS_OK : STORAGE_APP_STATUS_STORAGE_ERROR;
    resp[1] = (uint8_t)id;
    resp[2] = handle;
    *resp_len = 3U;
    return 0;
}

static int StorageApp_HandleGetStats(uint8_t *resp, uint16_t *resp_len, uint16_t resp_cap)
{
    const StorageManagerStats_t *sm = StorageManager_GetStats();
    const BlockTransferSessionStats_t *bt = BlockTransferSession_GetStats();

    if ((resp == 0) || (resp_len == 0) || (resp_cap < 41U))
    {
        return -1;
    }

    resp[0] = STORAGE_APP_STATUS_OK;
    StorageApp_WriteLe32(&resp[1], sm->read_count);
    StorageApp_WriteLe32(&resp[5], sm->write_count);
    StorageApp_WriteLe32(&resp[9], sm->erase_count);
    StorageApp_WriteLe32(&resp[13], sm->read_bytes);
    StorageApp_WriteLe32(&resp[17], sm->write_bytes);
    StorageApp_WriteLe32(&resp[21], sm->erase_bytes);
    StorageApp_WriteLe32(&resp[25], bt->bytes_written);
    StorageApp_WriteLe32(&resp[29], bt->bytes_read);
    StorageApp_WriteLe32(&resp[33], (uint32_t)bt->state);
    StorageApp_WriteLe32(&resp[37], (uint32_t)bt->last_error);
    *resp_len = 41U;
    return 0;
}

int StorageApp_HandleCommand(uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len,
                             uint8_t *resp,
                             uint16_t *resp_len,
                             uint16_t resp_cap)
{
    if ((resp == 0) || (resp_len == 0) || (resp_cap == 0U))
    {
        return -1;
    }

    *resp_len = 0U;

    switch (cmd)
    {
        case STORAGE_CMD_GET_INFO:
            return StorageApp_HandleGetInfo(resp, resp_len, resp_cap);

        case STORAGE_CMD_GET_PARTITION_COUNT:
            return StorageApp_HandleGetPartitionCount(resp, resp_len, resp_cap);

        case STORAGE_CMD_GET_PARTITION_INFO:
            return StorageApp_HandleGetPartitionInfo(payload, payload_len, resp, resp_len, resp_cap);

        case STORAGE_CMD_ERASE_PARTITION:
            return StorageApp_HandleErasePartition(payload, payload_len, resp, resp_len, resp_cap);

        case STORAGE_CMD_WRITE_PARTITION_BEGIN:
            return StorageApp_HandleWriteBegin(payload, payload_len, resp, resp_len, resp_cap);

        case STORAGE_CMD_READ_PARTITION_BEGIN:
            return StorageApp_HandleReadBegin(payload, payload_len, resp, resp_len, resp_cap);

        case STORAGE_CMD_GET_STATS:
            return StorageApp_HandleGetStats(resp, resp_len, resp_cap);

        case STORAGE_CMD_ABORT_TRANSFER:
            BlockTransferSession_Abort(0x01U);
            resp[0] = STORAGE_APP_STATUS_OK;
            *resp_len = 1U;
            return 0;

        default:
            resp[0] = STORAGE_APP_STATUS_NOT_FOUND;
            *resp_len = 1U;
            return 0;
    }
}


static int StorageApp_CommandHandler(const ProtocolFrame_t *req_frame,
                                     CommandManagerResponse_t *resp,
                                     void *ctx)
{
    uint8_t out_payload[PROTO_FRAME_MAX_PAYLOAD_SIZE];
    uint16_t out_len = 0U;
    int ret;

    (void)ctx;

    if ((req_frame == 0) || (resp == 0))
    {
        return COMMAND_SERVICE_INVALID_PARAM;
    }

    ret = StorageApp_HandleCommand(req_frame->cmd,
                                   req_frame->payload,
                                   req_frame->payload_len,
                                   out_payload,
                                   &out_len,
                                   (uint16_t)sizeof(out_payload));

    if (ret != 0)
    {
        CommandService_SetNack(resp,
                               req_frame->cmd,
                               PROTO_ERROR_INTERNAL_ERROR);
        return COMMAND_SERVICE_ERROR;
    }

    CommandService_SetResp(resp,
                           req_frame->cmd,
                           out_payload,
                           out_len);

    return COMMAND_SERVICE_OK;
}
