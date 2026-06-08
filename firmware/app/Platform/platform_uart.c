#include "platform_uart.h"

#include "main.h"
#include "usart.h"
#include "ring_buffer.h"

#include <stdio.h>
#include <string.h>

#define PLATFORM_UART_TX_TIMEOUT_MS        100U

#define PLATFORM_UART_DMA_RX_BUFFER_SIZE   256U
#define PLATFORM_UART_RX_RING_SIZE         1024U

static uint8_t g_uart_dma_rx_buf[PLATFORM_UART_DMA_RX_BUFFER_SIZE];
static uint8_t g_uart_rx_ring_mem[PLATFORM_UART_RX_RING_SIZE];
static RingBuffer_t g_uart_rx_ring;

static uint16_t g_uart_dma_last_pos = 0U;
static volatile uint8_t g_uart_tx_busy = 0U;
static PlatformUartStats_t g_uart_stats;

static int PlatformUart_ConvertHalStatus(HAL_StatusTypeDef status)
{
    switch (status)
    {
        case HAL_OK:
            return PLATFORM_UART_OK;

        case HAL_BUSY:
            return PLATFORM_UART_BUSY;

        case HAL_TIMEOUT:
            return PLATFORM_UART_TIMEOUT;

        case HAL_ERROR:
        default:
            return PLATFORM_UART_ERROR;
    }
}

void PlatformUart_Init(void)
{
    RingBuffer_Init(&g_uart_rx_ring,
                    g_uart_rx_ring_mem,
                    (uint16_t)sizeof(g_uart_rx_ring_mem));

    memset(&g_uart_stats, 0, sizeof(g_uart_stats));
    g_uart_dma_last_pos = 0U;
    g_uart_tx_busy = 0U;
}

/* -------------------------------------------------------------------------- */
/* Blocking TX APIs. Keep for printf / early bring-up only.                    */
/* -------------------------------------------------------------------------- */

int PlatformUart_SendByte(uint8_t byte)
{
    HAL_StatusTypeDef status;

    status = HAL_UART_Transmit(&huart1,
                               &byte,
                               1U,
                               PLATFORM_UART_TX_TIMEOUT_MS);

    if (status == HAL_OK)
    {
        g_uart_stats.tx_blocking_count++;
        g_uart_stats.tx_blocking_bytes++;
        g_uart_stats.tx_last_len = 1U;
        g_uart_stats.tx_last_error = PLATFORM_UART_OK;
    }
    else
    {
        g_uart_stats.tx_last_error = PlatformUart_ConvertHalStatus(status);
    }

    return PlatformUart_ConvertHalStatus(status);
}

int PlatformUart_SendBuffer(const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef status;

    if ((buf == NULL) || (len == 0U))
    {
        g_uart_stats.tx_last_error = PLATFORM_UART_INVALID_PARAM;
        return PLATFORM_UART_INVALID_PARAM;
    }

    status = HAL_UART_Transmit(&huart1,
                               (uint8_t *)buf,
                               len,
                               PLATFORM_UART_TX_TIMEOUT_MS);

    if (status == HAL_OK)
    {
        g_uart_stats.tx_blocking_count++;
        g_uart_stats.tx_blocking_bytes += len;
        g_uart_stats.tx_last_len = len;
        g_uart_stats.tx_last_error = PLATFORM_UART_OK;
    }
    else
    {
        g_uart_stats.tx_last_error = PlatformUart_ConvertHalStatus(status);
    }

    return PlatformUart_ConvertHalStatus(status);
}

int PlatformUart_SendString(const char *str)
{
    if (str == NULL)
    {
        return PLATFORM_UART_INVALID_PARAM;
    }

    return PlatformUart_SendBuffer((const uint8_t *)str,
                                   (uint16_t)strlen(str));
}

/* -------------------------------------------------------------------------- */
/* Non-blocking TX DMA primitive.                                              */
/* -------------------------------------------------------------------------- */

