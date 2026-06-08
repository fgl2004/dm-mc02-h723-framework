#ifndef STORAGE_APP_H
#define STORAGE_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* StorageApp host protocol commands. Keep these in sync with PC constants.py. */
#define STORAGE_CMD_GET_INFO               0xA0U
#define STORAGE_CMD_GET_PARTITION_COUNT    0xA1U
#define STORAGE_CMD_GET_PARTITION_INFO     0xA2U
#define STORAGE_CMD_ERASE_PARTITION        0xA3U
#define STORAGE_CMD_WRITE_PARTITION_BEGIN  0xA4U
#define STORAGE_CMD_READ_PARTITION_BEGIN   0xA5U
#define STORAGE_CMD_GET_STATS              0xA6U
#define STORAGE_CMD_ABORT_TRANSFER         0xA7U

#define STORAGE_APP_STATUS_OK              0x00U
#define STORAGE_APP_STATUS_ERROR           0x01U
#define STORAGE_APP_STATUS_INVALID_PARAM   0x02U
#define STORAGE_APP_STATUS_DENIED          0x03U
#define STORAGE_APP_STATUS_BUSY            0x04U
#define STORAGE_APP_STATUS_STORAGE_ERROR   0x05U
#define STORAGE_APP_STATUS_NOT_FOUND       0x06U

void StorageApp_Init(void);
void StorageApp_Process(void);
int StorageApp_RegisterCommands(void);

/*
 * CommandService integration helper.
 * Input: command id and request payload from TYPE=REQ.
 * Output: response payload for TYPE=RESP. Response payload always starts with
 * one status byte. Caller owns sending RESP/NACK through the existing command path.
 */
int StorageApp_HandleCommand(uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len,
                             uint8_t *resp,
                             uint16_t *resp_len,
                             uint16_t resp_cap);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_APP_H */
