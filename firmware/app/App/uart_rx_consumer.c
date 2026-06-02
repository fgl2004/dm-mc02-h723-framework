#include "uart_rx_consumer.h"

#include "platform_uart.h"
#include "platform_time.h"

#include <stddef.h>
#include <stdint.h>

#ifndef UART_RX_CONSUMER_DEFAULT_MODE
#define UART_RX_CONSUMER_DEFAULT_MODE            UART_RX_CONSUMER_MODE_LIMITED
#endif

#ifndef UART_RX_CONSUMER_PERIOD_MS
#define UART_RX_CONSUMER_PERIOD_MS                10U
#endif

#ifndef UART_RX_CONSUMER_BUDGET_BYTES
#define UART_RX_CONSUMER_BUDGET_BYTES             32U
#endif

#ifndef UART_RX_CONSUMER_FAST_READ_CHUNK
#define UART_RX_CONSUMER_FAST_READ_CHUNK          128U
#endif

static UartRxConsumerStats_t g_uart_rx_consumer_stats;
static uint32_t g_last_limited_consume_ms = 0U;

static void UartRxConsumer_ProcessBytes(const uint8_t *data, uint16_t len);
static void UartRxConsumer_RunNone(void);
static void UartRxConsumer_RunFast(void);
static void UartRxConsumer_RunLimited(void);
static void UartRxConsumer_RunCounter(void);

void UartRxConsumer_Init(void)
{
    UartRxConsumer_ResetStats();

    g_uart_rx_consumer_stats.mode = UART_RX_CONSUMER_DEFAULT_MODE;
    g_uart_rx_consumer_stats.limited_period_ms = UART_RX_CONSUMER_PERIOD_MS;
    g_uart_rx_consumer_stats.limited_budget_bytes = UART_RX_CONSUMER_BUDGET_BYTES;

    g_last_limited_consume_ms = PlatformTime_GetMs();
}

void UartRxConsumer_Run(void)
{
    switch (g_uart_rx_consumer_stats.mode)
    {
        case UART_RX_CONSUMER_MODE_NONE:
            UartRxConsumer_RunNone();
            break;

        case UART_RX_CONSUMER_MODE_FAST:
            UartRxConsumer_RunFast();
            break;

        case UART_RX_CONSUMER_MODE_LIMITED:
            UartRxConsumer_RunLimited();
            break;

        case UART_RX_CONSUMER_MODE_COUNTER:
            UartRxConsumer_RunCounter();
            break;

        default:
            UartRxConsumer_RunFast();
            break;
    }
}

void UartRxConsumer_SetMode(uint32_t mode)
{
    if ((mode == UART_RX_CONSUMER_MODE_NONE) ||
        (mode == UART_RX_CONSUMER_MODE_FAST) ||
        (mode == UART_RX_CONSUMER_MODE_LIMITED) ||
        (mode == UART_RX_CONSUMER_MODE_COUNTER))
    {
        g_uart_rx_consumer_stats.mode = mode;
    }
}

uint32_t UartRxConsumer_GetMode(void)
{
    return g_uart_rx_consumer_stats.mode;
}

const UartRxConsumerStats_t *UartRxConsumer_GetStats(void)
{
    return &g_uart_rx_consumer_stats;
}

void UartRxConsumer_ResetStats(void)
{
    g_uart_rx_consumer_stats.mode = UART_RX_CONSUMER_DEFAULT_MODE;

    g_uart_rx_consumer_stats.read_bytes = 0U;
    g_uart_rx_consumer_stats.run_count = 0U;

    g_uart_rx_consumer_stats.mismatch_count = 0U;
    g_uart_rx_consumer_stats.estimated_drop_bytes = 0U;

    g_uart_rx_consumer_stats.expected_counter = 0U;
    g_uart_rx_consumer_stats.last_actual = 0U;
    g_uart_rx_consumer_stats.last_expected = 0U;

    g_uart_rx_consumer_stats.limited_period_ms = UART_RX_CONSUMER_PERIOD_MS;
    g_uart_rx_consumer_stats.limited_budget_bytes = UART_RX_CONSUMER_BUDGET_BYTES;
}

static void UartRxConsumer_RunNone(void)
{
    /*
     * Intentionally do nothing.
     *
     * This mode is used to verify:
     * 1. RX RingBuffer high watermark
     * 2. RingBuffer overflow statistics
     * 3. System robustness when upper-layer consumer stops
     */
    g_uart_rx_consumer_stats.run_count++;
}

static void UartRxConsumer_RunFast(void)
{
    uint8_t buf[UART_RX_CONSUMER_FAST_READ_CHUNK];
    uint16_t len;

    g_uart_rx_consumer_stats.run_count++;

    do
    {
        len = PlatformUart_ReadRx(buf, sizeof(buf));

        if (len > 0U)
        {
            g_uart_rx_consumer_stats.read_bytes += len;
        }
    } while (len > 0U);
}

static void UartRxConsumer_RunLimited(void)
{
    uint8_t buf[UART_RX_CONSUMER_BUDGET_BYTES];
    uint32_t now;
    uint16_t len;

    now = PlatformTime_GetMs();

    if ((now - g_last_limited_consume_ms) < UART_RX_CONSUMER_PERIOD_MS)
    {
        return;
    }

    g_last_limited_consume_ms = now;
    g_uart_rx_consumer_stats.run_count++;

    len = PlatformUart_ReadRx(buf, UART_RX_CONSUMER_BUDGET_BYTES);

    if (len > 0U)
    {
        g_uart_rx_consumer_stats.read_bytes += len;
    }
}

static void UartRxConsumer_RunCounter(void)
{
    uint8_t buf[UART_RX_CONSUMER_FAST_READ_CHUNK];
    uint16_t len;

    g_uart_rx_consumer_stats.run_count++;

    do
    {
        len = PlatformUart_ReadRx(buf, sizeof(buf));

        if (len > 0U)
        {
            UartRxConsumer_ProcessBytes(buf, len);
        }
    } while (len > 0U);
}

static void UartRxConsumer_ProcessBytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == NULL)
    {
        return;
    }

    for (i = 0U; i < len; i++)
    {
        uint8_t actual = data[i];
        uint8_t expected = g_uart_rx_consumer_stats.expected_counter;

        g_uart_rx_consumer_stats.read_bytes++;

        if (actual != expected)
        {
            uint8_t diff;

            g_uart_rx_consumer_stats.mismatch_count++;

            /*
             * For 8-bit counter stream:
             * expected = next expected byte
             * actual   = received byte
             *
             * The difference gives an estimated lost-byte count modulo 256.
             * This is not a perfect absolute loss counter, but it is enough
             * for UART RX path stress testing before protocol sequence number
             * is introduced.
             */
            diff = (uint8_t)(actual - expected);

            if (diff > 0U)
            {
                g_uart_rx_consumer_stats.estimated_drop_bytes += diff;
            }

            g_uart_rx_consumer_stats.last_expected = expected;
            g_uart_rx_consumer_stats.last_actual = actual;

            g_uart_rx_consumer_stats.expected_counter = (uint8_t)(actual + 1U);
        }
        else
        {
            g_uart_rx_consumer_stats.expected_counter = (uint8_t)(expected + 1U);
        }
    }
}