int PlatformUart_SendBufferDma(const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef status;

    if ((buf == NULL) || (len == 0U))
    {
        g_uart_stats.tx_last_error = PLATFORM_UART_INVALID_PARAM;
        return PLATFORM_UART_INVALID_PARAM;
    }

    if (g_uart_tx_busy != 0U)
    {
        g_uart_stats.tx_busy_count++;
        g_uart_stats.tx_last_error = PLATFORM_UART_BUSY;
        return PLATFORM_UART_BUSY;
    }

    /*
     * Only starts one complete DMA transfer.
     * No protocol semantics and no data chunking here.
     */
    g_uart_tx_busy = 1U;
    g_uart_stats.tx_busy = 1U;
    g_uart_stats.tx_last_len = len;

    status = HAL_UART_Transmit_DMA(&huart1, (uint8_t *)buf, len);
    if (status != HAL_OK)
    {
        g_uart_tx_busy = 0U;
        g_uart_stats.tx_busy = 0U;
        g_uart_stats.tx_dma_error_count++;
        g_uart_stats.tx_last_error = PlatformUart_ConvertHalStatus(status);
        return PlatformUart_ConvertHalStatus(status);
    }

    g_uart_stats.tx_dma_start_count++;
    g_uart_stats.tx_bytes += len;
    g_uart_stats.tx_last_error = PLATFORM_UART_OK;

    return PLATFORM_UART_OK;
}

uint8_t PlatformUart_IsTxBusy(void)
{
    return g_uart_tx_busy;
}

void PlatformUart_OnTxComplete(void)
{
    g_uart_tx_busy = 0U;
    g_uart_stats.tx_busy = 0U;
    g_uart_stats.tx_dma_done_count++;
    g_uart_stats.tx_last_error = PLATFORM_UART_OK;
}

void PlatformUart_OnTxError(void)
{
    g_uart_tx_busy = 0U;
    g_uart_stats.tx_busy = 0U;
    g_uart_stats.tx_dma_error_count++;
    g_uart_stats.tx_last_error = PLATFORM_UART_ERROR;
}

/* -------------------------------------------------------------------------- */
/* RX DMA circular receive.                                                    */
/* -------------------------------------------------------------------------- */

static void PlatformUart_MoveDmaDataToRing(uint16_t start_pos, uint16_t end_pos)
{
    uint16_t written;
    uint16_t len;

    if (start_pos == end_pos)
    {
        return;
    }

    if (end_pos > start_pos)
    {
        len = (uint16_t)(end_pos - start_pos);

        written = RingBuffer_Write(&g_uart_rx_ring,
                                   &g_uart_dma_rx_buf[start_pos],
                                   len);

        g_uart_stats.rx_bytes += written;

        if (written < len)
        {
            g_uart_stats.rx_ring_overflow++;
        }
    }
    else
    {
        len = (uint16_t)(PLATFORM_UART_DMA_RX_BUFFER_SIZE - start_pos);

        written = RingBuffer_Write(&g_uart_rx_ring,
                                   &g_uart_dma_rx_buf[start_pos],
                                   len);

        g_uart_stats.rx_bytes += written;

        if (written < len)
        {
            g_uart_stats.rx_ring_overflow++;
        }

        if (end_pos > 0U)
        {
            len = end_pos;

            written = RingBuffer_Write(&g_uart_rx_ring,
                                       &g_uart_dma_rx_buf[0],
                                       len);

            g_uart_stats.rx_bytes += written;

            if (written < len)
            {
                g_uart_stats.rx_ring_overflow++;
            }
        }
    }
}

static uint16_t PlatformUart_GetDmaCurrentPos(void)
{
    uint16_t pos;

    /*
     * NDTR means remaining transfer count.
     * Current DMA write position = buffer_size - NDTR.
     */
    pos = (uint16_t)(PLATFORM_UART_DMA_RX_BUFFER_SIZE -
                    __HAL_DMA_GET_COUNTER(huart1.hdmarx));

    if (pos >= PLATFORM_UART_DMA_RX_BUFFER_SIZE)
    {
        pos = 0U;
    }

    return pos;
}

