#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "protocol_frame.h"

#define COMMAND_MANAGER_MAX_EVENT_PAYLOAD_SIZE    96U
#define COMMAND_MANAGER_EVENT_QUEUE_SIZE          8U

typedef enum
{
    COMMAND_MANAGER_OK = 0,
    COMMAND_MANAGER_ERROR = -1,
    COMMAND_MANAGER_INVALID_PARAM = -2,
    COMMAND_MANAGER_UNKNOWN_CMD = -3,
    COMMAND_MANAGER_QUEUE_FULL = -4,
    COMMAND_MANAGER_NO_EVENT = -5
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
    uint8_t event_id;
    uint16_t payload_len;
    uint8_t payload[COMMAND_MANAGER_MAX_EVENT_PAYLOAD_SIZE];
} CommandManagerEventRecord_t;

typedef struct
{
    uint32_t init_count;
    uint32_t dispatch_count;

    uint32_t routed_to_command_service_count;

    uint32_t post_event_count;
    uint32_t event_pop_count;
    uint32_t event_drop_count;

    uint32_t unknown_cmd_count;
    uint32_t invalid_param_count;
    uint32_t error_count;

    uint8_t last_cmd;
    uint8_t last_event_id;
    uint8_t last_error;
} CommandManagerStats_t;

void CommandManager_Init(void);

int CommandManager_Dispatch(const ProtocolFrame_t *req_frame,
                            CommandManagerResponse_t *resp);

int CommandManager_PostEvent(uint8_t event_id,
                             const uint8_t *payload,
                             uint16_t payload_len);

int CommandManager_TryGetPendingEvent(CommandManagerEventRecord_t *event);

const CommandManagerStats_t *CommandManager_GetStats(void);
void CommandManager_ResetStats(void);
void CommandManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_MANAGER_H */
