#include "event_manager.h"

#include "ring_buffer.h"
#include "board_log.h"

#include <string.h>

typedef struct
{
    uint8_t initialized;

    EventManagerStats_t stats;

    RingBuffer_t event_rb;
    uint8_t event_storage[EVENT_MANAGER_EVENT_QUEUE_SIZE * sizeof(EventManagerRecord_t)];
} EventManagerContext_t;

static EventManagerContext_t g_event_manager;

static void EventManager_UpdateQueueSnapshot(void);

void EventManager_Init(void)
{
    memset(&g_event_manager, 0, sizeof(g_event_manager));

    RingBuffer_Init(&g_event_manager.event_rb,
                    g_event_manager.event_storage,
                    (uint16_t)sizeof(g_event_manager.event_storage));

    g_event_manager.initialized = 1U;
    g_event_manager.stats.init_count++;

    EventManager_UpdateQueueSnapshot();

    BoardLog_Info("EventManager init OK\r\n");
}

int EventManager_PostEvent(uint8_t event_id,
                           const uint8_t *payload,
                           uint16_t payload_len)
{
    return EventManager_PostEventEx(event_id,
                                    payload,
                                    payload_len,
                                    EVENT_MANAGER_PRIORITY_NORMAL);
}

int EventManager_PostEventEx(uint8_t event_id,
                             const uint8_t *payload,
                             uint16_t payload_len,
                             EventManagerPriority_t priority)
{
    EventManagerRecord_t record;
    uint16_t copy_len;
    uint16_t written;

    if (g_event_manager.initialized == 0U)
    {
        g_event_manager.stats.not_initialized_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_INVALID_STATE;
        return EVENT_MANAGER_NOT_INITIALIZED;
    }

    if ((payload_len > 0U) && (payload == 0))
    {
        g_event_manager.stats.invalid_param_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return EVENT_MANAGER_INVALID_PARAM;
    }

    if ((priority != EVENT_MANAGER_PRIORITY_NORMAL) &&
        (priority != EVENT_MANAGER_PRIORITY_HIGH))
    {
        g_event_manager.stats.invalid_param_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return EVENT_MANAGER_INVALID_PARAM;
    }

    if (RingBuffer_Free(&g_event_manager.event_rb) < (uint16_t)sizeof(EventManagerRecord_t))
    {
        g_event_manager.stats.event_drop_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_BUSY;
        EventManager_UpdateQueueSnapshot();
        return EVENT_MANAGER_QUEUE_FULL;
    }

    memset(&record, 0, sizeof(record));

    record.event_id = event_id;
    record.priority = (uint8_t)priority;

    copy_len = payload_len;
    if (copy_len > EVENT_MANAGER_MAX_EVENT_PAYLOAD_SIZE)
    {
        copy_len = EVENT_MANAGER_MAX_EVENT_PAYLOAD_SIZE;
    }

    if ((payload != 0) && (copy_len > 0U))
    {
        memcpy(record.payload, payload, copy_len);
        record.payload_len = copy_len;
    }

    written = RingBuffer_Write(&g_event_manager.event_rb,
                               (const uint8_t *)&record,
                               (uint16_t)sizeof(record));

    if (written != (uint16_t)sizeof(record))
    {
        g_event_manager.stats.event_drop_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_BUSY;
        EventManager_UpdateQueueSnapshot();
        return EVENT_MANAGER_QUEUE_FULL;
    }

    g_event_manager.stats.post_event_count++;
    g_event_manager.stats.last_event_id = event_id;
    g_event_manager.stats.last_priority = (uint8_t)priority;
    g_event_manager.stats.last_error = PROTO_ERROR_OK;

    if (priority == EVENT_MANAGER_PRIORITY_HIGH)
    {
        g_event_manager.stats.high_event_post_count++;
    }
    else
    {
        g_event_manager.stats.normal_event_post_count++;
    }

    EventManager_UpdateQueueSnapshot();

    return EVENT_MANAGER_OK;
}

