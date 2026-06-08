#include "app_main.h"



#include "platform_time.h"
#include "platform_uart.h"
#include "platform_reset.h"
#include "board_log.h"
#include "ring_buffer.h"
#include "crc16.h"
#include "state_machine.h"
#include "uart_rx_consumer.h"
#include "protocol_frame.h"
#include "command_manager.h"
#include "protocol_manager.h"
#include "mcu_info_app.h"
#include "diagnostic_app.h"

#include "platform_spi.h"
#include "bsp_bmi088.h"
#include "imu_app.h"

#include "platform_qspi.h"
#include "bsp_w25q64jv.h"
#include "flash_block_device.h"
#include "storage_partition.h"
#include "storage_manager.h"


#include <stdio.h>

#define ENABLE_SOFTWARE_RESET_TEST   0
#define SOFTWARE_RESET_DELAY_MS      5000U

#define ENABLE_HARDFAULT_TEST        0
#define HARDFAULT_TEST_DELAY_MS      5000U

#define ENABLE_DWT_TEST              0

#define ENABLE_RING_BUFFER_TEST      0

#define ENABLE_CRC16_TEST            0

#define ENABLE_STATE_MACHINE_TEST    0



#define ENABLE_UART_RX_DUMP_TEST       0
#define ENABLE_UART_RX_STATS_REPORT    0
#define UART_RX_STATS_PERIOD_MS        100U

#define ENABLE_UART_RX_SLOW_FAST_TEST   0

#define ENABLE_PROTOCOL_FRAME_TEST     0

#define ENABLE_PROTOCOL_MANAGER_TEST     0

#define ENABLE_MCU_INFO_APP_EVENT_TEST     0
#define MCU_INFO_APP_EVENT_TEST_PERIOD_MS         500U


#define ENABLE_BMI088_BSP_TEST           0
#define BMI088_BSP_TEST_PERIOD_MS        500U


#ifndef ENABLE_W25Q64_TEST
#define ENABLE_W25Q64_TEST          0
#endif

#ifndef W25Q64_TEST_PERIOD_MS
#define W25Q64_TEST_PERIOD_MS        2000U
#endif

#ifndef W25Q64_TEST_ADDR
#define W25Q64_TEST_ADDR             0x7F0000U
#endif

#ifndef ENABLE_FLASH_BLOCK_DEVICE_TEST
#define ENABLE_FLASH_BLOCK_DEVICE_TEST     0
#endif

#ifndef ENABLE_STORAGE_PARTITION_TEST
#define ENABLE_STORAGE_PARTITION_TEST      0
#endif

#ifndef FLASH_BLOCK_DEVICE_TEST_PERIOD_MS
#define FLASH_BLOCK_DEVICE_TEST_PERIOD_MS  3000U
#endif

#ifndef FLASH_BLOCK_DEVICE_TEST_LEN
#define FLASH_BLOCK_DEVICE_TEST_LEN        300U
#endif

#ifndef ENABLE_STORAGE_MANAGER_TEST
#define ENABLE_STORAGE_MANAGER_TEST     0
#endif

#ifndef STORAGE_MANAGER_TEST_PERIOD_MS
#define STORAGE_MANAGER_TEST_PERIOD_MS   4000U
#endif

#ifndef STORAGE_MANAGER_TEST_LEN
#define STORAGE_MANAGER_TEST_LEN         300U
#endif

#define ENABLE_UART1_DMA_TEST   1

static PlatformResetInfo_t g_reset_info;

typedef enum
{
    APP_TEST_STATE_IDLE = 0,
    APP_TEST_STATE_RUNNING,
    APP_TEST_STATE_ERROR
} AppTestState_t;

typedef enum
{
    APP_TEST_EVENT_START = 1,
    APP_TEST_EVENT_STOP,
    APP_TEST_EVENT_ERROR
} AppTestEvent_t;

typedef struct
{
    StateMachine_t sm;
    uint32_t enter_idle_count;
    uint32_t enter_running_count;
    uint32_t enter_error_count;
} AppTestStateMachineCtx_t;

static AppTestStateMachineCtx_t g_sm_test_ctx;

