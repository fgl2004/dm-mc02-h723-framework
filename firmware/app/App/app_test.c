#include "app_test.h"

#include "platform_time.h"
#include "platform_uart.h"
#include "platform_qspi.h"
#include "board_log.h"

#include "command_manager.h"
#include "event_manager.h"
#include "protocol_manager.h"
#include "stream_manager.h"
#include "block_manager.h"
#include "data_router.h"

#include "mcu_info_app.h"
#include "imu_app.h"
#include "storage_app.h"
#include <string.h>

static void AppTestStreamBlock_RunStreamSample(void);
static void AppTestStreamBlock_RunBlockTransfer(void);
static void AppTestStreamBlock_PrintStats(void);

void AppTestStreamBlock_Init(void)
{
    PlatformTime_Init();
    PlatformUart_Init();
		PlatformQspi_Init();
    BoardLog_Init();

    BoardLog_PrintBootBanner();

    if (PlatformUart_StartRxDma() == PLATFORM_UART_OK)
    {
        BoardLog_Info("[APP_TEST_STREAM_BLOCK] UART RX DMA started\r\n");
    }
    else
    {
        BoardLog_Error("[APP_TEST_STREAM_BLOCK] UART RX DMA start failed\r\n");
        return;
    }

    CommandManager_Init();
		CommandService_Init();
    EventManager_Init();
    ProtocolManager_Init();
    StreamManager_Init();
    BlockManager_Init();
    DataRouter_Init();
    McuInfoApp_Init();
	  ImuApp_Init();
    StorageApp_Init();
    BoardLog_Info("[APP_TEST_STREAM_BLOCK] init OK\r\n");
}

void AppTestStreamBlock_Run(void)
{
#if ENABLE_APP_TEST_STREAM_BLOCK
    ProtocolManager_Process();

#if ENABLE_APP_TEST_STREAM_SAMPLE
    AppTestStreamBlock_RunStreamSample();
#endif

#if ENABLE_APP_TEST_BLOCK_TRANSFER
    AppTestStreamBlock_RunBlockTransfer();
#endif

    ProtocolManager_Process();

#if ENABLE_APP_TEST_STREAM_BLOCK_STATS
    AppTestStreamBlock_PrintStats();
#endif
		
#endif
		ProtocolManager_Process();
		McuInfoApp_Run();
		ImuApp_Run();
    StorageApp_Process();
}

static void AppTestStreamBlock_RunStreamSample(void)
{
    static uint32_t last_ms = 0U;
    static uint16_t counter = 0U;
    uint32_t now;
    uint8_t sample[12];

    now = PlatformTime_GetMs();

    if ((now - last_ms) < APP_TEST_STREAM_PERIOD_MS)
    {
        return;
    }

    last_ms = now;

    sample[0] = (uint8_t)(counter & 0xFFU);
    sample[1] = (uint8_t)((counter >> 8) & 0xFFU);
    sample[2] = (uint8_t)(now & 0xFFU);
    sample[3] = (uint8_t)((now >> 8) & 0xFFU);
    sample[4] = (uint8_t)((now >> 16) & 0xFFU);
    sample[5] = (uint8_t)((now >> 24) & 0xFFU);

    sample[6] = 0x11U;
    sample[7] = 0x22U;
    sample[8] = 0x33U;
    sample[9] = 0x44U;
    sample[10] = 0x55U;
    sample[11] = 0x66U;

    (void)StreamManager_SendSample(STREAM_MANAGER_CHANNEL_IMU,
                                   sample,
                                   (uint16_t)sizeof(sample),
                                   PROTOCOL_TX_PRIORITY_LOW);

    counter++;
}

static void AppTestStreamBlock_RunBlockTransfer(void)
{
    static uint8_t state = 0U;
    static uint32_t last_ms = 0U;
    static uint32_t offset = 0U;
    static uint8_t chunk[32];
    uint32_t now;
    uint8_t flags;
    uint16_t i;

    now = PlatformTime_GetMs();

    if ((now - last_ms) < APP_TEST_BLOCK_PERIOD_MS)
    {
        return;
    }

    last_ms = now;

    switch (state)
    {
        case 0U:
            (void)BlockManager_SendBegin(1U,
                                         96U,
                                         0U,
                                         PROTOCOL_TX_PRIORITY_NORMAL);
            offset = 0U;
            state = 1U;
            break;

        case 1U:
            for (i = 0U; i < sizeof(chunk); i++)
            {
                chunk[i] = (uint8_t)(offset + i);
            }

            flags = BLOCK_MANAGER_FLAG_NEED_ACK;
            if (offset + sizeof(chunk) < 96U)
            {
                flags |= BLOCK_MANAGER_FLAG_MORE;
            }

            (void)BlockManager_SendChunk(1U,
                                         offset,
                                         chunk,
                                         (uint16_t)sizeof(chunk),
                                         flags,
                                         PROTOCOL_TX_PRIORITY_NORMAL);

            offset += (uint32_t)sizeof(chunk);

            if (offset >= 96U)
            {
                state = 2U;
            }
            break;

        case 2U:
            (void)BlockManager_SendEnd(1U,
                                       0U,
                                       PROTOCOL_TX_PRIORITY_NORMAL);
            state = 3U;
            break;

        default:
            break;
    }
}

static void AppTestStreamBlock_PrintStats(void)
{
    static uint32_t last_ms = 0U;
    uint32_t now;

    now = PlatformTime_GetMs();

    if ((now - last_ms) < APP_TEST_STATS_PERIOD_MS)
    {
        return;
    }

    last_ms = now;

    StreamManager_PrintStats();
    BlockManager_PrintStats();
    ProtocolManager_PrintStats();
    PlatformUart_PrintStats();
}
