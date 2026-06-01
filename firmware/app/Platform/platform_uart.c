#include "platform_uart.h"

#include "main.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

#define PLATFORM_UART_TX_TIMEOUT_MS    100U

void PlatformUart_Init(void)
{
    /*
     * USART1 is initialized by CubeMX in MX_USART1_UART_Init().
     * This function is reserved for future platform-level UART state.
     */
}

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

int PlatformUart_SendByte(uint8_t byte)
{
    HAL_StatusTypeDef status;

    status = HAL_UART_Transmit(&huart1,
                               &byte,
                               1U,
                               PLATFORM_UART_TX_TIMEOUT_MS);

    return PlatformUart_ConvertHalStatus(status);
}

int PlatformUart_SendBuffer(const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef status;

    if ((buf == NULL) || (len == 0U))
    {
        return PLATFORM_UART_INVALID_PARAM;
    }

    status = HAL_UART_Transmit(&huart1,
                               (uint8_t *)buf,
                               len,
                               PLATFORM_UART_TX_TIMEOUT_MS);

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

/*
 * printf retarget.
 *
 * Keil ARM Compiler 6 may behave like GCC-compatible toolchain,
 * so both fputc() and __io_putchar() are provided.
 */
int fputc(int ch, FILE *f)
{
    (void)f;

    PlatformUart_SendByte((uint8_t)ch);

    return ch;
}

int __io_putchar(int ch)
{
    PlatformUart_SendByte((uint8_t)ch);

    return ch;
}