static void AppMain_TestUart1TxDma(void)
{
    static uint8_t done = 0U;
    static uint8_t tx_data[256];

    uint32_t start;
    uint32_t elapsed_us;
    int ret;

    if (done != 0U)
    {
        return;
    }

    done = 1U;

    for (uint16_t i = 0U; i < sizeof(tx_data); i++)
    {
        tx_data[i] = (uint8_t)i;
    }

    /*
     * 注意：
     * BoardLog / printf 目前仍然走 UART1 阻塞发送。
     * 所以必须在 DMA 开始前打印 start，在 DMA 完成后再打印结果。
     */
    BoardLog_Info("[UART1_TX_DMA_TEST] start, len=%u\r\n", (unsigned int)sizeof(tx_data));

    PlatformTime_DelayMs(20U);

    start = PlatformTime_ProfileStart();

    ret = PlatformUart_SendBufferDma(tx_data, (uint16_t)sizeof(tx_data));
    if (ret != PLATFORM_UART_OK)
    {
        BoardLog_Error("[UART1_TX_DMA_TEST] start failed, ret=%d\r\n", ret);
        PlatformUart_PrintStats();
        return;
    }

    while (PlatformUart_IsTxBusy() != 0U)
    {
        PlatformTime_DelayMs(1U);
    }

    elapsed_us = PlatformTime_ProfileEndUs(start);

    BoardLog_Info("[UART1_TX_DMA_TEST] done, elapsed=%lu us\r\n",
                  (unsigned long)elapsed_us);

    PlatformUart_PrintStats();
}

static void App_RunStorageManagerTest(void)
{
#if ENABLE_STORAGE_MANAGER_TEST
    static uint8_t first_run = 1U;
    static uint32_t last_test_ms = 0U;
    static uint32_t test_count = 0U;

    const StoragePartition_t *partition;
    uint32_t now;
    uint32_t test_offset;
    int ret;

    now = PlatformTime_GetMs();

    if ((first_run == 0U) && ((now - last_test_ms) < STORAGE_MANAGER_TEST_PERIOD_MS))
    {
        return;
    }

    first_run = 0U;
    last_test_ms = now;
    test_count++;

    partition = StoragePartition_Get(STORAGE_PARTITION_FACTORY_RESERVED);
    if (partition == 0)
    {
        BoardLog_Error("[STORAGE_MANAGER_TEST] factory partition missing\r\n");
        return;
    }

    /*
     * Test the last sector inside factory_reserved partition.
     */
    test_offset = partition->size_bytes - partition->erase_size;

    if (test_count == 1U)
    {
        ret = StorageManager_TestPartition(STORAGE_PARTITION_FACTORY_RESERVED,
                                           test_offset,
                                           STORAGE_MANAGER_TEST_LEN);
        if (ret == STORAGE_MANAGER_OK)
        {
            BoardLog_Info("[STORAGE_MANAGER_TEST] first test PASS\r\n");
        }
        else
        {
            BoardLog_Error("[STORAGE_MANAGER_TEST] first test FAIL, ret=%d\r\n", ret);
        }

        StorageManager_PrintInfo();
        StorageManager_PrintStats();
        FlashBlockDevice_PrintStats();
    }
    else
    {
        const StorageManagerStats_t *stats = StorageManager_GetStats();

        BoardLog_Info("[STORAGE_MANAGER_TEST] #%lu last_error=%d read=%lu write=%lu erase=%lu test=%lu\r\n",
                      (unsigned long)test_count,
                      stats->last_error,
                      (unsigned long)stats->read_count,
                      (unsigned long)stats->write_count,
                      (unsigned long)stats->erase_count,
                      (unsigned long)stats->test_count);
    }
#endif
}
static void App_RunStoragePartitionTest(void)
{
#if ENABLE_STORAGE_PARTITION_TEST
    static uint8_t done = 0U;
    uint32_t i;

    if (done != 0U)
    {
        return;
    }

    done = 1U;

    StoragePartition_PrintTable();

    for (i = 0U; i < StoragePartition_GetCount(); i++)
    {
        const StoragePartition_t *p = StoragePartition_Get((StoragePartitionId_t)i);

        if (StoragePartition_IsValid(p) == 0)
        {
            BoardLog_Error("[STORAGE_PARTITION_TEST] partition %lu invalid\r\n",
                           (unsigned long)i);
        }
        else
        {
            BoardLog_Info("[STORAGE_PARTITION_TEST] partition %lu OK: %s start=0x%06lX size=%lu\r\n",
                          (unsigned long)i,
                          p->name,
                          (unsigned long)p->start_addr,
                          (unsigned long)p->size_bytes);
        }
    }
#endif
}

