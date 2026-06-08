#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "protocol_frame.h"

/*
 * EventManager
 *
 * Responsibility:
 *   - Own asynchronous MCU -> PC event queue.
 *   - Provide EventManager_PostEvent() for apps.
 *   - Provide EventManager_TryGetPendingEvent() for ProtocolManager.
 *
 * Design note:
 *   - EventManager does NOT build protocol frames.
 *   - EventManager does NOT send UART.
 *   - ProtocolManager is still the only module that builds frames and sends TX DMA.
 */

#ifndef EVENT_MANAGER_MAX_EVENT_PAYLOAD_SIZE
#define EVENT_MANAGER_MAX_EVENT_PAYLOAD_SIZE     96U
#endif

#ifndef EVENT_MANAGER_EVENT_QUEUE_SIZE
#define EVENT_MANAGER_EVENT_QUEUE_SIZE           8U
#endif

typedef enum
{
    EVENT_MANAGER_OK = 0,
    EVENT_MANAGER_ERROR = -1,
    EVENT_MANAGER_INVALID_PARAM = -2,
    EVENT_MANAGER_QUEUE_FULL = -3,
    EVENT_MANAGER_NO_EVENT = -4,
    EVENT_MANAGER_NOT_INITIALIZED = -5
} EventManagerResult_t;

typedef enum
{
    EVENT_MANAGER_PRIORITY_NORMAL = 0,
    EVENT_MANAGER_PRIORITY_HIGH = 1
} EventManagerPriority_t;

typedef struct
{
    uint8_t event_id;
    uint8_t priority;

    uint16_t payload_len;
    uint8_t payload[EVENT_MANAGER_MAX_EVENT_PAYLOAD_SIZE];
} EventManagerRecord_t;

typedef struct
{
    uint32_t init_count;

    uint32_t post_event_count;
    uint32_t event_pop_count;
    uint32_t event_drop_count;

    uint32_t invalid_param_count;
    uint32_t not_initialized_count;
    uint32_t error_count;

    uint32_t high_event_post_count;
    uint32_t normal_event_post_count;

    uint32_t last_queue_available;
    uint32_t last_queue_free;

    uint8_t last_event_id;
    uint8_t last_priority;
    uint8_t last_error;
} EventManagerStats_t;

void EventManager_Init(void);

int EventManager_PostEvent(uint8_t event_id,
                           const uint8_t *payload,
                           uint16_t payload_len);

int EventManager_PostEventEx(uint8_t event_id,
                             const uint8_t *payload,
                             uint16_t payload_len,
                             EventManagerPriority_t priority);

int EventManager_TryGetPendingEvent(EventManagerRecord_t *event);

const EventManagerStats_t *EventManager_GetStats(void);
void EventManager_ResetStats(void);
void EventManager_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_MANAGER_H */
