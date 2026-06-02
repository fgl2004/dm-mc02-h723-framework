#ifndef PLATFORM_UART_H
#define PLATFORM_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    PLATFORM_UART_OK = 0,
    PLATFORM_UART_ERROR = -1,
    PLATFORM_UART_INVALID_PARAM = -2,
    PLATFORM_UART_TIMEOUT = -3,
    PLATFORM_UART_BUSY = -4
} PlatformUartResult_t;

typedef struct
{
    uint32_t rx_dma_start_count;
    uint32_t rx_half_count;
    uint32_t rx_full_count;
    uint32_t rx_idle_count;
    uint32_t rx_bytes;
    uint32_t rx_ring_overflow;
    uint32_t rx_error_count;
} PlatformUartStats_t;

typedef struct
{
    uint32_t rx_dma_start_count;
    uint32_t rx_half_count;
    uint32_t rx_full_count;
    uint32_t rx_idle_count;
    uint32_t rx_bytes;
    uint32_t rx_ring_overflow;
    uint32_t rx_error_count;

    uint16_t rx_ring_available;
    uint16_t rx_ring_free;

    uint32_t rb_write_bytes;
    uint32_t rb_read_bytes;
    uint32_t rb_overflow_count;
    uint16_t rb_high_watermark;
} PlatformUartRxSnapshot_t;

void PlatformUart_Init(void);

int PlatformUart_SendByte(uint8_t byte);
int PlatformUart_SendBuffer(const uint8_t *buf, uint16_t len);
int PlatformUart_SendString(const char *str);
/**
 * @brief Get UART RX runtime snapshot.
 *
 * This function copies UART DMA statistics and RX RingBuffer statistics
 * into a snapshot structure. It is mainly used for observability and
 * PC-side stress test visualization.
 *
 * @param snapshot Output snapshot pointer.
 */
void PlatformUart_GetRxSnapshot(PlatformUartRxSnapshot_t *snapshot);
/**
 * @brief Start UART RX DMA circular receive.
 */
int PlatformUart_StartRxDma(void);

/**
 * @brief Read bytes from UART RX ring buffer.
 */
uint16_t PlatformUart_ReadRx(uint8_t *buf, uint16_t len);

/**
 * @brief Get available bytes in UART RX ring buffer.
 */
uint16_t PlatformUart_RxAvailable(void);

/**
 * @brief Print UART RX statistics.
 */
void PlatformUart_PrintStats(void);

/**
 * @brief Get UART statistics.
 */
const PlatformUartStats_t *PlatformUart_GetStats(void);

/**
 * @brief UART event hooks called from HAL callbacks.
 */
void PlatformUart_OnRxHalfTransfer(void);
void PlatformUart_OnRxTransferComplete(void);
void PlatformUart_OnRxIdle(void);
void PlatformUart_OnError(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_UART_H */