static void App_RunFlashBlockDeviceTest(void)
{
#if ENABLE_FLASH_BLOCK_DEVICE_TEST
    static uint8_t first_run = 1U;
    static uint32_t last_test_ms = 0U;
    static uint32_t test_count = 0U;

    const StoragePartition_t *factory_partition;
    uint32_t now;
    uint32_t test_addr;
    int ret;

    now = PlatformTime_GetMs();

    if ((first_run == 0U) && ((now - last_test_ms) < FLASH_BLOCK_DEVICE_TEST_PERIOD_MS))
    {
        return;
    }

    first_run = 0U;
    last_test_ms = now;
    test_count++;

    factory_partition = StoragePartition_Get(STORAGE_PARTITION_FACTORY_RESERVED);
    if (factory_partition == 0)
    {
        BoardLog_Error("[FLASH_BLOCK_TEST] factory partition missing\r\n");
        return;
    }

    /*
     * Use the last 4KB sector in factory/reserved partition as temporary test sector.
     * Do not keep this enabled after real factory data is introduced.
     */
    test_addr = factory_partition->start_addr + factory_partition->size_bytes - factory_partition->erase_size;

    if (test_count == 1U)
    {
        ret = FlashBlockDevice_WriteReadTest(test_addr, FLASH_BLOCK_DEVICE_TEST_LEN);
        if (ret == FLASH_BLOCK_DEVICE_OK)
        {
            BoardLog_Info("[FLASH_BLOCK_TEST] write/read PASS addr=0x%06lX len=%lu\r\n",
                          (unsigned long)test_addr,
                          (unsigned long)FLASH_BLOCK_DEVICE_TEST_LEN);
        }
        else
        {
            BoardLog_Error("[FLASH_BLOCK_TEST] write/read FAIL ret=%d addr=0x%06lX len=%lu\r\n",
                           ret,
                           (unsigned long)test_addr,
                           (unsigned long)FLASH_BLOCK_DEVICE_TEST_LEN);
        }

        FlashBlockDevice_PrintStats();
    }
    else
    {
        const FlashBlockDeviceStats_t *stats = FlashBlockDevice_GetStats();
        const FlashBlockDeviceInfo_t *info = FlashBlockDevice_GetInfo();

        BoardLog_Info("[FLASH_BLOCK_TEST] #%lu capacity=%lu erase=%lu program=%lu last_error=%d read=%lu program=%lu erase=%lu\r\n",
                      (unsigned long)test_count,
                      (unsigned long)info->capacity_bytes,
                      (unsigned long)info->erase_size,
                      (unsigned long)info->program_size,
                      stats->last_error,
                      (unsigned long)stats->read_count,
                      (unsigned long)stats->program_count,
                      (unsigned long)stats->erase_count);
    }
#endif
}

