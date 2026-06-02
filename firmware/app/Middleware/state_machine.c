#include "state_machine.h"

#include <stddef.h>

static const StateDef_t *StateMachine_FindStateDef(const StateMachine_t *sm,
                                                   StateId_t state)
{
    if ((sm == NULL) || (sm->state_table == NULL))
    {
        return NULL;
    }

    for (uint16_t i = 0U; i < sm->state_count; i++)
    {
        if (sm->state_table[i].state == state)
        {
            return &sm->state_table[i];
        }
    }

    return NULL;
}

int StateMachine_Init(StateMachine_t *sm,
                      const StateDef_t *state_table,
                      uint16_t state_count,
                      StateId_t init_state,
                      void *ctx,
                      uint32_t now_ms)
{
    const StateDef_t *init_def;

    if ((sm == NULL) || (state_table == NULL) || (state_count == 0U))
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    sm->current_state = STATE_MACHINE_INVALID_STATE;
    sm->previous_state = STATE_MACHINE_INVALID_STATE;
    sm->state_enter_time_ms = now_ms;
    sm->transition_count = 0U;
    sm->dispatch_count = 0U;
    sm->error_count = 0U;
    sm->state_table = state_table;
    sm->state_count = state_count;
    sm->ctx = ctx;

    init_def = StateMachine_FindStateDef(sm, init_state);
    if (init_def == NULL)
    {
        sm->error_count++;
        return STATE_MACHINE_STATE_NOT_FOUND;
    }

    sm->current_state = init_state;
    sm->previous_state = STATE_MACHINE_INVALID_STATE;
    sm->state_enter_time_ms = now_ms;

    if (init_def->on_enter != NULL)
    {
        init_def->on_enter(sm->ctx);
    }

    return STATE_MACHINE_OK;
}

int StateMachine_Transition(StateMachine_t *sm,
                            StateId_t next_state,
                            uint32_t now_ms)
{
    const StateDef_t *current_def;
    const StateDef_t *next_def;

    if (sm == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    next_def = StateMachine_FindStateDef(sm, next_state);
    if (next_def == NULL)
    {
        sm->error_count++;
        return STATE_MACHINE_STATE_NOT_FOUND;
    }

    if (sm->current_state == next_state)
    {
        return STATE_MACHINE_OK;
    }

    current_def = StateMachine_FindStateDef(sm, sm->current_state);

    if ((current_def != NULL) && (current_def->on_exit != NULL))
    {
        current_def->on_exit(sm->ctx);
    }

    sm->previous_state = sm->current_state;
    sm->current_state = next_state;
    sm->state_enter_time_ms = now_ms;
    sm->transition_count++;

    if (next_def->on_enter != NULL)
    {
        next_def->on_enter(sm->ctx);
    }

    return STATE_MACHINE_OK;
}

int StateMachine_Dispatch(StateMachine_t *sm,
                          EventId_t event,
                          const void *event_data)
{
    const StateDef_t *current_def;
    int ret;

    if (sm == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    sm->dispatch_count++;

    current_def = StateMachine_FindStateDef(sm, sm->current_state);
    if (current_def == NULL)
    {
        sm->error_count++;
        return STATE_MACHINE_STATE_NOT_FOUND;
    }

    if (current_def->on_event == NULL)
    {
        return STATE_MACHINE_OK;
    }

    ret = current_def->on_event(sm->ctx, event, event_data);

    if (ret != STATE_MACHINE_OK)
    {
        sm->error_count++;
    }

    return ret;
}

StateId_t StateMachine_GetState(const StateMachine_t *sm)
{
    if (sm == NULL)
    {
        return STATE_MACHINE_INVALID_STATE;
    }

    return sm->current_state;
}

StateId_t StateMachine_GetPreviousState(const StateMachine_t *sm)
{
    if (sm == NULL)
    {
        return STATE_MACHINE_INVALID_STATE;
    }

    return sm->previous_state;
}

uint32_t StateMachine_GetStateDurationMs(const StateMachine_t *sm,
                                         uint32_t now_ms)
{
    if (sm == NULL)
    {
        return 0U;
    }

    return now_ms - sm->state_enter_time_ms;
}

uint32_t StateMachine_GetTransitionCount(const StateMachine_t *sm)
{
    if (sm == NULL)
    {
        return 0U;
    }

    return sm->transition_count;
}

uint32_t StateMachine_GetDispatchCount(const StateMachine_t *sm)
{
    if (sm == NULL)
    {
        return 0U;
    }

    return sm->dispatch_count;
}

uint32_t StateMachine_GetErrorCount(const StateMachine_t *sm)
{
    if (sm == NULL)
    {
        return 0U;
    }

    return sm->error_count;
}