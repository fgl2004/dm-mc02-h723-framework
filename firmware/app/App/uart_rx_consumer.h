#ifndef UART_RX_CONSUMER_H
#define UART_RX_CONSUMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define UART_RX_CONSUMER_MODE_NONE        0U
#define UART_RX_CONSUMER_MODE_FAST        1U
#define UART_RX_CONSUMER_MODE_LIMITED     2U
#define UART_RX_CONSUMER_MODE_COUNTER     3U

typedef struct
{
    uint32_t mode;

    uint32_t read_bytes;
    uint32_t run_count;

    uint32_t mismatch_count;
    uint32_t estimated_drop_bytes;

    uint8_t expected_counter;
    uint8_t last_actual;
    uint8_t last_expected;

    uint32_t limited_period_ms;
    uint32_t limited_budget_bytes;
} UartRxConsumerStats_t;

void UartRxConsumer_Init(void);
void UartRxConsumer_Run(void);

void UartRxConsumer_SetMode(uint32_t mode);
uint32_t UartRxConsumer_GetMode(void);

const UartRxConsumerStats_t *UartRxConsumer_GetStats(void);
void UartRxConsumer_ResetStats(void);

#ifdef __cplusplus
}
#endif

#endif