static void App_RunW25q64Test(void)
{
//#if ENABLE_W25Q64_TEST
    static uint8_t first_run = 1U;
    static uint32_t last_test_ms = 0U;
    static uint32_t test_count = 0U;

    uint32_t now;
    BspW25q64JedecId_t jedec;
    BspW25q64UniqueId_t uid;
    uint8_t sr1 = 0U;
    uint8_t sr2 = 0U;
    uint8_t sr3 = 0U;
    int ret;

    now = PlatformTime_GetMs();

    if ((first_run == 0U) && ((now - last_test_ms) < W25Q64_TEST_PERIOD_MS))
    {
        return;
    }

    first_run = 0U;
    last_test_ms = now;
    test_count++;

    ret = BspW25q64_ReadJedecId(&jedec);
    if (ret != BSP_W25Q64_OK)
    {
        BoardLog_Error("[W25Q64_TEST] #%lu ReadJedecId failed, ret=%d\r\n",
                       (unsigned long)test_count,
                       ret);
        return;
    }

    ret = BspW25q64_ReadUniqueId(&uid);
    if (ret != BSP_W25Q64_OK)
    {
        BoardLog_Error("[W25Q64_TEST] #%lu ReadUniqueId failed, ret=%d\r\n",
                       (unsigned long)test_count,
                       ret);
        return;
    }

    (void)BspW25q64_ReadStatus1(&sr1);
    (void)BspW25q64_ReadStatus2(&sr2);
    (void)BspW25q64_ReadStatus3(&sr3);

    BoardLog_Info("[W25Q64_TEST] #%lu JEDEC=%02X %02X %02X UID=%02X%02X%02X%02X%02X%02X%02X%02X SR=%02X/%02X/%02X\r\n",
                  (unsigned long)test_count,
                  jedec.manufacturer_id,
                  jedec.memory_type,
                  jedec.capacity_id,
                  uid.bytes[0],
                  uid.bytes[1],
                  uid.bytes[2],
                  uid.bytes[3],
                  uid.bytes[4],
                  uid.bytes[5],
                  uid.bytes[6],
                  uid.bytes[7],
                  sr1,
                  sr2,
                  sr3);

    /*
     * Only run erase/program/readback once to avoid unnecessary flash wear.
     */
    if (test_count == 1U)
    {
        ret = BspW25q64_WriteReadTest(W25Q64_TEST_ADDR);
        if (ret == BSP_W25Q64_OK)
        {
            BoardLog_Info("[W25Q64_TEST] write/read test PASS, addr=0x%06lX\r\n",
                          (unsigned long)W25Q64_TEST_ADDR);
        }
        else
        {
            BoardLog_Error("[W25Q64_TEST] write/read test FAIL, ret=%d, addr=0x%06lX\r\n",
                           ret,
                           (unsigned long)W25Q64_TEST_ADDR);
        }

        BspW25q64_PrintStats();
        PlatformQspi_PrintStats();
    }
//#endif
}
static void App_RunBmi088BspTest(void)
{
#if ENABLE_BMI088_BSP_TEST
    static uint32_t last_test_ms = 0U;
    static uint32_t test_count = 0U;

    uint32_t now;
    BspBmi088ChipId_t chip_id;
    BspBmi088RawData_t raw;
    int ret;

    now = PlatformTime_GetMs();

    if ((now - last_test_ms) < BMI088_BSP_TEST_PERIOD_MS)
    {
        return;
    }

    last_test_ms = now;
    test_count++;

    ret = BspBmi088_ReadChipId(&chip_id);
    if (ret != BSP_BMI088_OK)
    {
        BoardLog_Error("[BMI088_TEST] #%lu ReadChipId failed, ret=%d\r\n",
                       (unsigned long)test_count,
                       ret);
        return;
    }

    ret = BspBmi088_ReadRaw(&raw);
    if (ret != BSP_BMI088_OK)
    {
        BoardLog_Error("[BMI088_TEST] #%lu ReadRaw failed, ret=%d, acc_id=0x%02X, gyro_id=0x%02X\r\n",
                       (unsigned long)test_count,
                       ret,
                       chip_id.acc_id,
                       chip_id.gyro_id);
        return;
    }

    BoardLog_Info("[BMI088_TEST] #%lu acc_id=0x%02X gyro_id=0x%02X "
                  "ax=%d ay=%d az=%d gx=%d gy=%d gz=%d temp=%d tick=%lu\r\n",
                  (unsigned long)test_count,
                  chip_id.acc_id,
                  chip_id.gyro_id,
                  raw.ax,
                  raw.ay,
                  raw.az,
                  raw.gx,
                  raw.gy,
                  raw.gz,
                  raw.temp,
                  (unsigned long)raw.tick_ms);
#endif
}

