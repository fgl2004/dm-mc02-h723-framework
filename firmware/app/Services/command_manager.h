#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "protocol_frame.h"

/*
 * CommandManager
 *
 * Responsibility:
 *   - Dispatch TYPE=REQ command frames to CommandService.
 *   - Generate CommandManagerResponse_t for ProtocolManager.
 *
 * Important:
 *   - Event queue has been moved to EventManager.
 *   - CommandManager no longer owns PostEvent / TryGetPendingEvent.
 */

typedef enum
{
    COMMAND_MANAGER_OK = 0,
    COMMAND_MANAGER_ERROR = -1,
    COMMAND_MANAGER_INVALID_PARAM = -2,
    COMMAND_MANAGER_UNKNOWN_CMD = -3
} CommandManagerResult_t;

typedef struct
{
    uint8_t frame_type;
    uint8_t cmd;
    uint8_t error_code;

    uint16_t payload_len;
    uint8_t payload[PROTO_FRAME_MAX_PAYLOAD_SIZE];
} CommandManagerResponse_t;

typedef struct
{
    uint32_t init_count;
    uint32_t dispatch_count;

    uint32_t routed_to_command_service_count;

    uint32_t unknown_cmd_count;
    uint32_t invalid_param_count;
    uint32_t error_count;

    uint32_t last_dispatch_us;
    uint32_t max_dispatch_us;

    uint8_t last_cmd;
    uint8_t last_error;
} CommandManagerStats_t;

void CommandManager_Init(void);

int CommandManager_Dispatch(const ProtocolFrame_t *req_frame,
                            CommandManagerResponse_t *resp);

const CommandManagerStats_t *CommandManager_GetStats(void);
void CommandManager_ResetStats(void);
void CommandManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_MANAGER_H */
