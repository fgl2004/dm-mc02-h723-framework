#ifndef APP_TEST_STREAM_BLOCK_H
#define APP_TEST_STREAM_BLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional test module for StreamManager + BlockManager.
 *
 * main.c:
 *   AppTestStreamBlock_Init();
 *   while (1)
 *   {
 *       AppTestStreamBlock_Run();
 *   }
 */

#ifndef ENABLE_APP_TEST_STREAM_BLOCK
#define ENABLE_APP_TEST_STREAM_BLOCK             0
#endif

#ifndef ENABLE_APP_TEST_STREAM_SAMPLE
#define ENABLE_APP_TEST_STREAM_SAMPLE            0
#endif

#ifndef ENABLE_APP_TEST_BLOCK_TRANSFER
#define ENABLE_APP_TEST_BLOCK_TRANSFER           0
#endif

#ifndef ENABLE_APP_TEST_STREAM_BLOCK_STATS
#define ENABLE_APP_TEST_STREAM_BLOCK_STATS       0
#endif

#ifndef APP_TEST_STREAM_PERIOD_MS
#define APP_TEST_STREAM_PERIOD_MS                20U
#endif

#ifndef APP_TEST_BLOCK_PERIOD_MS
#define APP_TEST_BLOCK_PERIOD_MS                 100U
#endif

#ifndef APP_TEST_STATS_PERIOD_MS
#define APP_TEST_STATS_PERIOD_MS                 2000U
#endif
#define   APP_TEST_STATS_PERIOD_MS              2000U
void AppTestStreamBlock_Init(void);
void AppTestStreamBlock_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TEST_STREAM_BLOCK_H */