static void PlatformUart_ProcessDmaToCurrentPos(void)
{
    uint16_t current_pos;

    current_pos = PlatformUart_GetDmaCurrentPos();

    PlatformUart_MoveDmaDataToRing(g_uart_dma_last_pos, current_pos);

    g_uart_dma_last_pos = current_pos;
}

int PlatformUart_StartRxDma(void)
{
    HAL_StatusTypeDef status;

    g_uart_dma_last_pos = 0U;

    status = HAL_UART_Receive_DMA(&huart1,
                                  g_uart_dma_rx_buf,
                                  PLATFORM_UART_DMA_RX_BUFFER_SIZE);

    if (status != HAL_OK)
    {
        g_uart_stats.rx_error_count++;
        return PlatformUart_ConvertHalStatus(status);
    }

    /*
     * Enable UART IDLE interrupt.
     * DMA half/full interrupts are generated by DMA callbacks.
     */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    g_uart_stats.rx_dma_start_count++;

    return PLATFORM_UART_OK;
}

void PlatformUart_OnRxHalfTransfer(void)
{
    g_uart_stats.rx_half_count++;
    PlatformUart_ProcessDmaToCurrentPos();
}

void PlatformUart_OnRxTransferComplete(void)
{
    g_uart_stats.rx_full_count++;
    PlatformUart_ProcessDmaToCurrentPos();
}

void PlatformUart_OnRxIdle(void)
{
    g_uart_stats.rx_idle_count++;
    PlatformUart_ProcessDmaToCurrentPos();
}

void PlatformUart_OnError(void)
{
    g_uart_stats.rx_error_count++;

    /*
     * Avoid TX scheduler stuck forever if UART error happens during TX DMA.
     */
    if (g_uart_tx_busy != 0U)
    {
        PlatformUart_OnTxError();
    }
}

uint16_t PlatformUart_ReadRx(uint8_t *buf, uint16_t len)
{
    return RingBuffer_Read(&g_uart_rx_ring, buf, len);
}

uint16_t PlatformUart_RxAvailable(void)
{
    return RingBuffer_Available(&g_uart_rx_ring);
}

/* -------------------------------------------------------------------------- */
/* Snapshots / stats.                                                          */
/* -------------------------------------------------------------------------- */

void PlatformUart_GetRxSnapshot(PlatformUartRxSnapshot_t *snapshot)
{
    const RingBufferStats_t *rb_stats;

    if (snapshot == NULL)
    {
        return;
    }

    rb_stats = RingBuffer_GetStats(&g_uart_rx_ring);

    snapshot->rx_dma_start_count = g_uart_stats.rx_dma_start_count;
    snapshot->rx_half_count = g_uart_stats.rx_half_count;
    snapshot->rx_full_count = g_uart_stats.rx_full_count;
    snapshot->rx_idle_count = g_uart_stats.rx_idle_count;
    snapshot->rx_bytes = g_uart_stats.rx_bytes;
    snapshot->rx_ring_overflow = g_uart_stats.rx_ring_overflow;
    snapshot->rx_error_count = g_uart_stats.rx_error_count;

    snapshot->rx_ring_available = RingBuffer_Available(&g_uart_rx_ring);
    snapshot->rx_ring_free = RingBuffer_Free(&g_uart_rx_ring);

    if (rb_stats != NULL)
    {
        snapshot->rb_write_bytes = rb_stats->write_bytes;
        snapshot->rb_read_bytes = rb_stats->read_bytes;
        snapshot->rb_overflow_count = rb_stats->overflow_count;
        snapshot->rb_high_watermark = rb_stats->high_watermark;
    }
    else
    {
        snapshot->rb_write_bytes = 0U;
        snapshot->rb_read_bytes = 0U;
        snapshot->rb_overflow_count = 0U;
        snapshot->rb_high_watermark = 0U;
    }
}

