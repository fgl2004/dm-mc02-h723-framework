#ifndef COMMAND_SERVICE_H
#define COMMAND_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "protocol_frame.h"
#include "command_manager.h"

#ifndef COMMAND_SERVICE_MAX_COMMANDS
#define COMMAND_SERVICE_MAX_COMMANDS              64U
#endif

#define COMMAND_SERVICE_NAME_MAX_LEN              24U

/*
 * Command ID layout plan.
 *
 * Current Stage 2 commands are 0x01 ~ 0x0C.
 * Future domains are reserved here so the command table has one stable place
 * to describe the whole PC <-> MCU command namespace.
 */
typedef enum
{
    CMD_DOMAIN_SYSTEM_START      = 0x01U,
    CMD_DOMAIN_SYSTEM_END        = 0x1FU,

    CMD_DOMAIN_DIAG_START        = 0x20U,
    CMD_DOMAIN_DIAG_END          = 0x2FU,

    CMD_DOMAIN_IMU_START         = 0x30U,
    CMD_DOMAIN_IMU_END           = 0x3FU,

    CMD_DOMAIN_CAN_START         = 0x40U,
    CMD_DOMAIN_CAN_END           = 0x4FU,

    CMD_DOMAIN_PARAM_START       = 0x50U,
    CMD_DOMAIN_PARAM_END         = 0x5FU,

    CMD_DOMAIN_BOOT_START        = 0x60U,
    CMD_DOMAIN_BOOT_END          = 0x6FU,

    CMD_DOMAIN_SECURITY_START    = 0x70U,
    CMD_DOMAIN_SECURITY_END      = 0x7FU,

    CMD_DOMAIN_POWER_START       = 0x80U,
    CMD_DOMAIN_POWER_END         = 0x8FU,

    CMD_DOMAIN_CHAOS_START       = 0x90U,
    CMD_DOMAIN_CHAOS_END         = 0x9FU,

    CMD_DOMAIN_STORAGE_START     = 0xA0U,
    CMD_DOMAIN_STORAGE_END       = 0xAFU,

    CMD_DOMAIN_DEBUG_START       = 0xF0U,
    CMD_DOMAIN_DEBUG_END         = 0xFFU
} CommandDomainRange_t;

typedef enum
{
    CMD_CATEGORY_SYSTEM      = 0x01U,
    CMD_CATEGORY_DIAG        = 0x02U,
    CMD_CATEGORY_IMU         = 0x03U,
    CMD_CATEGORY_CAN         = 0x04U,
    CMD_CATEGORY_PARAM       = 0x05U,
    CMD_CATEGORY_BOOT        = 0x06U,
    CMD_CATEGORY_SECURITY    = 0x07U,
    CMD_CATEGORY_POWER       = 0x08U,
    CMD_CATEGORY_CHAOS       = 0x09U,
    CMD_CATEGORY_STORAGE     = 0x0AU,
    CMD_CATEGORY_DEBUG       = 0x0FU
} CommandCategory_t;

typedef enum
{
    CMD_FLAG_NONE            = 0x00000000UL,
    CMD_FLAG_READ_ONLY       = 0x00000001UL,
    CMD_FLAG_WRITE           = 0x00000002UL,
    CMD_FLAG_DANGEROUS       = 0x00000004UL,
    CMD_FLAG_AUTH_REQUIRED   = 0x00000008UL,
    CMD_FLAG_STREAM_CONTROL  = 0x00000010UL,
    CMD_FLAG_BULK_TRANSFER   = 0x00000020UL
} CommandFlag_t;

/* Stage 2 / System information commands. */
typedef enum
{
    SYS_CMD_PING              = 0x01U,
    SYS_CMD_GET_VERSION       = 0x02U,
    SYS_CMD_GET_STATUS        = 0x03U,
    SYS_CMD_GET_RESET_INFO    = 0x04U,
    SYS_CMD_GET_TIME_INFO     = 0x05U,
    SYS_CMD_GET_FAULT_INFO    = 0x06U,
    SYS_CMD_GET_UART_STATS    = 0x07U,
    SYS_CMD_GET_APP_STATS     = 0x08U,
    SYS_CMD_GET_EVENT_STATS   = 0x09U,
    SYS_CMD_GET_BUILD_INFO    = 0x0AU,
    SYS_CMD_GET_CLOCK_INFO    = 0x0BU,
    SYS_CMD_GET_RUNTIME_INFO  = 0x0CU,
	  CMD_GET_COMMAND_STATS     = 0x0DU
} SystemCommandId_t;

