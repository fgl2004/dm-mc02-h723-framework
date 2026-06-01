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

void PlatformUart_Init(void);

int PlatformUart_SendByte(uint8_t byte);
int PlatformUart_SendBuffer(const uint8_t *buf, uint16_t len);
int PlatformUart_SendString(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_UART_H */