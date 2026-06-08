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

    uint32_t tx_blocking_count;
    uint32_t tx_blocking_bytes;
    uint32_t tx_dma_start_count;
    uint32_t tx_dma_done_count;
    uint32_t tx_dma_error_count;
    uint32_t tx_busy_count;
    uint32_t tx_bytes;

    uint16_t tx_last_len;
    int tx_last_error;
    uint8_t tx_busy;
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

typedef struct
{
    uint32_t tx_blocking_count;
    uint32_t tx_blocking_bytes;
    uint32_t tx_dma_start_count;
    uint32_t tx_dma_done_count;
    uint32_t tx_dma_error_count;
    uint32_t tx_busy_count;
    uint32_t tx_bytes;

    uint16_t tx_last_len;
    int tx_last_error;
    uint8_t tx_busy;
} PlatformUartTxSnapshot_t;

void PlatformUart_Init(void);

/* Blocking TX APIs: only for bring-up and printf compatibility. */
int PlatformUart_SendByte(uint8_t byte);
int PlatformUart_SendBuffer(const uint8_t *buf, uint16_t len);
int PlatformUart_SendString(const char *str);

/*
 * Non-blocking TX DMA primitive.
 *
 * The buffer passed into PlatformUart_SendBufferDma() must remain valid until
 * HAL_UART_TxCpltCallback() calls PlatformUart_OnTxComplete().
 *
 * This layer only starts one DMA transfer. It does not split big data.
 * File / stream / bulk chunking belongs to upper managers.
 */
int PlatformUart_SendBufferDma(const uint8_t *buf, uint16_t len);
uint8_t PlatformUart_IsTxBusy(void);

void PlatformUart_GetRxSnapshot(PlatformUartRxSnapshot_t *snapshot);
void PlatformUart_GetTxSnapshot(PlatformUartTxSnapshot_t *snapshot);

int PlatformUart_StartRxDma(void);

uint16_t PlatformUart_ReadRx(uint8_t *buf, uint16_t len);
uint16_t PlatformUart_RxAvailable(void);

const PlatformUartStats_t *PlatformUart_GetStats(void);
void PlatformUart_PrintStats(void);

void PlatformUart_OnRxHalfTransfer(void);
void PlatformUart_OnRxTransferComplete(void);
void PlatformUart_OnRxIdle(void);

void PlatformUart_OnTxComplete(void);
void PlatformUart_OnTxError(void);

void PlatformUart_OnError(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_UART_H */