static void App_RunMcuInfoEventTest(void)
{
//#if ENABLE_MCU_INFO_APP_EVENT_TEST
    static uint32_t last_post_ms = 0U;
    static uint32_t event_counter = 0U;

    uint32_t now;
    char payload[32];
    int len;

    now = PlatformTime_GetMs();

    if ((now - last_post_ms) < MCU_INFO_APP_EVENT_TEST_PERIOD_MS)
    {
        return;
    }

    last_post_ms = now;
    event_counter++;

    len = snprintf(payload,
                   sizeof(payload),
                   "EVT,%lu,%lu",
                   (unsigned long)event_counter,
                   (unsigned long)now);

    if (len < 0)
    {
        return;
    }

    if (len >= (int)sizeof(payload))
    {
        len = (int)(sizeof(payload) - 1);
        payload[len] = '\0';
    }

    (void)McuInfoApp_PostEvent(MCU_INFO_APP_ID_USER,
                               MCU_INFO_EVENT_APP_MESSAGE,
                               (const uint8_t *)payload,
                               (uint16_t)len);
//#endif
}
static void App_TestProtocolFrame(void)
{
    int ret;

    BoardLog_PrintSeparator();
    BoardLog_Info("ProtocolFrame Test Start\r\n");

    ret = ProtocolFrame_RunSelfTest();

    if (ret == 0)
    {
        BoardLog_Info("ProtocolFrame self test = PASS\r\n");
    }
    else
    {
        BoardLog_Error("ProtocolFrame self test = FAIL, ret=%d\r\n", ret);
    }

    BoardLog_Info("ProtocolFrame Test End\r\n");
}

static void App_ProcessUartRxTest(void)
{
    uint8_t buf[128];
    uint16_t len;

    /*
     * This function simulates the future ProtocolManager consuming
     * bytes from UART RX RingBuffer.
     *
     * In high-speed stress test, do not print every received byte,
     * otherwise UART TX printf will heavily disturb UART RX measurement.
     */
    len = PlatformUart_ReadRx(buf, sizeof(buf));

#if ENABLE_UART_RX_DUMP_TEST
    if (len > 0U)
    {
        BoardLog_Info("UART RX: len=%u, data=", len);

        for (uint16_t i = 0U; i < len; i++)
        {
            printf("%02X ", buf[i]);
        }

        printf("\r\n");
    }
#else
    (void)len;
#endif
}

static void App_ReportUartRxStatsPeriodically(void)
{
//#if ENABLE_UART_RX_STATS_REPORT
    static uint32_t last_report_ms = 0U;
    uint32_t now;
    PlatformUartRxSnapshot_t s;
    const UartRxConsumerStats_t *c;

    now = PlatformTime_GetMs();

    if ((now - last_report_ms) < UART_RX_STATS_PERIOD_MS)
    {
        return;
    }

    last_report_ms = now;

    PlatformUart_GetRxSnapshot(&s);
    c = UartRxConsumer_GetStats();

    printf("@UARTSTAT,t=%lu,rx_bytes=%lu,avail=%u,free=%u,rb_write=%lu,rb_read=%lu,overflow=%lu,rb_overflow=%lu,high=%u,half=%lu,full=%lu,idle=%lu,err=%lu,consumer_mode=%lu,consumer_read=%lu,consumer_mismatch=%lu,consumer_drop=%lu,consumer_expected=%u,consumer_last_actual=%u,consumer_last_expected=%u\r\n",
           now,
           s.rx_bytes,
           s.rx_ring_available,
           s.rx_ring_free,
           s.rb_write_bytes,
           s.rb_read_bytes,
           s.rx_ring_overflow,
           s.rb_overflow_count,
           s.rb_high_watermark,
           s.rx_half_count,
           s.rx_full_count,
           s.rx_idle_count,
           s.rx_error_count,
           c->mode,
           c->read_bytes,
           c->mismatch_count,
           c->estimated_drop_bytes,
           c->expected_counter,
           c->last_actual,
           c->last_expected);
//#endif
}

static void AppTest_OnEnterIdle(void *ctx)
{
    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx != NULL)
    {
        test_ctx->enter_idle_count++;
    }

    BoardLog_Info("StateMachine: enter IDLE\r\n");
}

static void AppTest_OnEnterRunning(void *ctx)
{
    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx != NULL)
    {
        test_ctx->enter_running_count++;
    }

    BoardLog_Info("StateMachine: enter RUNNING\r\n");
}

static void AppTest_OnEnterError(void *ctx)
{
    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx != NULL)
    {
        test_ctx->enter_error_count++;
    }

    BoardLog_Info("StateMachine: enter ERROR\r\n");
}

