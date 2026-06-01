#ifndef BOARD_LOG_H
#define BOARD_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void BoardLog_Init(void);

void BoardLog_PrintSeparator(void);
void BoardLog_PrintBootBanner(void);

void BoardLog_Info(const char *fmt, ...);
void BoardLog_Warn(const char *fmt, ...);
void BoardLog_Error(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_LOG_H */