/* Stage 3. */
typedef enum
{
    DIAG_CMD_GET_HEALTH          = 0x20U,
    DIAG_CMD_GET_ERROR_COUNTERS  = 0x21U,
    DIAG_CMD_GET_BUFFER_STATS    = 0x22U,
    DIAG_CMD_GET_TIMING_STATS    = 0x23U,
    DIAG_CMD_GET_LAST_RECORDS    = 0x24U,
    DIAG_CMD_GET_PIPELINE_STATS  = 0x25U,
    DIAG_CMD_CLEAR_COUNTERS      = 0x26U,
    DIAG_CMD_GET_TRACE_STATUS    = 0x27U,
    DIAG_CMD_DUMP_TRACE          = 0x28U,
    DIAG_CMD_GET_TASK_HEALTH     = 0x29U
} DiagnosticCommandId_t;
/* Stage 4. */
typedef enum
{
    IMU_CMD_GET_RAW              = 0x30U,
    IMU_CMD_GET_FILTERED         = 0x31U,
    IMU_CMD_GET_STATUS           = 0x32U,
    IMU_CMD_GET_BIAS             = 0x33U,
    IMU_CMD_START_CALIBRATION    = 0x34U,
    IMU_CMD_STOP_CALIBRATION     = 0x35U,
    IMU_CMD_GET_ALGO_STATS       = 0x36U,
    IMU_CMD_SET_SAMPLE_RATE      = 0x37U,
    IMU_CMD_START_STREAM         = 0x38U,
    IMU_CMD_STOP_STREAM          = 0x39U
} ImuCommandId_t;

/* Stage 5. */
typedef enum
{
    CAN_CMD_GET_STATUS           = 0x40U,
    CAN_CMD_GET_STATS            = 0x41U,
    CAN_CMD_SEND_TEST_FRAME      = 0x42U,
    CAN_CMD_SET_FILTER           = 0x43U,
    CAN_CMD_CLEAR_STATS          = 0x44U,
    CAN_CMD_GET_BUS_STATE        = 0x45U,
    CAN_CMD_RECOVER_BUS          = 0x46U,
    CAN_CMD_START_MONITOR_STREAM = 0x47U,
    CAN_CMD_STOP_MONITOR_STREAM  = 0x48U
} CanCommandId_t;

/* Stage 6. */
typedef enum
{
    PARAM_CMD_GET                = 0x50U,
    PARAM_CMD_SET                = 0x51U,
    PARAM_CMD_SAVE               = 0x52U,
    PARAM_CMD_LOAD               = 0x53U,
    PARAM_CMD_RESTORE_DEFAULT    = 0x54U,
    PARAM_CMD_GET_TABLE_INFO     = 0x55U,
    PARAM_CMD_EXPORT_BEGIN       = 0x56U,
    PARAM_CMD_EXPORT_NEXT        = 0x57U,
    PARAM_CMD_IMPORT_BEGIN       = 0x58U,
    PARAM_CMD_IMPORT_CHUNK       = 0x59U,
    PARAM_CMD_IMPORT_END         = 0x5AU
} ParamCommandId_t;

/* Stage 7. */
typedef enum
{
    BOOT_CMD_GET_INFO            = 0x60U,
    BOOT_CMD_ENTER_BOOTLOADER    = 0x61U,
    BOOT_CMD_UPGRADE_BEGIN       = 0x62U,
    BOOT_CMD_UPGRADE_CHUNK       = 0x63U,
    BOOT_CMD_UPGRADE_END         = 0x64U,
    BOOT_CMD_UPGRADE_ABORT       = 0x65U,
    BOOT_CMD_GET_UPGRADE_STATUS  = 0x66U,
    BOOT_CMD_APP_CONFIRM         = 0x67U
} BootCommandId_t;

