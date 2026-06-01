#include "board_log.h"

#include <stdio.h>
#include <stdarg.h>

#define BOARD_LOG_BOARD_NAME      "DM-MC02"
#define BOARD_LOG_MCU_NAME        "STM32H723VGT6"
#define BOARD_LOG_PROJECT_NAME    "DM-MC02 H723 Embedded Framework"
#define BOARD_LOG_STAGE_NAME      "Stage 1: Board Bring-up"
#define BOARD_LOG_CONSOLE_NAME    "USART1 115200 8N1"

void BoardLog_Init(void)
{
    /*
     * printf is retargeted by platform_uart.
     * This function is reserved for future log-level or output backend config.
     */
}

void BoardLog_PrintSeparator(void)
{
    printf("----------------------------------------\r\n");
}

void BoardLog_PrintBootBanner(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf(" %s\r\n", BOARD_LOG_PROJECT_NAME);
    printf(" %s\r\n", BOARD_LOG_STAGE_NAME);
    printf(" Board: %s\r\n", BOARD_LOG_BOARD_NAME);
    printf(" MCU: %s\r\n", BOARD_LOG_MCU_NAME);
    printf(" Console: %s\r\n", BOARD_LOG_CONSOLE_NAME);
    printf(" Build: %s %s\r\n", __DATE__, __TIME__);
    printf("========================================\r\n");
}

static void BoardLog_VPrintf(const char *level, const char *fmt, va_list args)
{
    printf("[%s] ", level);
    vprintf(fmt, args);
}

void BoardLog_Info(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    BoardLog_VPrintf("INFO", fmt, args);
    va_end(args);
}

void BoardLog_Warn(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    BoardLog_VPrintf("WARN", fmt, args);
    va_end(args);
}

void BoardLog_Error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    BoardLog_VPrintf("ERROR", fmt, args);
    va_end(args);
}