void PlatformUart_GetTxSnapshot(PlatformUartTxSnapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    snapshot->tx_blocking_count = g_uart_stats.tx_blocking_count;
    snapshot->tx_blocking_bytes = g_uart_stats.tx_blocking_bytes;
    snapshot->tx_dma_start_count = g_uart_stats.tx_dma_start_count;
    snapshot->tx_dma_done_count = g_uart_stats.tx_dma_done_count;
    snapshot->tx_dma_error_count = g_uart_stats.tx_dma_error_count;
    snapshot->tx_busy_count = g_uart_stats.tx_busy_count;
    snapshot->tx_bytes = g_uart_stats.tx_bytes;

    snapshot->tx_last_len = g_uart_stats.tx_last_len;
    snapshot->tx_last_error = g_uart_stats.tx_last_error;
    snapshot->tx_busy = g_uart_tx_busy;
}

const PlatformUartStats_t *PlatformUart_GetStats(void)
{
    g_uart_stats.tx_busy = g_uart_tx_busy;
    return &g_uart_stats;
}

void PlatformUart_PrintStats(void)
{
    const RingBufferStats_t *rb_stats;

    rb_stats = RingBuffer_GetStats(&g_uart_rx_ring);

    printf("----------------------------------------\r\n");
    printf(" Platform UART Stats:\r\n");

    printf("  RX:\r\n");
    printf("    rx_dma_start_count = %lu\r\n", g_uart_stats.rx_dma_start_count);
    printf("    rx_half_count      = %lu\r\n", g_uart_stats.rx_half_count);
    printf("    rx_full_count      = %lu\r\n", g_uart_stats.rx_full_count);
    printf("    rx_idle_count      = %lu\r\n", g_uart_stats.rx_idle_count);
    printf("    rx_bytes           = %lu\r\n", g_uart_stats.rx_bytes);
    printf("    rx_ring_overflow   = %lu\r\n", g_uart_stats.rx_ring_overflow);
    printf("    rx_error_count     = %lu\r\n", g_uart_stats.rx_error_count);

    if (rb_stats != NULL)
    {
        printf("    rb_write_bytes     = %lu\r\n", rb_stats->write_bytes);
        printf("    rb_read_bytes      = %lu\r\n", rb_stats->read_bytes);
        printf("    rb_overflow_count  = %lu\r\n", rb_stats->overflow_count);
        printf("    rb_high_watermark  = %u\r\n", rb_stats->high_watermark);
    }

    printf("  TX:\r\n");
    printf("    tx_blocking_count  = %lu\r\n", g_uart_stats.tx_blocking_count);
    printf("    tx_blocking_bytes  = %lu\r\n", g_uart_stats.tx_blocking_bytes);
    printf("    tx_dma_start_count = %lu\r\n", g_uart_stats.tx_dma_start_count);
    printf("    tx_dma_done_count  = %lu\r\n", g_uart_stats.tx_dma_done_count);
    printf("    tx_dma_error_count = %lu\r\n", g_uart_stats.tx_dma_error_count);
    printf("    tx_busy_count      = %lu\r\n", g_uart_stats.tx_busy_count);
    printf("    tx_bytes           = %lu\r\n", g_uart_stats.tx_bytes);
    printf("    tx_last_len        = %u\r\n", g_uart_stats.tx_last_len);
    printf("    tx_last_error      = %d\r\n", g_uart_stats.tx_last_error);
    printf("    tx_busy            = %u\r\n", g_uart_tx_busy);
}

/*
 * printf retarget.
 *
 * Keep printf on blocking TX for now.
 * After ProtocolManager TX queue is stable, logs that need non-blocking behavior
 * should be migrated to BoardLog / ProtocolManager queue instead of printf.
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    (void)PlatformUart_SendByte((uint8_t)ch);
    return ch;
}

int __io_putchar(int ch)
{
    (void)PlatformUart_SendByte((uint8_t)ch);
    return ch;
}

/* -------------------------------------------------------------------------- */
/* HAL callbacks.                                                              */
/* -------------------------------------------------------------------------- */

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        PlatformUart_OnRxHalfTransfer();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        PlatformUart_OnRxTransferComplete();
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        PlatformUart_OnTxComplete();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        PlatformUart_OnError();
    }
}