static int AppTest_IdleOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    (void)event_data;

    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == APP_TEST_EVENT_START)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_RUNNING,
                                       PlatformTime_GetMs());
    }

    if (event == APP_TEST_EVENT_ERROR)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int AppTest_RunningOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    (void)event_data;

    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == APP_TEST_EVENT_STOP)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_IDLE,
                                       PlatformTime_GetMs());
    }

    if (event == APP_TEST_EVENT_ERROR)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_ERROR,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static int AppTest_ErrorOnEvent(void *ctx, EventId_t event, const void *event_data)
{
    (void)event_data;

    AppTestStateMachineCtx_t *test_ctx = (AppTestStateMachineCtx_t *)ctx;

    if (test_ctx == NULL)
    {
        return STATE_MACHINE_INVALID_PARAM;
    }

    if (event == APP_TEST_EVENT_STOP)
    {
        return StateMachine_Transition(&test_ctx->sm,
                                       APP_TEST_STATE_IDLE,
                                       PlatformTime_GetMs());
    }

    return STATE_MACHINE_OK;
}

static const StateDef_t g_app_test_state_table[] =
{
    {
        APP_TEST_STATE_IDLE,
        AppTest_OnEnterIdle,
        NULL,
        AppTest_IdleOnEvent
    },
    {
        APP_TEST_STATE_RUNNING,
        AppTest_OnEnterRunning,
        NULL,
        AppTest_RunningOnEvent
    },
    {
        APP_TEST_STATE_ERROR,
        AppTest_OnEnterError,
        NULL,
        AppTest_ErrorOnEvent
    }
};