/* Stage 8. */
typedef enum
{
    SEC_CMD_GET_STATUS           = 0x70U,
    SEC_CMD_CHALLENGE            = 0x71U,
    SEC_CMD_AUTH                 = 0x72U,
    SEC_CMD_LOGOUT               = 0x73U,
    SEC_CMD_GET_SECURITY_LOG     = 0x74U,
    SEC_CMD_CLEAR_SECURITY_LOG   = 0x75U
} SecurityCommandId_t;

/* Stage 9. */
typedef enum
{
    PWR_CMD_GET_STATUS           = 0x80U,
    PWR_CMD_ENTER_SLEEP          = 0x81U,
    PWR_CMD_ENTER_STOP           = 0x82U,
    PWR_CMD_GET_WAKEUP_REASON    = 0x83U,
    PWR_CMD_GET_CLOCK_STATUS     = 0x84U,
    PWR_CMD_GET_HW_STATUS        = 0x85U
} PowerCommandId_t;

/* Stage 10. */
typedef enum
{
    CHAOS_CMD_GET_STATUS         = 0x90U,
    CHAOS_CMD_TRIGGER_FAULT      = 0x91U,
    CHAOS_CMD_TRIGGER_RESET      = 0x92U,
    CHAOS_CMD_FILL_BUFFER        = 0x93U,
    CHAOS_CMD_CORRUPT_PARAM      = 0x94U,
    CHAOS_CMD_START_STRESS       = 0x95U,
    CHAOS_CMD_STOP_STRESS        = 0x96U,
    CHAOS_CMD_GET_REPORT         = 0x97U
} ChaosCommandId_t;


typedef enum
{
    COMMAND_SERVICE_OK = 0,
    COMMAND_SERVICE_ERROR = -1,
    COMMAND_SERVICE_INVALID_PARAM = -2,
    COMMAND_SERVICE_TABLE_FULL = -3,
    COMMAND_SERVICE_DUPLICATE_CMD = -4,
    COMMAND_SERVICE_UNKNOWN_CMD = -5,
    COMMAND_SERVICE_NOT_INITIALIZED = -6
} CommandServiceResult_t;

typedef int (*CommandServiceHandler_t)(const ProtocolFrame_t *req_frame,
                                       CommandManagerResponse_t *resp,
                                       void *ctx);

typedef struct
{
    uint8_t cmd;
    uint8_t category;
    uint32_t flags;

    CommandServiceHandler_t handler;
    void *ctx;
    const char *name;
} CommandServiceEntry_t;

typedef struct
{
    uint32_t init_count;
    uint32_t register_count;
    uint32_t duplicate_register_count;
    uint32_t dispatch_count;

    uint32_t system_cmd_count;
    uint32_t diag_cmd_count;
    uint32_t imu_cmd_count;
    uint32_t can_cmd_count;
    uint32_t param_cmd_count;
    uint32_t boot_cmd_count;
    uint32_t security_cmd_count;
    uint32_t power_cmd_count;
    uint32_t chaos_cmd_count;
    uint32_t storage_cmd_count;
    uint32_t debug_cmd_count;

    uint32_t unknown_cmd_count;
    uint32_t table_full_count;
    uint32_t invalid_param_count;
    uint32_t handler_error_count;

    uint32_t last_dispatch_us;
    uint32_t max_dispatch_us;

    uint8_t registered_count;
    uint8_t last_cmd;
    uint8_t last_category;
    uint8_t last_error;
} CommandServiceStats_t;

void CommandService_Init(void);

int CommandService_Register(uint8_t cmd,
                            uint8_t category,
                            uint32_t flags,
                            CommandServiceHandler_t handler,
                            void *ctx,
                            const char *name);

int CommandService_Dispatch(const ProtocolFrame_t *req_frame,
                            CommandManagerResponse_t *resp);

const CommandServiceStats_t *CommandService_GetStats(void);
void CommandService_ResetStats(void);
void CommandService_PrintStats(void);

void CommandService_SetResp(CommandManagerResponse_t *resp,
                            uint8_t cmd,
                            const uint8_t *payload,
                            uint16_t payload_len);

void CommandService_SetNack(CommandManagerResponse_t *resp,
                            uint8_t cmd,
                            uint8_t error_code);

uint8_t CommandService_GetCategoryByCmd(uint8_t cmd);
const char *CommandService_GetCategoryName(uint8_t category);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_SERVICE_H */
