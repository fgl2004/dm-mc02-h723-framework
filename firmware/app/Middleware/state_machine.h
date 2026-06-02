#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint16_t StateId_t;
typedef uint16_t EventId_t;

#define STATE_MACHINE_INVALID_STATE    ((StateId_t)0xFFFFU)

typedef enum
{
    STATE_MACHINE_OK = 0,
    STATE_MACHINE_ERROR = -1,
    STATE_MACHINE_INVALID_PARAM = -2,
    STATE_MACHINE_STATE_NOT_FOUND = -3
} StateMachineResult_t;

typedef void (*StateEnterFunc_t)(void *ctx);
typedef void (*StateExitFunc_t)(void *ctx);
typedef int  (*StateEventFunc_t)(void *ctx, EventId_t event, const void *event_data);

typedef struct
{
    StateId_t state;
    StateEnterFunc_t on_enter;
    StateExitFunc_t on_exit;
    StateEventFunc_t on_event;
} StateDef_t;

typedef struct
{
    StateId_t current_state;
    StateId_t previous_state;

    uint32_t state_enter_time_ms;
    uint32_t transition_count;
    uint32_t dispatch_count;
    uint32_t error_count;

    const StateDef_t *state_table;
    uint16_t state_count;

    void *ctx;
} StateMachine_t;

int StateMachine_Init(StateMachine_t *sm,
                      const StateDef_t *state_table,
                      uint16_t state_count,
                      StateId_t init_state,
                      void *ctx,
                      uint32_t now_ms);

int StateMachine_Transition(StateMachine_t *sm,
                            StateId_t next_state,
                            uint32_t now_ms);

int StateMachine_Dispatch(StateMachine_t *sm,
                          EventId_t event,
                          const void *event_data);

StateId_t StateMachine_GetState(const StateMachine_t *sm);
StateId_t StateMachine_GetPreviousState(const StateMachine_t *sm);

uint32_t StateMachine_GetStateDurationMs(const StateMachine_t *sm,
                                         uint32_t now_ms);

uint32_t StateMachine_GetTransitionCount(const StateMachine_t *sm);
uint32_t StateMachine_GetDispatchCount(const StateMachine_t *sm);
uint32_t StateMachine_GetErrorCount(const StateMachine_t *sm);

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H */