static void App_TestStateMachine(void)
{
    int ret;

    BoardLog_PrintSeparator();
    BoardLog_Info("StateMachine Test Start\r\n");

    ret = StateMachine_Init(&g_sm_test_ctx.sm,
                            g_app_test_state_table,
                            (uint16_t)(sizeof(g_app_test_state_table) / sizeof(g_app_test_state_table[0])),
                            APP_TEST_STATE_IDLE,
                            &g_sm_test_ctx,
                            PlatformTime_GetMs());

    BoardLog_Info("StateMachine init ret=%d, state=%u\r\n",
                  ret,
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    StateMachine_Dispatch(&g_sm_test_ctx.sm, APP_TEST_EVENT_START, NULL);
    BoardLog_Info("After START: state=%u\r\n",
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    StateMachine_Dispatch(&g_sm_test_ctx.sm, APP_TEST_EVENT_ERROR, NULL);
    BoardLog_Info("After ERROR: state=%u\r\n",
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    StateMachine_Dispatch(&g_sm_test_ctx.sm, APP_TEST_EVENT_STOP, NULL);
    BoardLog_Info("After STOP: state=%u\r\n",
                  StateMachine_GetState(&g_sm_test_ctx.sm));

    BoardLog_Info("transition_count=%lu, dispatch_count=%lu, error_count=%lu\r\n",
                  StateMachine_GetTransitionCount(&g_sm_test_ctx.sm),
                  StateMachine_GetDispatchCount(&g_sm_test_ctx.sm),
                  StateMachine_GetErrorCount(&g_sm_test_ctx.sm));

    BoardLog_Info("enter_idle=%lu, enter_running=%lu, enter_error=%lu\r\n",
                  g_sm_test_ctx.enter_idle_count,
                  g_sm_test_ctx.enter_running_count,
                  g_sm_test_ctx.enter_error_count);

    BoardLog_Info("StateMachine Test End\r\n");
}

static void App_TestCrc16(void)
{
    uint8_t passed;
    uint16_t crc;

    static const uint8_t test_data[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

    BoardLog_PrintSeparator();
    BoardLog_Info("CRC16 Test Start\r\n");

    crc = Crc16_CcittFalse(test_data, (uint16_t)sizeof(test_data));
    passed = Crc16_SelfTest();

    BoardLog_Info("CRC16-CCITT-FALSE test data: 123456789\r\n");
    BoardLog_Info("CRC16 result = 0x%04X\r\n", crc);
    BoardLog_Info("Expected     = 0x29B1\r\n");
    BoardLog_Info("Self test    = %s\r\n", passed ? "PASS" : "FAIL");

    BoardLog_Info("CRC16 Test End\r\n");
}
static void App_TestRingBuffer(void)
{
    static uint8_t rb_mem[8];
    RingBuffer_t rb;

    uint8_t input1[] = {1, 2, 3, 4, 5};
    uint8_t input2[] = {6, 7, 8, 9};
    uint8_t out[8];
    uint16_t written;
    uint16_t read_len;
    const RingBufferStats_t *stats;

    RingBuffer_Init(&rb, rb_mem, sizeof(rb_mem));

    BoardLog_PrintSeparator();
    BoardLog_Info("RingBuffer Test Start\r\n");

    written = RingBuffer_Write(&rb, input1, sizeof(input1));
    BoardLog_Info("Write input1: written=%u, available=%u, free=%u\r\n",
                  written,
                  RingBuffer_Available(&rb),
                  RingBuffer_Free(&rb));

    read_len = RingBuffer_Read(&rb, out, 3U);
    BoardLog_Info("Read 3 bytes: read=%u, data=%u %u %u, available=%u, free=%u\r\n",
                  read_len,
                  out[0],
                  out[1],
                  out[2],
                  RingBuffer_Available(&rb),
                  RingBuffer_Free(&rb));

    written = RingBuffer_Write(&rb, input2, sizeof(input2));
    BoardLog_Info("Write input2: written=%u, available=%u, free=%u\r\n",
                  written,
                  RingBuffer_Available(&rb),
                  RingBuffer_Free(&rb));

    read_len = RingBuffer_Read(&rb, out, sizeof(out));
    BoardLog_Info("Read all: read=%u\r\n", read_len);

    for (uint16_t i = 0U; i < read_len; i++)
    {
        BoardLog_Info("  out[%u]=%u\r\n", i, out[i]);
    }

    written = RingBuffer_Write(&rb, input1, sizeof(input1));
    written += RingBuffer_Write(&rb, input2, sizeof(input2));

    stats = RingBuffer_GetStats(&rb);

    BoardLog_Info("Overflow test: available=%u, overflow=%lu, high_watermark=%u\r\n",
                  RingBuffer_Available(&rb),
                  stats->overflow_count,
                  stats->high_watermark);

    BoardLog_Info("RingBuffer Test End\r\n");
}

static void App_PrintClockInfo(void)
{
    printf("----------------------------------------\r\n");
    printf(" Clock Info:\r\n");
    printf("  SYSCLK = %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("  HCLK   = %lu Hz\r\n", HAL_RCC_GetHCLKFreq());
    printf("  PCLK1  = %lu Hz\r\n", HAL_RCC_GetPCLK1Freq());
    printf("  PCLK2  = %lu Hz\r\n", HAL_RCC_GetPCLK2Freq());
}

static void App_PrintTickTest(void)
{
    printf("----------------------------------------\r\n");
    printf(" Tick Test:\r\n");
    printf("  HAL_GetTick = %lu ms\r\n", PlatformTime_GetMs());
}

static void App_RunSoftwareResetTest(void)
{
//#if ENABLE_SOFTWARE_RESET_TEST
    if ((PlatformReset_IsSoftwareReset(&g_reset_info) == 0U) &&
        (PlatformTime_GetMs() > SOFTWARE_RESET_DELAY_MS))
    {
        BoardLog_Info("[RESET_TEST] Trigger software reset by NVIC_SystemReset()\r\n");
        PlatformTime_DelayMs(100U);
        PlatformReset_SoftwareReset();
    }
//#endif
}

static void App_RunHardFaultTest(void)
{
//#if ENABLE_HARDFAULT_TEST
    if (PlatformTime_GetMs() > HARDFAULT_TEST_DELAY_MS)
    {
        BoardLog_Info("[FAULT_TEST] Trigger HardFault test\r\n");
        PlatformTime_DelayMs(100U);

        volatile uint32_t *bad_addr = (uint32_t *)0xFFFFFFFFU;
        *bad_addr = 0x12345678U;
    }
//#endif
}
static void App_PrintUptimePeriodically(void)
{
    static uint32_t last_log_ms = 0U;
    uint32_t now;

    now = PlatformTime_GetMs();

    if ((now - last_log_ms) >= 1000U)
    {
        last_log_ms = now;
        BoardLog_Info("[RUN] uptime = %lu ms\r\n", now);
    }
}
void App_Init(void)
{
    PlatformUart_Init();
    PlatformTime_Init();
    BoardLog_Init();

#if ENABLE_BMI088_BSP_TEST
    PlatformSpi_Init();

    if (BspBmi088_Init() == BSP_BMI088_OK)
    {
        BoardLog_Info("BMI088 BSP init OK\r\n");
    }
    else
    {
        BoardLog_Error("BMI088 BSP init failed\r\n");
    }
#endif

    BoardLog_PrintBootBanner();
    if (PlatformUart_StartRxDma() == PLATFORM_UART_OK)
    {
        BoardLog_Info("UART RX DMA started\r\n");
    }
    else
    {
        BoardLog_Error("UART RX DMA start failed\r\n");
    }

    
    PlatformReset_Capture(&g_reset_info);
    PlatformReset_PrintInfo(&g_reset_info);
    PlatformReset_ClearFlags();
#if ENABLE_W25Q64_TEST
    PlatformQspi_Init();

    if (BspW25q64_Init() == BSP_W25Q64_OK)
    {
        BoardLog_Info("W25Q64 BSP init OK\r\n");
    }
    else
    {
        BoardLog_Error("W25Q64 BSP init failed\r\n");
    }
#endif
#if ENABLE_FLASH_BLOCK_DEVICE_TEST
		PlatformQspi_Init();
    if (FlashBlockDevice_Init() == FLASH_BLOCK_DEVICE_OK)
    {
        BoardLog_Info("FlashBlockDevice init OK\r\n");
    }
    else
    {
        BoardLog_Error("FlashBlockDevice init failed\r\n");
    }
#endif

#if ENABLE_STORAGE_MANAGER_TEST
		PlatformQspi_Init();
    if (StorageManager_Init() == STORAGE_MANAGER_OK)
    {
        BoardLog_Info("StorageManager init OK\r\n");
    }
    else
    {
        BoardLog_Error("StorageManager init failed\r\n");
    }
#endif

#if ENABLE_UART_RX_SLOW_FAST_TEST
    UartRxConsumer_Init();
#endif
    App_PrintClockInfo();
    App_PrintTickTest();

#if ENABLE_DWT_TEST
    PlatformTime_PrintStatus();
    PlatformTime_RunDwtTest();
#endif
	
#if ENABLE_RING_BUFFER_TEST
    App_TestRingBuffer();
#endif

#if ENABLE_CRC16_TEST
    App_TestCrc16();
#endif

#if ENABLE_STATE_MACHINE_TEST
    App_TestStateMachine();
#endif
#if ENABLE_PROTOCOL_FRAME_TEST
    App_TestProtocolFrame();
#endif
#if ENABLE_PROTOCOL_MANAGER_TEST
    CommandService_Init();

    McuInfoApp_Init();
    (void)McuInfoApp_UpdateResetSnapshot(&g_reset_info);

    DiagnosticApp_Init();

    CommandManager_Init();
    ProtocolManager_Init();
		ImuApp_Init();
#endif
		
    printf("========================================\r\n");
}

void App_Run(void)
{
#if ENABLE_UART_RX_SLOW_FAST_TEST
        UartRxConsumer_Run();
#endif 

#if ENABLE_MCU_INFO_APP_EVENT_TEST
		App_RunMcuInfoEventTest();
#endif 

#if ENABLE_PROTOCOL_MANAGER_TEST
				ImuApp_Run();
        DiagnosticApp_Run();
        McuInfoApp_Run();
        ProtocolManager_Process();
#endif

#if ENABLE_BMI088_BSP_TEST
        App_RunBmi088BspTest();
#endif    

#if ENABLE_UART_RX_STATS_REPORT
        App_ReportUartRxStatsPeriodically();
#endif 

#if ENABLE_SOFTWARE_RESET_TEST
        App_RunSoftwareResetTest();
#endif

#if ENABLE_HARDFAULT_TEST
        App_RunHardFaultTest();
#endif
#if ENABLE_W25Q64_TEST
        //App_RunW25q64Test();
#endif

#if ENABLE_STORAGE_PARTITION_TEST
    App_RunStoragePartitionTest();
#endif

#if ENABLE_FLASH_BLOCK_DEVICE_TEST
    App_RunFlashBlockDeviceTest();
#endif
#if ENABLE_STORAGE_MANAGER_TEST
    App_RunStorageManagerTest();
#endif


#if ENABLE_UART1_DMA_TEST
    AppMain_TestUart1TxDma();
#endif
		App_PrintUptimePeriodically();

}