int EventManager_TryGetPendingEvent(EventManagerRecord_t *event)
{
    uint16_t read_len;

    if (event == 0)
    {
        g_event_manager.stats.invalid_param_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_INVALID_PARAM;
        return EVENT_MANAGER_INVALID_PARAM;
    }

    if (g_event_manager.initialized == 0U)
    {
        g_event_manager.stats.not_initialized_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_INVALID_STATE;
        return EVENT_MANAGER_NOT_INITIALIZED;
    }

    if (RingBuffer_Available(&g_event_manager.event_rb) < (uint16_t)sizeof(EventManagerRecord_t))
    {
        EventManager_UpdateQueueSnapshot();
        return EVENT_MANAGER_NO_EVENT;
    }

    read_len = RingBuffer_Read(&g_event_manager.event_rb,
                               (uint8_t *)event,
                               (uint16_t)sizeof(*event));

    if (read_len != (uint16_t)sizeof(*event))
    {
        g_event_manager.stats.error_count++;
        g_event_manager.stats.last_error = PROTO_ERROR_INTERNAL_ERROR;
        EventManager_UpdateQueueSnapshot();
        return EVENT_MANAGER_ERROR;
    }

    g_event_manager.stats.event_pop_count++;
    g_event_manager.stats.last_event_id = event->event_id;
    g_event_manager.stats.last_priority = event->priority;
    g_event_manager.stats.last_error = PROTO_ERROR_OK;

    EventManager_UpdateQueueSnapshot();

    return EVENT_MANAGER_OK;
}

const EventManagerStats_t *EventManager_GetStats(void)
{
    EventManager_UpdateQueueSnapshot();
    return &g_event_manager.stats;
}

void EventManager_ResetStats(void)
{
    memset(&g_event_manager.stats, 0, sizeof(g_event_manager.stats));
    EventManager_UpdateQueueSnapshot();
}

void EventManager_PrintStats(void)
{
    const RingBufferStats_t *rb_stats;

    rb_stats = RingBuffer_GetStats(&g_event_manager.event_rb);

    EventManager_UpdateQueueSnapshot();

    BoardLog_PrintSeparator();

    BoardLog_Info("EventManager Stats:\r\n");
    BoardLog_Info("  initialized              = %u\r\n", g_event_manager.initialized);
    BoardLog_Info("  init_count               = %lu\r\n", g_event_manager.stats.init_count);
    BoardLog_Info("  post_event_count         = %lu\r\n", g_event_manager.stats.post_event_count);
    BoardLog_Info("  event_pop_count          = %lu\r\n", g_event_manager.stats.event_pop_count);
    BoardLog_Info("  event_drop_count         = %lu\r\n", g_event_manager.stats.event_drop_count);
    BoardLog_Info("  high_event_post_count    = %lu\r\n", g_event_manager.stats.high_event_post_count);
    BoardLog_Info("  normal_event_post_count  = %lu\r\n", g_event_manager.stats.normal_event_post_count);
    BoardLog_Info("  invalid_param_count      = %lu\r\n", g_event_manager.stats.invalid_param_count);
    BoardLog_Info("  not_initialized_count    = %lu\r\n", g_event_manager.stats.not_initialized_count);
    BoardLog_Info("  error_count              = %lu\r\n", g_event_manager.stats.error_count);
    BoardLog_Info("  last_event_id            = 0x%02X\r\n", g_event_manager.stats.last_event_id);
    BoardLog_Info("  last_priority            = %u\r\n", g_event_manager.stats.last_priority);
    BoardLog_Info("  last_error               = 0x%02X\r\n", g_event_manager.stats.last_error);
    BoardLog_Info("  event_available          = %lu\r\n", g_event_manager.stats.last_queue_available);
    BoardLog_Info("  event_free               = %lu\r\n", g_event_manager.stats.last_queue_free);

    if (rb_stats != 0)
    {
        BoardLog_Info("  event_rb_write_bytes     = %lu\r\n", rb_stats->write_bytes);
        BoardLog_Info("  event_rb_read_bytes      = %lu\r\n", rb_stats->read_bytes);
        BoardLog_Info("  event_rb_overflow        = %lu\r\n", rb_stats->overflow_count);
        BoardLog_Info("  event_rb_high            = %u\r\n", rb_stats->high_watermark);
    }
}

static void EventManager_UpdateQueueSnapshot(void)
{
    g_event_manager.stats.last_queue_available = RingBuffer_Available(&g_event_manager.event_rb);
    g_event_manager.stats.last_queue_free = RingBuffer_Free(&g_event_manager.event_rb);
}
